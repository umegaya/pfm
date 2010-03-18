/***************************************************************
 * vmd.h: daemon build by sfc which run with script vm
 * 2010/02/15 iyatomi : create
 *                             Copyright (C) 2008-2010 Takehiro Iyatomi
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
/* sfc::vm::object_factory_impl				       */
/*-------------------------------------------------------------*/
template <class S, class IDG, class KVS>
map<typename object_factory_impl<S,IDG,KVS>::object, 
	typename object_factory_impl<S,IDG,KVS>::UUID>
object_factory_impl<S,IDG,KVS>::m_om;

template <class S, class IDG, class KVS>
map<typename object_factory_impl<S,IDG,KVS>::world,
	typename object_factory_impl<S,IDG,KVS>::world_id>
object_factory_impl<S,IDG,KVS>::m_wl;

/* object_factory_impl */
template <class S, class IDG, class KVS>
int object_factory_impl<S,IDG,KVS>::init(int max_object, int max_world)
{
	if (!m_om.init(max_object, max_object, -1,
		opt_threadsafe | opt_expandable)) {
		return NBR_EMALLOC;
	}
	if (!m_wl.init(max_world, max_world / 3, -1,
		opt_threadsafe | opt_expandable)) {
		return NBR_EMALLOC;
	}
	int r;
	if ((r = KVS::init(max_object)) < 0) {
		return r;
	}
	return NBR_OK;
}

template <class S, class IDG, class KVS>
void object_factory_impl<S,IDG,KVS>::fin()
{
	m_om.fin();
	m_wl.fin();
	KVS::fin();
}

/* world_impl */
template <class S, class IDG, class KVS>
template <class C>
int object_factory_impl<S,IDG,KVS>::world_impl<C>::init(
		int max_node, int max_replica)
{
	if (!(m_ch = nbr_conhash_init(NULL, max_node, max_replica))) {
		return NBR_EMALLOC;
	}
	if (!m_nodes.init(max_node, max_node / 10, -1, opt_expandable | opt_threadsafe)) {
		nbr_conhash_fin(m_ch);
		return NBR_EEXPIRE;
	}
	return NBR_OK;
}

template <class S, class IDG, class KVS>
template <class C>
typename object_factory_impl<S,IDG,KVS>::conn *
object_factory_impl<S,IDG,KVS>::world_impl<C>::
	connect_assigned_node(connector_factory &cf, const UUID &uuid)
{
	conn *c = NULL;
	bool retry_f = false;
	const CHNODE *n[vmprotocol::vmd_object_multiplexity];
	int n_node;
retry:
	if ((n_node = lookup_node(uuid, n,
		vmprotocol::vmd_object_multiplexity)) < 0) {
		ASSERT(false);
		return NULL;
	}
	for (int i = 0 ; i < n_node; i++) {
		if (!(c = cf.connect(uuid, n[i]->iden))) {
			del_node(*(n[i]));
			if (!retry_f) {
				retry_f = true;
				goto retry;
			}
			ASSERT(false);
			return NULL;
		}
		TRACE("object replicate addr <%s>\n", n[i]->iden);
	}
	return c;
}


template <class S, class IDG, class KVS>
template <class C>
int object_factory_impl<S,IDG,KVS>::world_impl<C>::add_node(const C &s)
{
	int r = add_node(*s.chnode());
	if (r < 0) {
		return r == NBR_EALREADY ? NBR_OK : r;
	}
	if (m_nodes.end() != m_nodes.insert(&s, s.chnode()->iden)) {
		return NBR_OK;
	}
	del_node(s);
	return NBR_EEXPIRE;
}
template <class S, class IDG, class KVS>
template <class C>
int object_factory_impl<S,IDG,KVS>::world_impl<C>::del_node(const C &s)
{
	m_nodes.erase(s.chnode()->iden);
	return del_node(*s.chnode());
}


/*-------------------------------------------------------------*/
/* sfc::vm::vmprotocol_impl
template <class S,class IDG,class SNDR>
vmdprotocol_impl<S,IDG,SNDR>::
  -------------------------------------------------------------*/
