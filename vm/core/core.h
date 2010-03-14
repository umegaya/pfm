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
int object_factory_impl<S,IDG,KVS>::init(int max_object)
{
	if (!m_om.init(max_object, max_object, 
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
	KVS::fin();
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
	int r, len;
	typename S::querydata *q = NULL;
	typename S::factory *sf = (typename S::factory *)_this().f();
	proc_id pid;
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
		POP_STR(account, sizeof(account));
		len = sizeof(UUID);
		POP_8A(&(uuid), len);
		char addr[l]; int adrl = l;
		POP_8A(addr, adrl);
		_this().recv_cmd_new_object(msgid, account, uuid, addr, adrl,
				POP_BUF(), POP_REMAIN());
		} break;
	case vmprotocol::vmcmd_login:
		POP_STR(account, sizeof(account));
		_this().recv_cmd_login(msgid, account, POP_BUF(), POP_REMAIN());
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
		POP_STR(account, sizeof(account));
		len = sizeof(UUID);
		POP_8A(&(uuid), len);
		if (r < 0) { 
			q->sender()->recv_code_new_object(*q, r, account, uuid, "", 0); }
		else {
			q->sender()->recv_code_new_object(*q, r, account, uuid, 
				POP_BUF(), POP_REMAIN());
		}
		break;
	case vmprotocol::vmcode_login:
		q = (typename S::querydata *)sf->find_query(msgid);
		if (!q || !nbr_sock_is_same(q->sk, q->sender()->sk())) {
			ASSERT(q); goto error; }
		POP_32(r);
		len = sizeof(UUID);
		POP_8A(&(uuid), len);
		q->sender()->recv_code_login(*q, r, uuid, POP_BUF(), POP_REMAIN());
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
int vmprotocol_impl<S,IDG,SNDR>::send_new_object(SNDR &s,
		const char *acc, U32 rmsgid, const UUID &uuid,
		char *addr, size_t adrl, char *p, size_t l,
		Q **pq)
{
	size_t sz = 
		2 * (1 + vmprotocol::vmd_account_maxlen + sizeof(UUID) + sizeof(rmsgid));
	char buf[sz];
	PUSH_START(buf, sz);
	PUSH_8(vmprotocol::vmcmd_new_object);
	U32 msgid = _this().f()->msgid();
	PUSH_32(msgid);
	PUSH_STR(acc);
	PUSH_8A((char *)&uuid, sizeof(UUID));
	PUSH_8A(addr, adrl);
	PUSH_MEM(p, l);
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
int vmprotocol_impl<S,IDG,SNDR>::reply_new_object(SNDR &s, U32 msgid, int r,
		const char *acc, UUID &uuid, char *p, size_t l)
{
	size_t sz =
		2 * (1 + sizeof(msgid) + sizeof(r) + sizeof(UUID) + l);
	char buf[sz];
	PUSH_START(buf, sz);
	PUSH_8(vmprotocol::vmcode_new_object);
	PUSH_32(msgid);
	PUSH_32(r);
	PUSH_STR(acc);
	PUSH_8A(&uuid, sizeof(UUID));
	PUSH_MEM(p, l);
	return _this().send(buf, PUSH_LEN());
}

template <class S,class IDG,class SNDR>
int vmprotocol_impl<S,IDG,SNDR>::send_login(SNDR &s, U32 rmsgid,
		const char *acc, char *authdata, size_t len)
{
	size_t sz = 2 * (1 + vmprotocol::vmd_account_maxlen + len);
	char buf[sz];
	PUSH_START(buf, sz);
	PUSH_8(vmprotocol::vmcmd_login);
	U32 msgid = _this().f()->msgid();
	PUSH_32(msgid);
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
		int r, const UUID &uuid, char *p, size_t l)
{
	size_t sz =
		2 * (1 + vmprotocol::vmd_account_maxlen + 
			sizeof(UUID) + sizeof(U32) + l);
	char buf[sz];
	PUSH_START(buf, sz);
	PUSH_8(vmprotocol::vmcode_login);
	PUSH_32(msgid);
	PUSH_32(r);
	PUSH_8A(&uuid, sizeof(UUID));
	PUSH_MEM(p, l);
	return _this().send(buf, PUSH_LEN());
}


/*-------------------------------------------------------------*/
/* sfc::vm::vmnode											   */
/*-------------------------------------------------------------*/
template <class S,template <class OF,class SR> class L,
	class KVS,class SR,class IDG,
	template <class S, class IDG, class KVS> class OF>
int vm_impl<S,L,KVS,SR,IDG,OF>::init_vm(
	int max_object, int max_rpc, int max_rpc_ongoing)
{
	int r;
	if ((r = script::init(max_rpc, max_rpc_ongoing)) < 0) { return r; }
	if ((r = IDG::init()) < 0) { return r; }
	if ((r = object_factory::init(max_object)) < 0) { return r; }
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
	return script::call_proc(protocol::_this(), protocol::_this().cf(),
			msgid, *o, pid, p, (size_t)l, rt, protocol::rpcopt_flag_invoked);
}

template <class S>
int vmnode<S>::recv_code_rpc(querydata &q, char *p, size_t l, rpctype rt)
{
	/* resume fiber */
	return script::resume_proc(*(q.sender()), protocol::_this().cf(),
			q.vm(), p, l, rt);
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
