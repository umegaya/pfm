/***************************************************************
 * session.h : template implementation part of sfc::session
 * 2009/12/23 iyatomi : create
 *                             Copyright (C) 2008-2009 Takehiro Iyatomi
 * This file is part of libnbr.
 * libnbr is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either
 * version 2.1 of the License or any later version.
 * libnbr is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU Lesser General Public License for more details.
 * You should have received a copy of
 * the GNU Lesser General Public License along with libnbr;
 * if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 ****************************************************************/

/*-------------------------------------------------------------*/
/* sfc::cluster												   */
/*-------------------------------------------------------------*/
/* cluster_protocol */
template <class S>
int cluster_protocol_impl<S>::recv_handler(S &s, SOCK sk, char *p, int l)
{
	switch(*p) {
	case ncmd_update_node_state:
		return s.on_cmd_update_node_state(p, l);
	case ncmd_get_master_list:
		return s.on_cmd_get_master_list(sk, p, l);
	case ncmd_broadcast:
		return s.on_cmd_broadcast(p, l);
	case ncmd_unicast:
		{
			address a;
			POP_START(p, l);
			POP_ADDR(a);
			return s.on_cmd_unicast(a, POP_BUF(), POP_REMAIN());
		}
	default:
		s.log(ERROR, "invalid command recv (%d)\n", *p);
		break;
	}
	return NBR_OK;
}

template <class S>
int cluster_protocol_impl<S>::event_handler(S &s, int t, char *p, int l)
{
	switch(t) {
	case nev_finder_query_master_init:
		return s.on_ev_finder_query_master_init(p, l);
	case nev_finder_query_servant_init:
		return s.on_ev_finder_query_servant_init(p, l);
	case nev_finder_query_poll:
		return s.on_ev_finder_query_poll(p, l);
	case nev_finder_reply_init:
		return s.on_ev_finder_reply_init(p, l);
	case nev_finder_reply_poll:
		return s.on_ev_finder_reply_poll(p, l);
	default:
		s.log(ERROR, "invalid event recv (%d)\n", *p);
		break;
	}
	return NBR_OK;
}

/* cluster_finder */

/* cluster_factory_impl */
template <class S, class P, class D>
bool cluster_factory_impl<S,P,D>::master() const
{
	return ((cluster_property &)super::cfg()).m_master;
}

template <class S, class P, class D>
int cluster_factory_impl<S,P,D>::init(const config &cfg)
{
	int r;
	if (cfg.m_max_query <= 0) {
		log(super::ERROR, "node: node needs querymap\n");
		return NBR_ECONFIGURE;
	}
	if (cfg.m_query_bufsz < (int)sizeof(typename S::querydata)) {
		/* force fix */
		((config &)cfg).m_query_bufsz = sizeof(typename S::querydata);
	}
	if ((r = factory::init(cfg,
			super::on_open,
			super::on_close,
			cluster_factory_impl<S,P,D>::on_recv,
			super::on_event,
			cluster_factory_impl<S,P,D>::on_mgr,
			super::on_connect,
			super::on_poll)) < 0) {
		log(super::ERROR, "node: cannot init factory (%d)\n", r);
		return r;
	}
	return cluster_finder_factory_container::init((const cluster_property &)cfg);
}

template <class S, class P, class D>
void cluster_factory_impl<S,P,D>::fin()
{
	if (finder().is_initialized()) {
		finder().fin();
	}
	return super::fin();
}

template <class S, class P, class D>
int cluster_factory_impl<S,P,D>::update_nodestate(const address &a, const D &p)
{
	typename NODE::data &d = (typename NODE::data &)p;
	char work[sizeof(D) * 2];
	int r;
	PUSH_START(work, sizeof(work));
	PUSH_8(cluster_protocol::ncmd_update_node_state);
	PUSH_ADDR(a);
	if ((r = d.pack(PUSH_BUF(), PUSH_REMAIN())) < 0) {
		log(super::ERROR, "update_nodestate: data pack fail(%d)\n", r);
		return r;
	}
	return send(primary(), work, PUSH_LEN());
}