template <class S,class IDG,class SNDR>
int vmprotocol_impl<S,IDG,SNDR>::on_recv(char *p, int l)
{
#if defined(_DEBUG)
	_this().log(INFO, "vmproto: recv %u byte from %s\n", l,
			(const char *)_this().addr());
#endif
	U8 cmd, rt; U32 msgid;
	char account[vmprotocol::vmd_account_maxlen];
	char strcmd[vmprotocol::vmd_max_node_ctrl_cmd];
	int r, len;
	typename S::querydata *q = NULL;
	typename S::factory *sf = (typename S::factory *)_this().f();
	proc_id pid; world_id wid;
	address a;
	UUID uuid;
	POP_START(p, l);
	POP_8(cmd);
	POP_32(msgid);
	switch(cmd) {
	case vmprotocol::vmcmd_rpc:             /* c->s, c->c, s->s */
		POP_8(rt);
		len = sizeof(UUID);
		POP_8A(&(uuid), len);
		POP_STR(pid, sizeof(pid));
		/* execute fiber */
		_this().recv_cmd_rpc(msgid, uuid, pid, POP_BUF(), POP_REMAIN(), (rpctype)rt);
		break;
	case vmprotocol::vmcmd_new_object: {     /* s->m, m->s */
		POP_STR(wid, sizeof(wid));
		len = sizeof(UUID);
		POP_8A(&(uuid), len);
		_this().recv_cmd_new_object(msgid, wid, uuid, POP_BUF(), POP_REMAIN());
		} break;
	case vmprotocol::vmcmd_login:
		POP_STR(wid, sizeof(wid));
		POP_STR(account, sizeof(account));
		_this().recv_cmd_login(msgid, wid, account, POP_BUF(), POP_REMAIN());
		break;
	case vmprotocol::vmcmd_node_ctrl:		/* s->m */
		POP_STR(strcmd, sizeof(strcmd));
		POP_STR(wid, sizeof(wid));
		_this().recv_cmd_node_ctrl(msgid, strcmd, wid, POP_BUF(), POP_REMAIN());
		break;
	case vmprotocol::vmcmd_node_register:
		POP_ADDR(a);
		_this().recv_cmd_node_register(msgid, a);
		break;
	case vmprotocol::vmcode_rpc:            /* s->c, c->c, s->s */
		POP_8(rt);
		q = (typename S::querydata *)sf->find_query(msgid);
		if (!q || !nbr_sock_is_same(q->sk, q->s->sk())) { ASSERT(q); goto error; }
		_this().recv_code_rpc(*q, POP_BUF(), POP_REMAIN(), (rpctype)rt);
		break;
	case vmprotocol::vmcode_new_object:     /* s->m, m->s */
		q = (typename S::querydata *)sf->find_query(msgid);
		if (!q || !nbr_sock_is_same(q->sk, q->s->sk())) { ASSERT(q); goto error; }
		POP_32(r);
		len = sizeof(UUID);
		POP_8A(&(uuid), len);
		q->sender()->recv_code_new_object(*q, r, uuid, POP_BUF(), POP_REMAIN());
		break;
	case vmprotocol::vmcode_login:
		q = (typename S::querydata *)sf->find_query(msgid);
		if (!q || !nbr_sock_is_same(q->sk, q->sender()->sk())) {
			ASSERT(q); goto error; }
		POP_32(r);
		POP_STR(wid, sizeof(wid));
		len = sizeof(UUID);
		POP_8A(&(uuid), len);
		q->sender()->recv_code_login(*q, r, wid, uuid, POP_BUF(), POP_REMAIN());
		break;
	case vmprotocol::vmcode_node_ctrl:
		q = (typename S::querydata *)sf->find_query(msgid);
		if (!q || !nbr_sock_is_same(q->sk, q->sender()->sk())) {
			ASSERT(q); goto error; }
		POP_32(r);
		POP_STR(strcmd, sizeof(strcmd));
		POP_STR(wid, sizeof(wid));
		q->sender()->recv_code_node_ctrl(*q, r, strcmd, wid,
				POP_BUF(), POP_REMAIN());
		break;
	case vmprotocol::vmcode_node_register:
		q = (typename S::querydata *)sf->find_query(msgid);
		if (!q || !nbr_sock_is_same(q->sk, q->sender()->sk())) {
			ASSERT(q); goto error; }
		POP_32(r);
		q->sender()->recv_code_node_register(*q, r);
		break;
	case vmprotocol::vmnotify_node_change:	/* m->s */
		POP_STR(strcmd, sizeof(strcmd));
		POP_STR(wid, sizeof(wid));
		POP_ADDR(a);
		_this().recv_notify_node_change(strcmd, wid, a);
		break;
	default:
		_this().log(kernel::ERROR, "invalid command (%d) recved\n", cmd);
		ASSERT(false);
		break;
	}
error:
	if (q) { sf->remove_query(msgid); }
	return NBR_OK;
}

