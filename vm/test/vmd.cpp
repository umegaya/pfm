/***************************************************************
 * vmd.cpp : daemon work with various script languages
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
#include "vmd.h"

using namespace sfc;
using namespace sfc::vm;
/*-------------------------------------------------------------*/
/* sfc::vmd::vmdmaster 					       */
/*-------------------------------------------------------------*/
/* static variable */
map<vmd::vmdmstr::account_info,vmd::vmdmstr::protocol::login_id> 
	vmd::vmdmstr::m_lm;

/* methods */
int vmd::vmdmstr::init_login_map(int max_user)
{
	if (!m_lm.init(max_user, max_user, -1, opt_expandable | opt_threadsafe)) {
		return NBR_EMALLOC;
	}
	return NBR_OK;
}

int vmd::vmdmstr::recv_cmd_node_register(U32 msgid, const address &a)
{
	/* this node actually have node address 'a' */
	world::set_node(*this, a);
	log(INFO, "node_reg>A:%s,NA:%s\n", (const char *)addr(), (const char *)a);
	/* here temporary test code */
#if defined(_TEST)
	world_id wid = "test_MMO";
	cmd_add_node(0, wid, addr());
#endif
	return reply_node_register(*this, msgid, NBR_OK);
}

int vmd::vmdmstr::recv_cmd_node_ctrl(U32 msgid, const char *cmd,
		const world_id &wid, char *p, size_t l)
{
	int r;
	sr().unpack_start(p, l);
	if (strcmp("add", cmd) == 0) {
		data d;
		if (sr().unpack(d) <= 0) { r = NBR_EFORMAT; }
		else {
			r = cmd_add_node(msgid, wid, script::to_s(d));
		}
	}
	if (r < 0) {
		return reply_node_ctrl(*this, msgid, r, cmd, wid, p, l);
	}
	return NBR_OK;
}

int vmd::vmdmstr::cmd_add_node(U32 msgid, const world_id &wid, const address &a)
{
	int r;
	/* add node to world */
	world *w = super::create_world(wid);
	if (!w) { return NBR_ENOTFOUND; }
	vmdmstr *s = sf(*this)->pool().find(a);
	if (!s) { return NBR_ENOTFOUND; }
	if ((r = w->add_node(*s)) < 0) { return r; }
	/* pack current node list */
	size_t sz = w->nodes().size() * (sizeof(address) + sizeof(U32));
	char buffer[sz];
	sr().pack_start(buffer, sz);
	sr().push_raw((char *)&(w->world_object_uuid()), sizeof(UUID));
	world::nditer i = w->nodes().begin();
	for (; i != w->nodes().end(); i = w->nodes().next(i)) {
		if (!i->registered()) { continue; }
		if (sr().push_string(i->chnode()->iden,
				strlen(i->chnode()->iden)) < 0) {
			return NBR_ESHORT;
		}
		TRACE("address = <%s>\n", i->chnode()->iden);
	}
	return send_node_ctrl(*this, msgid, "add", wid, sr().p(), sr().len());
}

#define IS_ADD(cmd) (*cmd == 'a')
int vmd::vmdmstr::recv_code_node_ctrl(querydata &q,
		int r, const char *cmd, const world_id &wid, char *p, size_t l)
{
	char b[256];
	log(INFO, "node ctrl cmd = %s<%s:%s>\n", cmd, wid, chnode()->iden);
	world *w = object_factory::find_world(wid);
	if (!w) { ASSERT(false); return NBR_ENOTFOUND; }
	int rr = protocol::notify_node_change(*this, cmd, wid, chnode()->iden);
	r = rr < 0 ? rr : r;
	data d;
	sr().unpack_start(p, l);
	if (IS_ADD(cmd)) {
		UUID uuid = w->world_object_uuid();
		if (r < 0 || sr().unpack(d) <= 0) {
			w->del_node(*this);
			protocol::notify_node_change(*this, "del", wid, chnode()->iden);
		}
		else if (!protocol::is_valid_id(uuid)) {
			w->set_world_object_uuid(*(UUID *)script::to_p(d));
			log(INFO, "set world object %s\n",
					w->world_object_uuid().to_s(b, sizeof(b)));
		}
	}
	if (q.msgid != 0) {
		q.sender()->reply_node_ctrl(*this, q.msgid, r, cmd, wid, p, l);
	}
	return NBR_OK;
}