template <class S, class P, class D>
void cluster_factory_impl<S,P,D>::poll(UTIME ut)
{
	/* TODO: need to protect by mutex or something */
	iterator it = pool().begin(), tmp;
	int max_nd = pool().max();
	NODE *wc[max_nd],	/* connected,writable */
		 *c[max_nd],	/* connecting? */
		 *nc[max_nd];	/* not connected */
	const cluster_property &p = (const cluster_property &)super::cfg();
	int r, n_c = 0, n_nc = 0, n_wc = 0;
	/* first: poll finder. */
	finder().poll(ut);
	/* check mesh state and reconnection */
	for (;it != pool().end();) {
		tmp = it;
		ASSERT(from_iter(tmp) == (NODE *)tmp.m_e->get());
		it = pool().next(it);
		if (tmp->valid()) 	{
			if (tmp->poll(ut, false) < 0) {
				tmp->fin();
				pool().erase(tmp->addr());
				continue;
			}
			if (tmp->writable() > 0){ wc[n_wc++] = from_iter(tmp); }
			/* takes too long time (5sec) to connect */
			else if ((ut - tmp->last_access()) >
				P::node_reconnection_timeout_sec * 1000 * 1000) {
				super::log(super::ERROR, "takes too long time to connect\n");
				tmp->incr_conn_retry();
				if (tmp->conn_retry() > P::node_reconnection_retry_times) {
					super::log(super::INFO,
							"%s: retry too much (%u), unregister it\n",
							(const char *)tmp->addr(), tmp->conn_retry());
					tmp->fin();
					pool().erase(tmp->addr());
				}
				else {
					tmp->close();
				}
			}
			else { c[n_c++] = from_iter(tmp); }
		}
		else {
			nc[n_nc++] = from_iter(tmp);
		}
	}
	int n_establish = p.m_master ? pool().use() : p.m_multiplex;
	bool established = false;
	TRACE("n_establish = %u\n", n_establish);
	if (n_wc >= n_establish) {
		established = ((p.m_master) ||(writable() > 0));
	}
	if (established) {
		setstate(cluster_finder_factory_container::ns_establish);
	}
	else if (pool().use() > 0) {
		setstate(cluster_finder_factory_container::ns_found);
	}
	if ((n_wc + n_c) < n_establish) {
		if (!p.m_master && n_nc > 1) {
			NODE::sort(nc, n_nc);
		}
		for (int i = 0; i < n_nc; i++) {
			NODE *p = nc[i];
			if ((r = factory::connect(p)) < 0) { continue; }
			c[n_c++] = p;
			p->update_access();
			if ((n_wc + n_c) >= n_establish) { break; }
		}
	}
	if (p.m_master) {
		/* master: periodically sent mcast to find master */
		if (can_inquiry(ut)) {
			if (finder().inquiry(p.m_sym_poll) >= 0) {
				m_last_inquiry = ut;
			}
		}
	}
	else {
		if (!established) {
			if (can_inquiry(ut)) {
				if (finder().inquiry(p.m_sym_servant_init) >= 0) {
					m_last_inquiry = ut;
				}
			}
		}
		else if (pool().use() <= (n_establish + 1)) {
			if (primary() && primary()->writable() > 0) {
				if (can_inquiry(ut)) {
					/* number of known (and available) master is too short.
					 * query latest master list to master server. */
					char ch = (char)cluster_protocol::ncmd_get_master_list;
					primary()->send(&ch, 1);
					super::log(super::INFO, "query master list\n");
					m_last_inquiry = ut;
				}
			}
		}
	}
	return;
}