template <class S,class IDG,class SNDR>
template <class Q>
int vmprotocol_impl<S,IDG,SNDR>::send_rpc(
			SNDR &s, const UUID &uuid, const proc_id &p,
                        char *args, size_t alen, rpctype rt, Q **pq)
{
	size_t sz =
		2 * (alen + sizeof(proc_id) + sizeof(UUID) + sizeof(U32));
	char buf[sz];
	PUSH_START(buf, sz);
	PUSH_8(vmprotocol::vmcmd_rpc);
	U32 msgid = _this().f()->msgid();
	PUSH_32(msgid);
	PUSH_8(rt);
	PUSH_8A((U8 *)&uuid, sizeof(UUID));
	PUSH_STR(p);
#if defined(_DEBUG)
	static int rpc_c = 0;
	if (rt == vmprotocol::rpct_getter && strcmp(p, "echo") == 0) {
		rpc_c++;
		ASSERT(rpc_c <= 1);
	}
#endif
	PUSH_MEM(args, alen);
	/* instatiation of vmdmstr, here causes type error. 
	but actually vmdmstr never uses sendrpc call. so cast is ok
	(no effect) */
	Q *q = (Q *)_this().senddata(s, msgid, buf, PUSH_LEN());
	if (q) {
		if (pq) { *pq = q; }
		return PUSH_LEN();
	}
	return NBR_EEXPIRE;
}

template <class S,class IDG,class SNDR>
int vmprotocol_impl<S,IDG,SNDR>::reply_rpc(
                        SNDR &s, U32 rmsgid, char *args, size_t alen, rpctype rt)
{
	size_t sz = 2 * (1 + alen + sizeof(U32));
	char buf[sz];
	PUSH_START(buf, sz);
	PUSH_8(vmprotocol::vmcode_rpc);
	PUSH_32(rmsgid);
	PUSH_8(rt);
	PUSH_MEM(args, alen);
	return _this().send(buf, PUSH_LEN());
}

template <class S,class IDG,class SNDR>
template <class Q>
int vmprotocol_impl<S,IDG,SNDR>::send_new_object(SNDR &s, U32 rmsgid,
		const world_id &wid, const UUID &uuid, char *p, size_t pl, Q **pq)
{
	size_t sz = 
		2 * (1 + pl + vmprotocol::vmd_account_maxlen +
				sizeof(UUID) + sizeof(rmsgid));
	char buf[sz];
	PUSH_START(buf, sz);
	PUSH_8(vmprotocol::vmcmd_new_object);
	U32 msgid = _this().f()->msgid();
	PUSH_32(msgid);
	PUSH_STR(wid);
	PUSH_8A((char *)&uuid, sizeof(UUID));
	PUSH_MEM(p, pl);
	/* same as send_rpc's cast, it causes no effect */
	Q *q = (Q *)_this().senddata(s, msgid, buf, PUSH_LEN());
	if (q) {
		if (pq) { *pq = q; }
		q->msgid = rmsgid;
		return PUSH_LEN();
	}
	return  NBR_EEXPIRE;
}

template <class S,class IDG,class SNDR>
int vmprotocol_impl<S,IDG,SNDR>::reply_new_object(SNDR &s, U32 msgid,
		int r, UUID &uuid, char *p, size_t l)
{
	size_t sz =
		2 * (1 + sizeof(msgid) + sizeof(r) + sizeof(UUID) + l);
	char buf[sz];
	PUSH_START(buf, sz);
	PUSH_8(vmprotocol::vmcode_new_object);
	PUSH_32(msgid);
	PUSH_32(r);
	PUSH_8A(&uuid, sizeof(UUID));
	PUSH_MEM(p, l);
	return _this().send(buf, PUSH_LEN());
}

template <class S,class IDG,class SNDR>
int vmprotocol_impl<S,IDG,SNDR>::send_login(SNDR &s, U32 rmsgid,
		const world_id &wid, const char *acc, char *authdata, size_t len)
{
	size_t sz = 2 * (1 + vmprotocol::vmd_account_maxlen + len);
	char buf[sz];
	PUSH_START(buf, sz);
	PUSH_8(vmprotocol::vmcmd_login);
	U32 msgid = _this().f()->msgid();
	PUSH_32(msgid);
	PUSH_STR(wid);
	PUSH_STR(acc);
	PUSH_MEM(authdata, len);
	typename S::querydata *q = _this().senddata(s, msgid, buf, PUSH_LEN());
	if (q) {
		q->msgid = rmsgid;
		return PUSH_LEN();
	}
	return NBR_EEXPIRE;
}