int
vmd::vmdmstr::recv_cmd_login(U32 msgid, const world_id &wid,
		const char *acc, char *authdata, size_t len)
{
	char b[256];
	/* TODO: do something before do this step
	 * authenticate acc with authdata & len */
	world *w = object_factory::find_world(wid);
	if (!w) {
		ASSERT(false);
		reply_login(*this, msgid, NBR_ENOTFOUND, wid, UUID(), "", 0);
		return NBR_OK;
	}
	UUID uuid = w->world_object_uuid();
	if (!protocol::is_valid_id(uuid)) {
		log(ERROR, "world(%s) not exist\n", wid);
		reply_login(*this, msgid, NBR_ENOTFOUND, wid, UUID(), "", 0);
		return NBR_OK;
	}
	account_info *pa = m_lm.create(acc);
	if (pa) {
		if (pa->login()) {
			/* already another player login */
			reply_login(*this, msgid, NBR_EALREADY, wid, pa->m_uuid, "", 0);
		}
		else {
			memcpy(pa->m_login_wid, wid, sizeof(wid));	/* login now */
			log(INFO, "login>A:%s,UUID:%s,W:%s\n",
					acc,pa->m_uuid.to_s(b,sizeof(b)),wid);
			reply_login(*this, msgid, NBR_OK, wid, pa->m_uuid, "", 0);
		}
	}
	else {
		reply_login(*this, msgid, NBR_EEXPIRE, wid,
				protocol::invalid_id(), (char*)"", 0);
	}
	return NBR_OK;
}


/*-------------------------------------------------------------*/
/* sfc::vmd::vmdservant                                        */
/*-------------------------------------------------------------*/
/* player object - session mapping */
map<address, vmd::vmdsvnt::UUID> vmd::vmdsvnt::m_pm;

int
vmd::vmdsvnt::init_player_map(int max_session)
{
	if (!m_pm.init(max_session, max_session, -1,
			opt_threadsafe | opt_expandable)) {
		return NBR_EMALLOC;
	}
	return NBR_OK;
}

int
vmd::vmdsvnt::recv_cmd_new_object(U32 msgid, const world_id &wid,
		UUID &uuid, char *p, size_t l)
{
	/* this function should be executed connector session.
	 * so m_session_uuid & m_wid is invalid basically. */
	int r;
#if defined(_DEBUG)
	char b[256];
	log(INFO, "svnt: new object: %u for <%s>\n",msgid, uuid.to_s(b, sizeof(b)));
#endif
	if (protocol::is_valid_id(m_session_uuid)) {
		ASSERT(false);
		return reply_new_object(*this, msgid, NBR_EINVAL, uuid, NULL, 0);
	}
	object *o = script::object_new(cf(), wid, NULL/*main VM*/, uuid, NULL, true);
	if (!o) {
		return reply_new_object(*this, msgid, NBR_EEXPIRE, uuid, NULL, 0);
	}
	/* if really newly created, then call initializer with parameter */
	if (o->create_new()) {
		proc_id pid = "init_object";
		if ((r = script::call_proc(*this, cf(), wid, msgid, *o, pid, p, l,
				rpct_global, vmprotocol::rpcopt_flag_not_set)) < 0) {
			return reply_new_object(*this, msgid, r, uuid, NULL, 0);
		}
	}
	/* restart pack */
	char buffer[64 * 1024];
	sr().pack_start(buffer, sizeof(buffer));
	/* pack with object value */
	if ((r = script::pack_object(sr(), *o, true)) < 0) {
		return reply_new_object(*this, msgid, r, uuid, NULL, 0);
	}
	/* TODO: send this object information to replicate host. */
	return reply_new_object(*this, msgid, NBR_OK, uuid, sr().p(), sr().len());
}

