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
/* sfc::session												   */
/*-------------------------------------------------------------*/
template <class S, class P>
int session::factory_impl<S,P>::init(const config &cfg)
{
	if (!init_pool(cfg)) {
		return NBR_ESHORT;
	}
	return session::factory::init(cfg,
			session::factory_impl<S,P>::on_open,
			session::factory_impl<S,P>::on_close,
			session::factory_impl<S,P>::on_recv,
			session::factory_impl<S,P>::on_event,
			NULL, /* use default */
			session::factory_impl<S,P>::on_connect,
			session::factory_impl<S,P>::on_poll);
}

template <class S, class P>
void session::factory_impl<S,P>::fin()
{
	m_skm = NULL;
	P::fin();
}

template <class S, class P>
int session::factory_impl<S,P>::broadcast(char *p, int l)
{
	int r;
	iterator it = pool().begin();
	for (;it != pool().end(); it = pool().next(it)) {
		if ((r = it->send(p, l)) < 0) {
			return r;
		}
	}
	return NBR_OK;
}

template <class S, class P>
void session::factory_impl<S,P>::poll(UTIME ut)
{
	iterator p = pool().begin(), tmp;
	for (;p != pool().end();) {
		tmp = p;
		p = pool().next(p);
		if (!tmp->valid()) {
			if (cfg().client()) {
				/* no-recconection or poll failure */
				if (tmp->cfg().m_ld_wait <= 0 || tmp->poll(ut, false) < 0) {
					goto finalize_session;
				}
				else if ((ut - tmp->last_access()) > tmp->cfg().m_ld_wait) {
					S &rs = *tmp;
					if (connect(&rs, NULL, tmp->cfg().proto_p()) < 0) {
						rs.incr_conn_retry();
						continue;
					}
					else {
						ASSERT(tmp->valid());
						tmp->update_access();
					}
				}
				continue;
			}
			else if (tmp->poll(ut, false) > 0) {
				continue;
			}
finalize_session:
			P::erase(tmp);
		}
	}
}

template <class S, class P>
int session::factory_impl<S,P>::on_open(SOCK sk)
{
	S **s = (S**)nbr_sock_get_data(sk, NULL), *obj;
	if (s == NULL) {
		return NBR_ENOTFOUND;
	}
	obj = *s;
	int r;
	if ((r = obj->on_open(obj->cfg())) < 0) {
		return r;
	}
	obj->clear_conn_retry();
	obj->setstate(session::ss_connected);
	return NBR_OK;
}

template <class S, class P>
int session::factory_impl<S,P>::on_close(SOCK sk, int reason)
{
	S **s = (S**)nbr_sock_get_data(sk, NULL);
	if (s == NULL) {
		return NBR_ENOTFOUND;
	}
	S *obj = *s;
	obj->on_close(reason);
	*s = NULL;
	obj->clear_sock();
	obj->setstate(session::ss_closed);
	return NBR_OK;

}

template <class S, class P>
int session::factory_impl<S,P>::on_connect(SOCK sk, void *p)
{
	S **s = (S**)nbr_sock_get_data(sk, NULL);
	session::factory_impl<S,P> *f =
		(session::factory_impl<S,P> *)nbr_sockmgr_get_data(nbr_sock_get_mgr(sk));
	if (s == NULL || f == NULL) {
		return NBR_ENOTFOUND;
	}
	*s = (S *)(p ? p : f->create(sk));
	if (!(*s)) {
		return NBR_EEXPIRE;
	}
	(*s)->setstate(session::ss_connecting);
	(*s)->set(sk, f);
	(*s)->setaddr();
	return NBR_OK;
}

template <class S, class P>
int session::factory_impl<S,P>::on_recv(SOCK sk, char *p, int l)
{
	S **s = (S**)nbr_sock_get_data(sk, NULL);
	if (s == NULL) {
		return NBR_ENOTFOUND;
	}
	S *obj = *s;
	if (obj->ping().recv((**s), p, l) < 0) {
		return NBR_EINVAL;	/* invalid ping result received */
	}
	obj->update_access();
	return obj->on_recv(p, l);
}