template <class S,class IDG,class SNDR>
int vmprotocol_impl<S,IDG,SNDR>::reply_login(SNDR &s, U32 msgid,
		int r, const world_id &wid, const UUID &uuid, char *p, size_t l)
{
	size_t sz =
		2 * (1 + vmprotocol::vmd_account_maxlen + 
			sizeof(UUID) + sizeof(U32) + l);
	char buf[sz];
	PUSH_START(buf, sz);
	PUSH_8(vmprotocol::vmcode_login);
	PUSH_32(msgid);
	PUSH_32(r);
	PUSH_STR(wid);
	PUSH_8A(&uuid, sizeof(UUID));
	PUSH_MEM(p, l);
	return _this().send(buf, PUSH_LEN());
}

template <class S,class IDG,class SNDR>
int vmprotocol_impl<S,IDG,SNDR>::send_node_ctrl(SNDR &s,
		U32 rmsgid, const char *cmd, const world_id &wid, char *p, size_t pl)
{
	size_t sz =
		2 * (1 + sizeof(U32) + vmd_max_node_ctrl_cmd +
				sizeof(world_id) + sizeof(address) + pl);
	char buf[sz];
	PUSH_START(buf, sz);
	PUSH_8(vmprotocol::vmcmd_node_ctrl);
	U32 msgid = _this().f()->msgid();
	PUSH_32(msgid);
	PUSH_STR(cmd);
	PUSH_STR(wid);
	PUSH_MEM(p, pl);
	typename S::querydata *q = _this().senddata(s, msgid, buf, PUSH_LEN());
	if (q) {
		q->msgid = rmsgid;
		return PUSH_LEN();
	}
	return NBR_EEXPIRE;
}

template <class S,class IDG,class SNDR>
int vmprotocol_impl<S,IDG,SNDR>::reply_node_ctrl(SNDR &s, U32 msgid,
		int r, const char *cmd, const world_id &wid,
		char *p, size_t pl)
{
	size_t sz =
		2 * (1 + sizeof(U32) + vmd_max_node_ctrl_cmd +
				sizeof(world_id) + pl);
	char buf[sz];
	PUSH_START(buf, sz);
	PUSH_8(vmprotocol::vmcode_node_ctrl);
	PUSH_32(msgid);
	PUSH_32(r);
	PUSH_STR(cmd);
	PUSH_STR(wid);
	PUSH_MEM(p, pl);
	return _this().send(buf, PUSH_LEN());
}

template <class S,class IDG,class SNDR>
int vmprotocol_impl<S,IDG,SNDR>::send_node_register(SNDR &s,
		U32 rmsgid, const address &node_addr)
{
	size_t sz = 2 * (1 + sizeof(U32) + sizeof(address));
	char buf[sz];
	PUSH_START(buf, sz);
	PUSH_8(vmprotocol::vmcmd_node_register);
	U32 msgid = _this().f()->msgid();
	PUSH_32(msgid);
	PUSH_ADDR(node_addr);
	typename S::querydata *q = _this().senddata(s, msgid, buf, PUSH_LEN());
	if (q) {
		q->msgid = rmsgid;
		return PUSH_LEN();
	}
	return NBR_EEXPIRE;
}

template <class S,class IDG,class SNDR>
int vmprotocol_impl<S,IDG,SNDR>::reply_node_register(SNDR &s,
		U32 msgid, int r)
{
	size_t sz = 2 * (1 + sizeof(U32) + sizeof(U32));
	char buf[sz];
	PUSH_START(buf, sz);
	PUSH_8(vmprotocol::vmcode_node_register);
	PUSH_32(msgid);
	PUSH_32(r);
	return _this().send(buf, PUSH_LEN());
}

template <class S,class IDG,class SNDR>
int vmprotocol_impl<S,IDG,SNDR>::notify_node_change(SNDR &s,
		const char *cmd, const world_id &wid, const address &a)
{
	size_t sz =
		2 * (1 + sizeof(U32) + vmd_max_node_ctrl_cmd +
				sizeof(world_id) + sizeof(address));
	char buf[sz];
	PUSH_START(buf, sz);
	PUSH_8(vmprotocol::vmnotify_node_change);
	U32 msgid = 0;
	PUSH_32(msgid);
	PUSH_STR(cmd);
	PUSH_STR(wid);
	PUSH_ADDR(a);
	return _this().f()->broadcast(buf, PUSH_LEN());
}


/*-------------------------------------------------------------*/
/* sfc::vm::vmnode											   */
/*-------------------------------------------------------------*/
template <class S,template <class OF,class SR> class L,
	class KVS,class SR,class IDG,
	template <class S, class IDG, class KVS> class OF>