template <class S, class P, class D>
int cluster_factory_impl<S,P,D>::on_ev_finder_reply_init(char *p, int l)
{
	address a;
	NODE *n;
	POP_START(p, l);
	U32 sz; int r;
	POP_32(sz);
	for (U32 i = 0; i < sz; i++) {
		POP_ADDR(a);
		if (!(n = (NODE *)pool().find(a))) {
			if (!(n = (NODE *)pool().create(a))) {
				return NBR_EEXPIRE;
			}
		}
		n->setaddr(a);
		if ((r = n->get().unpack(POP_BUF(), POP_REMAIN())) < 0) {
			return r;
		}
		POP_SKIP(r);
	}
	return NBR_OK;
}

template <class S, class P, class D>
int cluster_factory_impl<S,P,D>::on_ev_finder_reply_poll(char *p, int l)
{
	address a;
	NODE *n;
	int r;
	TRACE("on_ev_fider_reply_poll: %u\n", l);
	POP_START(p, l);
	POP_ADDR(a);
	TRACE("on_ev_finder_reply_poll: found addr %s\n", (const char *)a);
	if (!(n = (NODE *)pool().find(a))) {
		if (!(n = (NODE *)pool().create(a))) {
			return NBR_EEXPIRE;
		}
	}
	n->setaddr(a);
	if ((r = n->get().unpack(POP_BUF(), POP_REMAIN())) < 0) {
		return r;
	}
	return NBR_OK;
}

template <class S, class P, class D>
int cluster_factory_impl<S,P,D>::on_open(SOCK sk)
{
	return factory_impl<S>::on_open(sk);
}

template <class S, class P, class D>
int cluster_factory_impl<S,P,D>::on_close(SOCK sk, int r)
{
	return factory_impl<S>::on_close(sk,r);
}

template <class S, class P, class D>
int cluster_factory_impl<S,P,D>::on_recv(SOCK sk, char *p, int l)
{
	S **s = (S**)nbr_sock_get_data(sk, NULL);
	if (s == NULL) {
		return NBR_ENOTFOUND;
	}
	int r;
	if ((r = S::recvping((**s), p, l)) <= 0) {
		return r;
	}
	S *obj = *s;
	typename P::impl *nf;
	switch(*p) {
	case cluster_protocol::ncmd_app:
		obj->on_recv(p, l);
		break;
	default:
		nf = (typename P::impl *)obj->f();
		P::recv_handler(*nf, sk, p, l);
		break;
	}
	return NBR_OK;
}

template <class S, class P, class D>
int cluster_factory_impl<S,P,D>::on_event(SOCK sk, char *p, int l)
{
	return factory_impl<S>::on_event(sk, p, l);
}

template <class S, class P, class D>
void cluster_factory_impl<S,P,D>::on_mgr(SOCKMGR s, int t, char *p, int l)
{
	typename P::impl *nf = (typename P::impl *)nbr_sockmgr_get_data(s);
	P::event_handler(*nf, t, p, l);
}

template <class S, class P, class D>
int cluster_factory_impl<S,P,D>::on_connect(SOCK sk, void *p)
{
	return factory_impl<S>::on_connect(sk, p);
}



/*-------------------------------------------------------------*/
/* servant_session_factory_impl								   */
/*-------------------------------------------------------------*/
template <class S>
int servant_session_factory_impl<S>::init(const config &cfg)
{
	if (!super::pool().init(cfg.m_max_connection, cfg.m_max_connection,
			sizeof(S), cfg.m_option)) {
		return NBR_ESHORT;
	}
	return factory::init(cfg,
			super::on_open,
			super::on_close,
			servant_session_factory_impl<S>::on_recv,
			super::on_event,
			NULL, /* use default */
			super::on_connect,
			super::on_poll);
}

template <class S>
int servant_session_factory_impl<S>::on_cmd_update_node_state(char *p, int l)
{
	if (super::broadcast(p, l) < 0) {
		super::log(super::ERROR, "update_node_state: self bcast fail\n");
		ASSERT(false);
	}
	if (master_node()->broadcast(p, l) < 0) {
		super::log(super::ERROR, "update_node_state: mnode bcast fail\n");
		ASSERT(false);
	}
	return NBR_OK;
}