template <class S, class P>
int session::factory_impl<S,P>::on_event(SOCK sk, char *p, int l)
{
	S **s = (S**)nbr_sock_get_data(sk, NULL);
	if (s == NULL) {
		return NBR_ENOTFOUND;
	}
	return (*s)->on_event(p, l);
}

template <class S, class P>
void session::factory_impl<S,P>::on_poll(SOCK sk)
{
	S **s = (S**)nbr_sock_get_data(sk, NULL);
	if (s == NULL) {
		return;
	}
	S *obj = *s;
	UTIME ut = nbr_clock();
	if (obj->cfg().m_ping_timeo > 0 && !obj->ping().validate(*obj, ut)) {
		obj->close();
		return;
	}
	obj->poll(ut, true);
}



/*-------------------------------------------------------------*/
/* sfc::finder											   */
/*-------------------------------------------------------------*/
template <class F>
int finder::factory::on_recv(SOCK sk, char *p, int l)
{
	finder::factory *f =
		(finder::factory *)nbr_sockmgr_get_data(nbr_sock_get_mgr(sk));
	if (!f) {
		f->log(ERROR, "factory is empty\n");
		return NBR_ENOTFOUND;
	}
	char sym[finder::SYM_SIZE], opt[finder::MAX_OPT_LEN];
	int olen = finder::MAX_OPT_LEN;
	U8 is_reply;
	POP_START(p, l);
	POP_8(is_reply);
	POP_STR(sym, (int)sizeof(sym));
	F obj(sk, f);
	if (is_reply) {
		POP_8A(opt, olen);
		return obj.on_reply(sym, opt, olen);
	}
	else {
		return obj.on_inquiry(sym, opt, olen);
	}
}



/*-------------------------------------------------------------*/
/* sfc::node											   */
/*-------------------------------------------------------------*/
inline node::protocol::nodeevent
node::protocol::convert(nodefinder::finderevent e)
{
	switch(e) {
	case nodefinder::fev_init_master_query:
		return nev_finder_query_master_init;
	case nodefinder::fev_init_servant_query:
		return nev_finder_query_servant_init;
	case nodefinder::fev_poll_query:
		return nev_finder_query_poll;
	case nodefinder::fev_init_reply:
		return nev_finder_reply_init;
	case nodefinder::fev_poll_reply:
		return nev_finder_reply_poll;
	default:
		return nev_finder_invalid;
	}
}

template <class S, class D>
bool node::factory_impl<S,D>::master() const
{
	return ((node::property &)cfg()).m_master;
}

template <class S, class D>
int node::factory_impl<S,D>::init(const config &cfg)
{
	int r;
	if (cfg.m_max_query <= 0) {
		log(ERROR, "node: node needs querymap\n");
		return NBR_ECONFIGURE;
	}
	cfg.m_query_bufsz = sizeof(typename S::queryctx);
	if ((r = session::factory::init(cfg,
			super::on_open,
			super::on_close,
			node::factory_impl<S,D>::on_recv,
			super::on_event,
			node::factory_impl<S,D>::on_mgr,
			super::on_connect,
			super::on_poll)) < 0) {
		log(ERROR, "node: cannot init factory (%d)\n", r);
		return r;
	}
	return init_finder(cfg);
}

template <class S, class D>
void node::factory_impl<S,D>::fin()
{
	if (m_finder) {
		m_finder->fin();
		delete m_finder;
		m_finder = NULL;
	}
	return super::fin();
}

template <class S, class D>
int node::factory_impl<S,D>::update_nodestate(const address &a, const D &p)
{
	typename NODE::data &d = (typename NODE::data &)p;
	char work[sizeof(D) * 2];
	int r;
	PUSH_START(work, sizeof(work));
	PUSH_8(ncmd_update_node_state);
	PUSH_ADDR(a);
	if ((r = d.pack(PUSH_BUF(), PUSH_REMAIN())) < 0) {
		log(ERROR, "update_nodestate: data pack fail(%d)\n", r);
		return r;
	}
	return send(*primary(), work, PUSH_LEN());
}