typename vm_impl<S,L,KVS,SR,IDG,OF>::connector_factory *
	vm_impl<S,L,KVS,SR,IDG,OF>::m_cf = NULL;

template <class S,template <class OF,class SR> class L,
	class KVS,class SR,class IDG,
	template <class S, class IDG, class KVS> class OF>
int vm_impl<S,L,KVS,SR,IDG,OF>::init_vm(
	int max_object, int max_world, int max_rpc, int max_rpc_ongoing)
{
	int r;
	if ((r = script::init(max_rpc, max_rpc_ongoing)) < 0) { return r; }
	if ((r = IDG::init()) < 0) { return r; }
	if ((r = object_factory::init(max_object, max_world)) < 0) { return r; }
	return r;
}

template <class S,template <class OF,class SR> class L,
	class KVS,class SR,class IDG,
	template <class S, class IDG, class KVS> class OF>
void vm_impl<S,L,KVS,SR,IDG,OF>::fin_vm()
{
	object_factory::fin();
	IDG::fin();
	script::fin();
}


/*-------------------------------------------------------------*/
/* sfc::vm::vmnode											   */
/*-------------------------------------------------------------*/
/* vmnode */
template <class S>
int vmnode<S>::recv_cmd_rpc(U32 msgid, UUID &uuid, proc_id &pid, 
		char *p, int l, rpctype rt)
{
	const UUID &vuuid = protocol::_this().verify_uuid(uuid);
	object *o = object_factory::find(vuuid);
	if (!o) {
		char buf[256];
		log(ERROR, "object not found (%s)\n", vuuid.to_s(buf, sizeof(buf)));
	}
	/* execute fiber */
	return script::call_proc(protocol::_this(), protocol::_this().cf(), wid(),
			msgid, *o, pid, p, (size_t)l, rt, protocol::rpcopt_flag_invoked);
}

template <class S>
int vmnode<S>::recv_code_rpc(querydata &q, char *p, size_t l, rpctype rt)
{
	/* resume fiber */
	return script::resume_proc(*(q.sender()), protocol::_this().cf(), wid(),
			q.vm(), p, l, rt);
}

template <class S>
template <class Q>
int vmnode<S>::create_object_with_type(const world_id *id,
		const char *type, size_t tlen, U32 msgid, loadpurpose lp, Q **pq)
{
	char b[tlen + sizeof(U32)];
	vmmodule::sr().pack_start(b, sizeof(b));
	vmmodule::sr().push_string(type, tlen);	/* object type. blessed */
	UUID uuid = protocol::new_id();
	return load_or_create_object(msgid, id, uuid,
			vmmodule::sr().p(), vmmodule::sr().len(), lp, pq);
}

template <class S>
template <class Q>
int vmnode<S>::load_or_create_object(U32 msgid, const world_id *id,
		UUID &uuid, char *p, size_t l, loadpurpose lp, Q **pq)
{
	world *w = object_factory::find_world(id ? *id : wid());
	connector *c;
	if (!w || !(c = w->connect_assigned_node(vmmodule::cf(), uuid))) {
		ASSERT(false);
		return NBR_ENOTFOUND;
	}
	/* send object create */
	Q *q;
	if (c->send_new_object(protocol::_this(), msgid, wid(), uuid, p, l, &q) >= 0) {
		/* success. store purpose data */
		q->m_data = lp;
		if (pq) { *pq = q; }
		return NBR_OK;
	}
	return NBR_EEXPIRE;
}

template <class S>
typename vmnode<S>::world *vmnode<S>::create_world(
		const world_id &wid) const
{
	world *w = object_factory::find_world(wid);
	if (!w) {
		if (!(w = object_factory::create_world(wid))) {
			ASSERT(false);
			return NULL;
		}
		w->set_id(wid);
		int r;
		const vmdconfig &vcfg = (const vmdconfig &)super::cfg();
		if ((r = w->init(vcfg.m_max_node, vcfg.m_max_replica)) < 0) {
			return NULL;
		}
	}
	return w;
}

template <class S> typename vmnode<S>::querydata *
vmnode<S>::senddata(S &s, U32 msgid, char *p, int l)
{
	querydata *q = (querydata *)sf(*((S *)this))->insert_query(msgid);
	if (!q) {
		log(ERROR, "vmnode:senddata(%u) fail\n", msgid);
		ASSERT(false);
		return NULL;
	}
	if (send(p, l) >= 0) {
		q->sk = s.sk();
		q->s = &s;
		return q;
	}
	log(ERROR, "vmnode:send fail\n");
	sf(*((S *)this))->remove_query(msgid);
	return NULL;
}