template <class S>
int servant_session_factory_impl<S>::on_cmd_get_master_list(SOCK sk, char *p, int l)
{
	if (master_node()->event(
			cluster_protocol::nev_finder_query_servant_init,
			(char *)&sk, sizeof(sk)) < 0) {
		super::log(super::ERROR, "get_master_list: send event fail\n");
		ASSERT(false);
	}
	return NBR_OK;
}

template <class S>
int servant_session_factory_impl<S>::on_cmd_broadcast(char *p, int l)
{
	if (master_node()->broadcast(p, l) < 0) {
		super::log(super::ERROR, "cmd_broadcast: mnode bcast fail\n");
		ASSERT(false);
	}
	*p = cluster_protocol::ncmd_app;
	if (super::broadcast(p, l) < 0) {
		super::log(super::ERROR, "cmd_broadcast: self bcast fail\n");
		ASSERT(false);
	}
	return NBR_OK;
}

template <class S>
int servant_session_factory_impl<S>::on_cmd_unicast(address &a, char *p, int l)
{
	S *nd = super::pool().find(a);
	if (nd) {
		/* add 1byte for ncmd_app */
		*(p - 1) = cluster_protocol::ncmd_app;
		if (nd->send(p - 1, l + 1) < 0) {
			super::log(super::ERROR, "cmd_unicast: send fail\n");
			ASSERT(false);
		}
	}
	else if (master_node()->broadcast(p, l) < 0) {
		super::log(super::ERROR, "unicast: mnode bcast fail\n");
		ASSERT(false);
	}
	return NBR_OK;
}

template <class S>
int servant_session_factory_impl<S>::on_recv(SOCK sk, char *p, int l)
{
	S **s = (S**)nbr_sock_get_data(sk, NULL);
	if (s == NULL) {
		return NBR_ENOTFOUND;
	}
	int r;
	if ((r = S::recvping((**s), p, l)) <= 0) {
		return r;
	}
	S *obj = *s;
	servant_session_factory_impl<S> *nf;
	switch(*p) {
	case cluster_protocol::ncmd_app:
		obj->on_recv(p + 1, l - 1);
		break;
	default:
		nf = (servant_session_factory_impl<S> *)obj->f();
		recv_handler(*nf, sk, p, l);
		break;
	}
	return NBR_OK;
}



/*-------------------------------------------------------------*/
/* sfc::cluster::master_session_factory_impl				   */
/*-------------------------------------------------------------*/
template <class S>
int master_session_factory_impl<S>::init(const config &cfg)
{
	if (!super::pool().init(cfg.m_max_connection, cfg.m_max_connection,
			sizeof(S), cfg.m_option)) {
		return NBR_ESHORT;
	}
	return factory::init(cfg,
			super::on_open,
			super::on_close,
			master_session_factory_impl<S>::on_recv,
			super::on_event,
			NULL, /* use default */
			super::on_connect,
			super::on_poll);
}

template <class S>
int master_session_factory_impl<S>::on_cmd_update_node_state(char *p, int l)
{
	if (super::broadcast(p, l) < 0) {
		super::log(super::ERROR, "update_node_state: self bcast fail\n");
		ASSERT(false);
	}
	return NBR_OK;
}

template <class S>
int master_session_factory_impl<S>::on_cmd_broadcast(char *p, int l)
{
	*p = cluster_protocol::ncmd_app;
	if (servant_session()->broadcast(p, l) < 0) {
		super::log(super::ERROR, "cmd_broadcast: svnt bcast fail\n");
		ASSERT(false);
	}
	return NBR_OK;
}

template <class S>
int master_session_factory_impl<S>::on_cmd_unicast(address &a, char *p, int l)
{
	S *nd = super::pool().find(a);
	if (nd) {
		/* add 1byte for ncmd_app */
		*(p - 1) = cluster_protocol::ncmd_app;
		if (nd->send(p - 1, l + 1) < 0) {
			super::log(super::ERROR, "cmd_unicas: send fail\n");
			ASSERT(false);
		}
	}
	return NBR_OK;
}