int
vmd::vmdsvnt::recv_code_new_object(
		querydata &q, int r, UUID &uuid, char *p, size_t l)
{
	char b[256];
	object *o;
#if defined(_DEBUG)
	log(INFO, "code_new_object: %d %s t=%u\n",
			r, uuid.to_s(b, sizeof(b)), q.m_data);
#endif
	switch(q.m_data) {
	case load_purpose_create:
		/* lua script resume the point attempt to create pfm object */
		sr().unpack_start(p, l);
		return script::resume_create(*(q.sender()), cf(), super::wid(),
			q.vm(), uuid, sr());
	case load_purpose_login:
		if (r < 0) {
			return q.sender()->reply_login(*this, q.msgid, r,
				super::wid(), uuid, p, l);
		}
		sr().unpack_start(p, l);
		/* when login, create local object */
		if ((o = script::object_new(cf(), super::wid(),
				NULL/*main VM*/, uuid, &(sr()), true))) {
			m_session_uuid = uuid;
			proc_id pid = "load_player";
			/* pid, "", 0 means no argument */
			r = script::call_proc(*this, cf(), super::wid(), q.msgid, *o, pid,
					"", 0, rpct_global, vmprotocol::rpcopt_flag_not_set);
			/* insert relation ship table of session - object */
			if (r >= 0 && m_pm.end() == m_pm.insert(addr(), uuid)) {
				ASSERT(false);
				r = NBR_EEXPIRE;
			}
			return q.sender()->reply_login(*this, q.msgid, r,
				super::wid(), uuid, p, l);
		}
		return q.sender()->reply_login(*this, q.msgid, NBR_EEXPIRE, 
			super::wid(), uuid, "", 0);
	case vmprotocol::load_purpose_create_world:
		q.wld().set_world_object_uuid(uuid);
		log(INFO, "world_create>object_id:%s\n", uuid.to_s(b, sizeof(b)));
		sr().pack_start(b, sizeof(b));
		sr().push_raw((char *)&uuid, sizeof(uuid));
		return backend_conn()->reply_node_ctrl(*this, q.msgid, NBR_OK, "add",
				q.wld().id(), sr().p(), sr().len());
	default:
		log(ERROR, "invalid purpose: %d\n", q.m_data);
		ASSERT(false);
	}
	return NBR_OK;
}

int
vmd::vmdsvnt::recv_cmd_login(U32 msgid, const world_id &wid,
		const char *acc, char *ath, size_t athl)
{
	/* just forward to master (with log) */
	log(INFO, "try_login>A:%s,W:%s\n",acc,wid);
	super::set_wid(wid);
	return backend_conn()->send_login(*this, msgid, wid, acc, ath, athl);
}

int
vmd::vmdsvnt::recv_code_login(querydata &q, int r, const world_id &wid,
		UUID &uuid, char *p, size_t l)
{
	char buf[256];
	log(INFO, "login>R:%d,UUID:%s,W:%s\n", r, uuid.to_s(buf,sizeof(buf)),wid);
	if (r < 0) {
		return q.sender()->reply_login(*this, q.msgid, r, wid, uuid, "", 0);
	}
	if (protocol::is_valid_id(uuid)) {
		sr().unpack_start(p, l);
		object *o = script::object_new(cf(), wid, NULL/*use main VM*/,
				uuid, &(sr()), false);
		if (!o) {
			return q.sender()->reply_login(*this,
					q.msgid, NBR_EINVAL, wid, uuid, "", 0);
		}
		return q.sender()->reply_login(*this, q.msgid, NBR_OK, wid, uuid, p, l);
	}
	else {
		querydata *dq;
		if ((r = create_object_with_type(&wid, "Player", sizeof("Player"),
			q.msgid, vmprotocol::load_purpose_login, &dq)) < 0) {
			return q.sender()->reply_login(*this, q.msgid, r, wid, uuid, "", 0);
		}
		return r;	/* waiting for creation reply */
	}
}

int
vmd::vmdsvnt::recv_notify_node_change(const char *cmd,
		const world_id &wid, const address &a)
{
	world *w = IS_ADD(cmd) ? create_world(wid) :
			object_factory::find_world(wid);
	if (!w) {
		ASSERT(false);
		return NBR_EEXPIRE;
	}
	vmdsvnt *s = cf().pool().create(a);
	TRACE("create for session object for %s(%p)\n", (const char*)a, s);
	if (!s) {
		ASSERT(false);
		return NBR_EEXPIRE;
	}
	if (s->registered()) {
		return NBR_OK;
	}
	world::set_node(*s, (const char *)a);
	return IS_ADD(cmd) ? w->add_node(*s) : w->del_node(*s);
}