template <class S, class D>
void node::factory_impl<S,D>::poll(UTIME ut)
{
	/* TODO: need to protect by mutex or something */
	iterator it = pool().begin(), tmp;
	int max_nd = pool().max();
	NODE *wc[max_nd],	/* connected,writable */
		 *c[max_nd],	/* connecting? */
		 *nc[max_nd];	/* not connected */
	const node::property &p = (const node::property &)cfg();
	int r, n_c = 0, n_nc = 0, n_wc = 0;
	/* first: poll finder. */
	m_finder->poll(ut);
	/* check mesh state and reconnection */
	for (;it != pool().end();) {
		tmp = it;
		it = pool().next(it);
		if (tmp->valid()) 	{
			if (tmp->poll(ut, false) < 0) {
				tmp->fin();
				pool().erase(tmp->addr());
				continue;
			}
			if (tmp->writable() > 0){ wc[n_wc++] = &(*tmp); }
			else if ((ut - tmp->last_access()) > 5 * 1000 * 1000) {
				/* takes too long time (5sec) to connect */
				tmp->close();
				nc[n_nc++] = &(*tmp);
			}
			else { c[n_c++] = &(*tmp); }
		}
		else { nc[n_nc++] = &(*tmp); }
	}
	int n_establish = p.m_master ? pool().size() : p.m_multiplex;
	bool established = false;
	if (n_wc >= n_establish) {
		established =
			((p.m_master) || (m_primary && m_primary->writable() > 0));
	}
	if (established) {
		setstate(ns_establish); /* established */
	}
	if ((n_wc + n_c) < n_establish) {
		NODE::sort(nc, n_nc);
		for (int i = 0; i < n_nc; i++) {
			NODE *p = nc[i];
			if ((r = connect(*p)) < 0) { continue; }
			c[n_c++] = p;
			if ((n_wc + n_c) >= n_establish) { break; }
		}
	}
	if (p.m_master) {
		/* master: periodically sent mcast to find master */
		m_finder->inquiry(p.m_sym_poll);
	}
	else {
		if (!established) {
			/* servant: if primary is disabled, found new */
			if (n_wc > 0) {
				for (int i = 0; i < n_wc; i++) {
					ASSERT(wc[i]);
					if (wc[i]->writable() > 0) { m_primary = wc[0]; }
				}
			}
		}
		if (pool().size() <= (n_establish + 1)) {
			if (m_primary && m_primary->writable() > 0) {
				/* number of known (and available) master is too short.
				 * query latest master list to master server. */
				m_primary->send("", 0);
			}
		}
	}
	return;
}

template <class S, class D>
int node::factory_impl<S,D>::on_finder_reply_init(char *p, int l)
{
	address a;
	NODE *n;
	POP_START(p, l);
	U32 sz; int r;
	POP_32(sz);
	for (U32 i = 0; i < sz; i++) {
		POP_ADDR(a);
		if (!(n = pool().find(addr))) {
			if (!(n = pool().create(addr))) {
				return NBR_EEXPIRE;
			}
		}
		n->setaddr(addr);
		if ((r = n->get().unpack(POP_BUF(), POP_REMAIN())) < 0) {
			return r;
		}
		POP_SKIP(r);
	}
	return NBR_OK;
}

template <class S, class D>
int node::factory_impl<S,D>::on_finder_reply_poll(char *p, int l)
{
	address a;
	NODE *n;
	int r;
	POP_START(p, l);
	POP_ADDR(a);
	if (!(n = pool().find(addr))) {
		if (!(n = pool().create(addr))) {
			return NBR_EEXPIRE;
		}
	}
	n->setaddr(addr);
	if ((r = n->get().unpack(POP_BUF(), POP_REMAIN())) < 0) {
		return r;
	}
	return NBR_OK;
}