template <class S>
int master_session_factory_impl<S>::on_recv(SOCK sk, char *p, int l)
{
	S **s = (S**)nbr_sock_get_data(sk, NULL);
	if (s == NULL) {
		return NBR_ENOTFOUND;
	}
	int r;
	if ((r = S::recvping((**s), p, l)) <= 0) {
		return r;
	}
	S *obj = *s;
	master_session_factory_impl<S> *nf;
	switch(*p) {
	case cluster_protocol::ncmd_app:
		obj->on_recv(p + 1, l - 1);
		break;
	default:
		nf = (master_session_factory_impl<S> *)obj->f();
		recv_handler(*nf, sk, p, l);
		break;
	}
	return NBR_OK;
}

/* servant_cluster_factory_impl */
template <class C, class SN>
int servant_cluster_factory_impl<C,SN>::init(const config &cfg)
{
	int r;
	const property &p = (const property &)cfg;
	if (!super::init_pool(p, sizeof(typename super::NODE))) {
		super::log(super::ERROR, "servant node: cannot init mempool\n");
		return NBR_ESHORT;
	}
	if ((r = super::init(p)) < 0) {
		super::log(super::ERROR, "servant node: super::init fail(%d)\n", r);
		return r;
	}
	if ((r = m_fccl.init(p.m_client_conf)) < 0) {
		super::log(super::ERROR, "servant node: m_clint.init fail(%d)\n", r);
		return r;
	}
	if (!super::finder().register_factory(this, p.m_sym_servant_init)) {
		super::log(super::ERROR,
				"servant node: fail to register factory for servant\n");
		return NBR_EEXPIRE;
	}
	if ((r = super::finder().inquiry(p.m_sym_servant_init)) < 0) {
		super::log(super::ERROR, "servant node: master inquiry fail (%d)\n", r);
		return r;
	}
	return r;
}

template <class C, class SN>
void servant_cluster_factory_impl<C,SN>::fin()
{
	m_fccl.fin();
	super::fin();
}

template <class C, class SN>
void servant_cluster_factory_impl<C,SN>::poll(UTIME ut)
{
	m_fccl.poll(ut);
	super::poll(ut);
}

/* master_cluster_factory_impl */
template <class M, class S, class N>
int master_cluster_factory_impl<M,S,N>::init(const config &c)
{
	int r;
	const property &p = (const property &)c;
	if (p.m_max_query <= 0) {
		return NBR_ECONFIGURE;
	}
	if (p.m_query_bufsz < (int)sizeof(typename S::querydata)) {
		((property &)p).m_query_bufsz = sizeof(typename S::querydata);
	}
	if ((r = factory::init(p,
			super::on_open,
			super::on_close,
			super::on_recv,
			super::on_event,
			master_cluster_factory_impl<M,S,N>::on_mgr,
			super::on_connect,
			super::on_poll)) < 0) {
		super::log(super::ERROR, "master node: cannot init factory (%d)\n", r);
		return r;
	}
	if (p.m_master_conf.disabled() || p.m_servant_conf.disabled() ||
		p.m_master_conf.client() || p.m_servant_conf.client()) {
		super::log(super::ERROR, "master node: master/servant factory cannot be disabled\n");
		return NBR_ECONFIGURE;
	}
	if ((r = m_fcmstr.init(p.m_master_conf)) < 0) {
		super::log(super::ERROR, "master node: fail to init master factory (%d)", r);
		return r;
	}
	if ((r = m_fcsvnt.init(p.m_servant_conf)) < 0) {
		super::log(super::ERROR, "master node: fail to init servant factory (%d)", r);
		return r;
	}
	if (!super::init_pool(p, sizeof(typename super::NODE))) {
		super::log(super::ERROR, "master node: cannot init mempool\n");
		return NBR_ESHORT;
	}
	/* to assure to get at least one master session when someone query
	 * master server addr through finder, connect local master server */
	typename super::NODE *lo =
			(typename super::NODE *)super::pool().create(m_fcmstr.ifaddr());
	if (!lo) {
		super::log(super::ERROR, "master node: cannot allocate loopback session");
		return NBR_EEXPIRE;
	}
	/* store basic node info */
	lo->setdata_from(m_fcsvnt);
	if ((r = super::connect(lo, m_fcmstr.ifaddr())) < 0) {
		super::log(super::ERROR, "master node: cannot connect to local master (%d)\n", r);
		return r;
	}
	if ((r = cluster_finder_factory_container::init(p)) < 0) {
		super::log(super::ERROR, "fail to initialize finder (%d)\n", r);
		return r;
	}
	/* register factory */
	if (!super::finder().register_factory(this, p.m_sym_master_init)) {
		super::log(super::ERROR,
				"master node: fail to register factory for master\n");
		return NBR_EEXPIRE;
	}
	if (!super::finder().register_factory(this, p.m_sym_servant_init)) {
		super::log(super::ERROR,
				"master node: fail to register factory for servant\n");
		return NBR_EEXPIRE;
	}
	if (!super::finder().register_factory(this, p.m_sym_poll)) {
		super::log(super::ERROR,
				"master node: fail to register factory for master\n");
		return NBR_EEXPIRE;
	}
	if ((r = super::finder().inquiry(p.m_sym_master_init)) < 0) {
		super::log(super::ERROR, "master node: master inquiry fail (%d)\n", r);
		return r;
	}
	return NBR_OK;
}