int
vmd::vmdsvnt::recv_cmd_node_ctrl(U32 msgid, const char *cmd,
			const world_id &wid, char *p, size_t l)
{
	int r;
	log(ERROR, "node_ctrl>W:%s,CMD:%s\n", wid, cmd);
	if (!IS_ADD(cmd)) {
		return backend_conn()->reply_node_ctrl(
				*this, msgid, NBR_OK, cmd, wid, "", 0);
	}
	data d;
	sr().unpack_start(p, l);
	if (sr().unpack(d) <= 0) { return NBR_EFORMAT; }
	bool has_object = protocol::is_valid_id(*(UUID *)script::to_p(d));
	while(sr().unpack(d)) {
		r = recv_notify_node_change(cmd, wid, script::to_s(d));
		if (r < 0 && r != NBR_EALREADY) {
			ASSERT(false);
			return backend_conn()->reply_node_ctrl(
					*this, msgid, r, cmd, wid, "", 0);
		}
		log(INFO, "add node to consistent hash %s:%d\n", script::to_s(d), r);
	}
	if (!has_object) {
		world *w = object_factory::find_world(wid);
		if (!w) {
			ASSERT(false);
			return backend_conn()->reply_node_ctrl(
					*this, msgid, NBR_ENOTFOUND, cmd, wid, "", 0);
		}
		TRACE("this world (%s) has no world object so create now\n", wid);
		querydata *q;
		if ((r = create_object_with_type(&wid, "World", sizeof("World"),
			msgid, vmprotocol::load_purpose_create_world, &q)) < 0) {
			return backend_conn()->reply_node_ctrl(
					*this, msgid, r, cmd, wid, "", 0);
		}
		q->m_world = w;
		return NBR_OK;
	}
	return backend_conn()->reply_node_ctrl(
			*this, msgid, NBR_OK, cmd, wid, "", 0);
}



/*-------------------------------------------------------------*/
/* sfc::vmd::vmdclient                                         */
/*-------------------------------------------------------------*/
int
vmd::vmdclnt::recv_code_login(querydata &q, int r, const world_id &wid, 
		UUID &uuid, char *p, size_t l)
{
	if (r < 0) {
		log(ERROR, "login fail: %d\n", r);
		return r;
	}
	object_factory::world *w;
	if (!(w = create_world(wid))) {
		ASSERT(false);
		return NBR_EEXPIRE;
	}
	super::set_wid(wid);
	/* add node */
	w->set_node(*this, addr());
	w->add_node(*this);
	/* create remote object */
	sr().unpack_start(p, l);
	object *o = script::object_new(cf(), wid, NULL/*use main VM*/,
			uuid, &(sr()), false);
	if (o) {
//		o->set_connection(c);
		proc_id pid = "main";
		/* pid, "", 0 means no argument */
		script::call_proc(*this, cf(), super::wid(), q.msgid, *o, pid, "", 0,
				rpct_global, vmprotocol::rpcopt_flag_not_set);
		return NBR_OK;
	}
	ASSERT(false);
	return NBR_OK;
}


/*-------------------------------------------------------------*/
/* sfc::vmd						       */
/*-------------------------------------------------------------*/
factory *
vmd::create_factory(const char *sname)
{
	if (config::cmp(sname, "mstr")) {
		return new vmdmstr::factory;
	}
	if (config::cmp(sname, "svnt")) {
		return new vmdsvnt::factory;
	}
	if (config::cmp(sname, "clnt")) {
		return new vmdclnt::factory;
	}
	return NULL;
}