template <class S, class D>
int node::factory_impl<S,D>::on_open(SOCK sk)
{
	return session::factory_impl<S>::on_open(sk);
}

template <class S, class D>
int node::factory_impl<S,D>::on_close(SOCK sk, int r)
{
	return session::factory_impl<S>::on_close(sk,r);
}

template <class S, class D>
int node::factory_impl<S,D>::on_recv(SOCK sk, char *p, int l)
{
	S **s = (S**)nbr_sock_get_data(sk, NULL);
	if (s == NULL) {
		return NBR_ENOTFOUND;
	}
	node::factory_impl<S,D> *nf =
			(node::factory_impl<S,D> *)nbr_sockmgr_get_data(s);
	S *obj = *s;
	if (obj->ping().recv((**s), p, l) < 0) {
		return NBR_EINVAL;	/* invalid ping result received */
	}
	switch(*p) {
	case ncmd_app:
		obj->on_recv(p, l);
		break;
	case ncmd_update_node_state:
		/* p + 1, l - 1 means skip command code (ncmd_***) */
		nf->event(convert(nodefinder::fev_poll_reply), p + 1, l - 1);
		break;
	case ncmd_get_master_list:
		/* p + 1, l - 1 means skip command code (ncmd_***) */
		nf->event(convert(nodefinder::fev_init_reply), p + 1, l - 1);
		break;
	default:
		log(ERROR, "invalid node command (%d)\n", *p);
		break;
	}
	return NBR_OK;
}

template <class S, class D>
int node::factory_impl<S,D>::on_event(SOCK sk, char *p, int l)
{
	return session::factory_impl<S>::on_event(sk, p, l);
}

template <class S, class D>
void node::factory_impl<S,D>::on_mgr(SOCKMGR s, int t, char *p, int l)
{
	node::factory_impl<S,D> *nf =
			(node::factory_impl<S,D> *)nbr_sockmgr_get_data(s);
	if (nf == NULL) {
		return;
	}
	switch(t) {
	case nev_finder_reply_init:
		nf->on_finder_reply_init(p, l);
		break;
	case nev_finder_reply_poll:
		nf->on_finder_reply_poll(p, l);
		break;
	default:
		ASSERT(false);
		nf->log(ERROR, "invalid event(%u) recved\n", t);
		break;
	}
}

template <class S, class D>
int node::factory_impl<S,D>::on_connect(SOCK sk, void *p)
{
	return session::factory_impl<S>::on_connect(sk, p);
}

template <class S, class D>
void node::factory_impl<S,D>::on_poll(SOCK sk)
{
	return session::factory_impl<S>::on_poll(sk);
}



/*-------------------------------------------------------------*/
/* sfc::cluster::servantsession								   */
/*-------------------------------------------------------------*/
template <class S>
int servantsession::factory_impl<S>::init(const config &cfg)
{
	if (!super::pool().init(cfg.m_max_connection, cfg.m_max_connection,
			sizeof(S), cfg.m_option)) {
		return NBR_ESHORT;
	}
	return servantsession::factory::init(cfg,
			super::on_open,
			super::on_close,
			servantsession::factory_impl<S>::on_recv,
			super::on_event,
			super::on_mgr, /* use default */
			super::on_connect,
			super::on_poll);
}