template <class M, class S, class N>
void master_cluster_factory_impl<M,S,N>::fin()
{
	m_fcmstr.fin();
	m_fcsvnt.fin();
	super::fin();
}

template <class M, class S, class N>
void master_cluster_factory_impl<M,S,N>::poll(UTIME ut)
{
	m_fcmstr.poll(ut);
	m_fcsvnt.poll(ut);
	super::poll(ut);
}

template <class M, class S, class N>
int master_cluster_factory_impl<M,S,N>::on_finder_query_init(
		bool is_master, char *p, int l)
{
	const property &c = (const property &)super::cfg();
	if (!super::ready()) {
		super::log(super::INFO, "this node not ready %u %u\n",
				c.m_master, super::state());
		return NBR_EINVAL;
	}
	if (l != sizeof(SOCK)) {
		super::log(super::ERROR, "length illegal %u\n", l);
		return NBR_EINVAL;
	}
	SOCK *sk = (SOCK *)p;
	cluster_finder nf(*sk, &(super::finder()));
	{
		typename super::NODE *n;
		char opt[finder_protocol::MAX_OPT_LEN];
		char a[address::SIZE], nodeaddr[address::SIZE];
		PUSH_START(opt, sizeof(opt));
		PUSH_8(finder_protocol::reply);
		PUSH_STR(is_master ? c.m_sym_master_init : c.m_sym_servant_init);
		PUSH_32(pool().use());
		TRACE("found %u node\n", pool().use());
		typename sspool::iterator s = pool().begin();
		int r;
		for (;s != pool().end(); s = pool().next(s)) {
			if ((r = s->addr().addrpart(a, sizeof(a))) < 0) {
				super::log(super::ERROR, "fail to get addrpart (%s)",(const char *)s->addr());
				return r;
			}
			snprintf(nodeaddr, sizeof(nodeaddr) - 1, "%s:%hu", (const char *)a,
				is_master ? c.m_master_port : c.m_servant_port);
			PUSH_STR(nodeaddr);
			TRACE("node addr=%s\n", nodeaddr);
			n = from_iter(s);
			if ((r = n->get().pack(PUSH_BUF(), PUSH_REMAIN())) < 0) {
				super::log(super::ERROR, "fail to pack nodedata (%d)\n", r);
				return r;
			}
			PUSH_SKIP(r);
			TRACE("push len now = %u\n", PUSH_LEN());
		}
		TRACE("push len final = %u\n", PUSH_LEN());
		return nf.send(opt, PUSH_LEN());
	}
}