int
vmd::create_config(config *cl[], int sz)
{
	cl[0] = new vmdconfig (
			"mstr",
			"localhost:8000",
			10,
			60, opt_not_set,
			64 * 1024, 64 * 1024,
			0, 0,
			10000,	sizeof(vmdmstr::querydata),
			"TCP", "eth0",
			1 * 10 * 1000/* 10msec task span */,
			1/* after 1us, again try to connect */,
			kernel::INFO,
			nbr_sock_rparser_bin16,
			nbr_sock_send_bin16,
			config::cfg_flag_server,
			"lua", "", "tc", "",
			10000, 10, 10000, 10000,
			10000, vmprotocol::vnode_replicate_num
			);
	cl[1] = new vmdconfig (
			"svnt",
			"localhost:9000",
			10,
			60, opt_expandable,
			64 * 1024, 64 * 1024,
			1000 * 1000 * 1000, 3 * 1000 * 1000,
			10000,	sizeof(vmdsvnt::querydata),
			"TCP", "eth0",
			1 * 10 * 1000/* 10msec task span */,
			1/* after 1us, again try to connect */,
			kernel::INFO,
			nbr_sock_rparser_bin16,
			nbr_sock_send_bin16,
			config::cfg_flag_server,
			"lua", "", "tc", "",
			10000, 10, 10000, 10000,
			10000, vmprotocol::vnode_replicate_num
			);
	cl[2] = new vmdconfig (
			"clnt",
			"",
			10,
			60, opt_not_set,
			64 * 1024, 64 * 1024,
			1000 * 1000 * 1000, 3 * 1000 * 1000,
			10000,
			sizeof(vmdclnt::factory::connector::querydata),
			"TCP", "eth0",
			1 * 10 * 1000/* 10msec task span */,
			1/* after 1us, again try to connect */,
			kernel::INFO,
			nbr_sock_rparser_bin16,
			nbr_sock_send_bin16,
			config::cfg_flag_not_set,
			"lua", "", "tc", "",
			10000, 10, 10000, 10000,
			10000, vmprotocol::vnode_replicate_num
			);
	return 3;
}

int
vmd::boot(int argc, char *argv[])
{
	int r;
	vmdconfig *vc;
	vmdmstr::factory *mstr = find_factory<vmdmstr::factory>("mstr");
	if (mstr) {
		if ((vc = find_config<vmdconfig>("mstr"))) {
			if ((r = vmdmstr::init_vm(vc->m_max_object, vc->m_max_world,
					vc->m_rpc_entry, vc->m_rpc_ongoing)) < 0) {
				return r;
			}
			if ((r = vmdmstr::init_login_map(10000)) < 0) {
				return r;
			}
		}
		else {
			return NBR_ENOTFOUND;
		}
	}
	vmdsvnt::factory *svnt = find_factory<vmdsvnt::factory>("svnt");
	if (svnt) {
		if ((vc = find_config<vmdconfig>("svnt"))) {
			if ((r = vmdsvnt::init_vm(vc->m_max_object, vc->m_max_world,
					vc->m_rpc_entry, vc->m_rpc_ongoing)) < 0) {
				return r;
			}
			if ((r = vmdsvnt::init_player_map(vc->m_max_connection)) < 0) {
				return r;
			}
			if (!vmdsvnt::load("./scp/svt.lua")) {
				return NBR_ESYSCALL;
			}
			vmdsvnt::connector *c = svnt->backend_connect("127.0.0.1:8000");
			if (!c) { return NBR_EEXPIRE; }
			address a("127.0.0.1:9000");
			if (c->send_node_register(*(c->chain()->m_s), 0, a) < 0) {
				return NBR_ESEND;
			}
		}
		else {
			return NBR_EINVAL;
		}
	}
#if defined(_TEST)
	/* waiting for world register */
	int n_cnt = 0;
	while (n_cnt < 10) { n_cnt++; heartbeat(); }
#endif
	vmdclnt::factory *clnt = find_factory<vmdclnt::factory>("clnt");
	if (clnt) {
		if ((vc = find_config<vmdconfig>("clnt"))) {
			if ((r = vmdclnt::init_vm(vc->m_max_object, vc->m_max_world,
					vc->m_rpc_entry, vc->m_rpc_ongoing)) < 0) {
				return r;
			}
			if (!vmdclnt::load("./scp/clt.lua")) {
				return NBR_ESYSCALL;
			}
			vmdclnt::connector *c = clnt->backend_connect("127.0.0.1:9000");
			if (!c) { return NBR_EEXPIRE; }
			/* set connector factory */
			vmdclnt::set_cf(clnt);
			/* send login (you can send immediately :D)*/
			vmdclnt::world_id wid = "test_MMO";
			return c->send_login(*(c->chain()->m_s), 0, wid,
					"umegaya", "iyatomi", sizeof("iyatomi"));
		}
		return NBR_EINVAL;
	}
	return NBR_ENOTFOUND;
}

int
vmd::initlib(CONFIG &c) 
{
	return NBR_OK;
}

void
vmd::shutdown()
{
}