template <class S>
int servantsession::factory_impl<S>::on_recv(SOCK sk, char *p, int l)
{
	S **s = (S**)nbr_sock_get_data(sk, NULL);
	if (s == NULL) {
		return NBR_ENOTFOUND;
	}
	servantsession::factory_impl<S> *nf =
			(servantsession::factory_impl<S> *)nbr_sockmgr_get_data(s);
	S *obj = *s;
	if (obj->ping().recv((**s), p, l) < 0) {
		return NBR_EINVAL;	/* invalid ping result received */
	}
	switch(*p) {
	case ncmd_app:
		obj->on_recv(p, l);
		break;
	case ncmd_update_node_state:
		super::broadcast(p, l);
		nf->master_node()->broadcast(p, l);
		break;
	case ncmd_get_master_list:
		nf->master_node()->event(
			node::protocol::convert(node::nodefinder::fev_init_servant_query),
			sk, sizeof(sk));
		break;
	case ncmd_broadcast:
		{
			nf->master_node()->broadcast(p, l);
			*p = ncmd_app;
			super::broadcast(p, l);
		} break;
	case ncmd_unicast:
		{
			address a;
			POP_START(p, l);
			POP_ADDR(a);
			S *nd = super::pool().find(a);
			if (nd) {
				p = POP_BUF();
				/* add 1byte for ncmd_app */
				*(p - 1) = ncmd_app;
				l = POP_REMAIN() + 1;
				nd->send(p, l);
			}
			else { nf->master_node()->broadcast(p, l); }
		} break;
	default:
		log(ERROR, "invalid node command (%d)\n", *p);
		break;
	}
	return NBR_OK;
}



/*-------------------------------------------------------------*/
/* sfc::cluster::mastersession								   */
/*-------------------------------------------------------------*/
template <class S>
int mastersession::factory_impl<S>::init(const config &cfg)
{
	if (!super::pool().init(cfg.m_max_connection, cfg.m_max_connection,
			sizeof(S), cfg.m_option)) {
		return NBR_ESHORT;
	}
	return servantsession::factory::init(cfg,
			super::on_open,
			super::on_close,
			mastersession::factory_impl<S>::on_recv,
			super::on_event,
			super::on_mgr, /* use default */
			super::on_connect,
			super::on_poll);
}

template <class S>
int mastersession::factory_impl<S>::on_recv(SOCK sk, char *p, int l)
{
	S **s = (S**)nbr_sock_get_data(sk, NULL);
	if (s == NULL) {
		return NBR_ENOTFOUND;
	}
	mastersession::factory_impl<S> *nf =
			(mastersession::factory_impl<S> *)nbr_sockmgr_get_data(s);
	S *obj = *s;
	if (obj->ping().recv((**s), p, l) < 0) {
		return NBR_EINVAL;	/* invalid ping result received */
	}
	switch(*p) {
	case ncmd_app:
		obj->on_recv(p, l);
		break;
	case ncmd_update_node_state:
		super::broadcast(p, l);
		break;
	case ncmd_get_master_list:
		break;
	case ncmd_broadcast:
		{
			*p = ncmd_app;
			servant_session()->broadcast(p, l);
		} break;
	case ncmd_unicast:
		{
			address a;
			POP_START(p, l);
			POP_ADDR(a);
			S *nd = servant_session()->pool().find(a);
			if (nd) {
				p = POP_BUF();
				/* add 1byte for ncmd_app */
				*(p - 1) = ncmd_app;
				l = POP_REMAIN() + 1;
				nd->send(p, l);
			}
		} break;
	default:
		log(ERROR, "invalid node command (%d)\n", *p);
		break;
	}
	return NBR_OK;
}