template <class M, class S, class N>
int master_cluster_factory_impl<M,S,N>::on_ev_finder_query_poll(char *p, int l)
{
	const property &c = (const property &)super::cfg();
	if (!super::ready()) {
		super::log(super::INFO, "this node not ready %u %u\n",
				c.m_master,super::state());
		return NBR_EINVAL;
	}
	if (l != sizeof(SOCK)) {
		super::log(super::ERROR, "length illegal %u\n", l);
		ASSERT(false);
		return NBR_EINVAL;
	}
	SOCK *sk = (SOCK *)p;
	cluster_finder nf(*sk, &(super::finder()));
	int r;
	typename super::NODE *n =
			(typename super::NODE *)pool().find(m_fcmstr.ifaddr());
	if (!n) {
		super::log(super::ERROR, "node not found (%s)\n",
				(const char *)m_fcmstr.ifaddr());
		return NBR_ENOTFOUND;
	}
	{
		char opt[finder_protocol::MAX_OPT_LEN];
		PUSH_START(opt, sizeof(opt));
		PUSH_8(finder_protocol::reply);
		PUSH_STR(c.m_sym_poll);
		PUSH_ADDR(m_fcmstr.ifaddr());
		if ((r = n->get().pack(PUSH_BUF(), PUSH_REMAIN())) < 0) {
			super::log(super::ERROR, "fail to pack nodedata (%d)\n", r);
			return r;
		}
		PUSH_SKIP(r);
		return nf.send(opt, PUSH_LEN());
	}
}

template <class M, class S, class N>
void master_cluster_factory_impl<M,S,N>::on_mgr(SOCKMGR s, int t, char *p, int l)
{
	master_cluster_factory_impl<M,S,N> *nf =
			(master_cluster_factory_impl<M,S,N> *)nbr_sockmgr_get_data(s);
	super::protocol::event_handler(*nf, t, p, l);
}

/* master_cluster_property */
template <class MF, class SF>
master_cluster_property<MF,SF>::master_cluster_property(
	BASE_CONFIG_PLIST, const char *sym,
	int multiplex, int packet_bkup_size,
	int master_port, int servant_port,
	const MC &master_conf, const SC &servant_conf) :
	cluster_property(BASE_CONFIG_CALL, sym, 1, multiplex, packet_bkup_size),
	m_master_port(master_port), m_servant_port(servant_port),
	m_master_conf(master_conf), m_servant_conf(servant_conf)
{
}

template <class MF, class SF>
int master_cluster_property<MF,SF>::set(
		const char *k, const char *v)
{
	if (nbr_mem_cmp(k, "master.", sizeof("master.") - 1) == 0) {
		return m_master_conf.set(k + sizeof("master.") - 1, v);
	}
	else if (nbr_mem_cmp(k, "servant.", sizeof("servant.") - 1) == 0) {
		return m_master_conf.set(k + sizeof("servant.") - 1, v);
	}
	else {
		if (cmp(k, "master_port")) {
			SAFETY_ATOI(v, m_master_port, U16);
			snprintf(m_master_conf.m_host,
					sizeof(m_master_conf.m_host),
					"0.0.0.0:%hu", m_master_port);
			return NBR_OK;
		}
		else if (cmp(k, "servant_port")) {
			SAFETY_ATOI(v, m_servant_port, U16);
			snprintf(m_servant_conf.m_host,
					sizeof(m_master_conf.m_host),
					"0.0.0.0:%hu", m_servant_port);
			return NBR_OK;
		}
		return cluster_property::set(k, v);
	}
}

/* servant_cluster_property */
template <class CF>
int servant_cluster_property<CF>::set(
		const char *k, const char *v)
{
	if (nbr_mem_cmp(k, "client.", sizeof("client.") - 1) == 0) {
		return m_client_conf.set(k + sizeof("client.") - 1, v);
	}
	return cluster_property::set(k, v);
}