/*-------------------------------------------------------------*/
/* sfc::cluster::masternode									   */
/*-------------------------------------------------------------*/
/* factory */
template <class M, class S, class N>
int masternode::factory_impl<M,S,N>::init(const config &c)
{
	int r;
	const masternode::property<M,S> &p =
			(const masternode::property<M,S> &)c;
	if (p.m_master_conf.disabled() || p.m_servant_conf.disabled()) {
		log(ERROR, "master node: master/servant factory cannot be disabled\n");
		return NBR_ECONFIGURE;
	}
	if ((r = m_fcmstr.init(p.m_master_conf)) < 0) {
		log(ERROR, "master node: fail to init master factory (%d)", r);
		return r;
	}
	if ((r = m_fcsvnt.init(p.m_servant_conf)) < 0) {
		log(ERROR, "master node: fail to init servant factory (%d)", r);
		return r;
	}
	if (!super::pool().init(p.m_max_connection, p.max_connection,
			sizeof(S), p.m_option)) {
		log(ERROR, "master node: cannot init mempool\n");
		return NBR_ESHORT;
	}
	address a;
	if ((r = m_fcmstr.get(a)) < 0) {
		log(ERROR, "master node: cannot get master bind addr(%d)\n", r);
		return r;
	}
	/* to assure to get at least one master session when someone query
	 * master server addr through finder, connect local master server */
	typename super::NODE *lo = super::pool().create(a);
	if (!lo) {
		log(ERROR, "master node: cannot allocate loopback session");
		return NBR_EEXPIRE;
	}
	/* store basic node info */
	lo->setdata_from(*m_fcsvnt);
	if ((r = super::connect(*lo, a)) < 0) {
		log(ERROR, "master node: cannot connect to local master (%d)\n", r);
		return r;
	}
	if (p.m_max_query <= 0) {
		log(ERROR, "master node: node needs querymap\n");
		return NBR_ECONFIGURE;
	}
	p.m_query_bufsz = sizeof(S::queryctx);
	if ((r = session::factory::init(cfg,
			super::on_open,
			super::on_close,
			super::on_recv,
			super::on_event,
			super::on_mgr,
			super::on_connect,
			super::on_poll)) < 0) {
		log(ERROR, "master node: cannot init factory (%d)\n", r);
		return r;
	}
	if ((r = init_finder(p)) < 0) {
		log(ERROR, "fail to initialize finder (%d)\n", r);
		return r;
	}
	/* register factory */
	if (!node::finder_mixin::finder()->register_factory(this, p.m_sym_master_init)) {
		log(ERROR, "master node: fail to register factory for master\n");
		return NBR_EEXPIRE;
	}
	if (!node::finder_mixin::finder()->register_factory(this, p.m_sym_servant_init)) {
		log(ERROR, "master node: fail to register factory for servant\n");
		return NBR_EEXPIRE;
	}
	if ((r = node::finder_mixin::finder()->inquiry(p.m_sym_master_init)) < 0) {
		log(ERROR, "master node: master inquiry fail (%d)\n", r);
		return r;
	}
	return NBR_OK;
}

template <class M, class S, class N>
void masternode::factory_impl<M,S,N>::fin()
{
	m_fcmstr.fin();
	m_fcsvnt.fin();
	super::fin();
}

template <class M, class S, class N>
void masternode::factory_impl<M,S,N>::poll(UTIME ut)
{
	m_fcmstr.poll(ut);
	m_fcsvnt.poll(ut);
	super::poll(ut);
}

template <class M, class S, class N>
int masternode::factory_impl<M,S,N>::on_finder_query_init(
		bool is_master, char *p, int l)
{
	const masternode::property<M,S> &c = (const masternode::property<M,S> &)cfg();
	if (!super::ready()) {
		log(INFO, "this node not ready %u %u\n", c.m_master, super::state());
		return NBR_EINVAL;
	}
	if (l != sizeof(SOCK)) {
		return NBR_EINVAL;
	}
	SOCK *sk = (SOCK *)p;
	nodefinder nf(*sk, node::finder_mixin::finder());
	{
		char opt[finder::MAX_OPT_LEN];
		char a[address::SIZE], nodeaddr[address::SIZE];
		PUSH_START(opt, sizeof(opt));
		PUSH_32(pool().size());
		typename sspool::iterator s = pool().begin();
		int r;
		for (;s != pool().end(); s = pool().next(s)) {
			if ((r = s->addr().addrpart(a, sizeof(a))) < 0) {
				log(ERROR, "fail to get addrpart (%s)", s->addr());
				return r;
			}
			nbr_str_printf(nodeaddr, sizeof(nodeaddr) - 1, "%s:%hu", a,
				is_master ? c.m_master_port : c.m_servant_port);
			PUSH_STR(nodeaddr);
			if ((r = s->get().pack(PUSH_BUF(), PUSH_REMAIN())) < 0) {
				log(ERROR, "fail to pack nodedata (%d)\n", r);
				return r;
			}
			PUSH_SKIP(r);
		}
		return nf.send(opt, PUSH_LEN());
	}
}

template <class M, class S, class N>
int masternode::factory_impl<M,S,N>::on_finder_query_poll(char *p, int l)
{
	const masternode::property<M,S> &c = (const masternode::property<M,S> &)cfg();
	if (!super::ready()) {
		log(INFO, "this node not ready %u %u\n", c.m_master, super::state());
		return NBR_EINVAL;
	}
	if (l != sizeof(SOCK)) {
		return NBR_EINVAL;
	}
	SOCK *sk = (SOCK *)p;
	nodefinder nf(*sk, node::finder_mixin::finder());
	address a;
	int r;
	if ((r = m_fcmstr.get(a)) < 0) {
		log(ERROR, "cannot get address (%d)\n", r);
		return r;
	}
	typename super::NODE *n = pool().find(a);
	if (!n) {
		log(ERROR, "node not found (%s)\n", a);
		return NBR_ENOTFOUND;
	}
	{
		char opt[finder::MAX_OPT_LEN];
		char a[address::SIZE], nodeaddr[address::SIZE];
		PUSH_START(opt, sizeof(opt));
		PUSH_ADDR(a);
		if ((r = n->get().pack(PUSH_BUF(), PUSH_REMAIN())) < 0) {
			log(ERROR, "fail to pack nodedata (%d)\n", r);
			return r;
		}
		PUSH_SKIP(r);
		return nf.send(opt, PUSH_LEN());
	}
}

template <class M, class S, class N>
void masternode::factory_impl<M,S,N>::on_mgr(SOCKMGR s, int t, char *p, int l)
{
	masternode::factory_impl<M,S,N> *nf =
			(masternode::factory_impl<M,S,N> *)nbr_sockmgr_get_data(s);
	if (nf == NULL) {
		return;
	}
	switch(t) {
	case super::nev_finder_query_master_init:
		nf->on_finder_query_init(true, p, l);
		break;
	case super::nev_finder_query_servant_init:
		nf->on_finder_query_init(false, p, l);
		break;
	case super::nev_finder_query_poll:
		nf->on_finder_query_poll(p, l);
		break;
	default:
		super::on_mgr(s, t, p, l);
		break;
	}
}

/* property */
template <class M, class S>
masternode::property<M,S>::property(BASE_CONFIG_PLIST, const char *sym,
	int multiplex, int master_port, int servant_port,
	const MC &master_conf, const SC &servant_conf) :
	node::property(BASE_CONFIG_CALL, sym, 1, multiplex),
	m_master_port(master_port), m_servant_port(servant_port),
	m_master_conf(master_conf), m_servant_conf(servant_conf)
{
}

template <class M, class S>
int masternode::property<M,S>::set(const char *k, const char *v)
{
	if (nbr_mem_cmp(k, "master.", sizeof("master.") - 1)) {
		return m_master_conf.set(k + sizeof("master.") - 1, v);
	}
	else if (nbr_mem_cmp(k, "servant.", sizeof("servant.") - 1)) {
		return m_master_conf.set(k + sizeof("servant.") - 1, v);
	}
	else {
		if (cmp(k, "master_port")) {
			SAFETY_ATOI(v, m_master_port, U16);
			nbr_str_printf(m_master_conf.m_host, "0.0.0.0:%hu", m_master_port);
			return NBR_OK;
		}
		else if (cmp(k, "servant_port")) {
			SAFETY_ATOI(v, m_servant_port, U16);
			nbr_str_printf(m_servant_conf.m_host, "0.0.0.0:%hu", m_servant_port);
			return NBR_OK;
		}
		return node::property::set(k, v);
	}
}

