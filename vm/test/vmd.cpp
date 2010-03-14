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
/* sfc::vmd::vmdconfig					       */
/*-------------------------------------------------------------*/
int
vmd::vmdconfig::set(const char *k, const char *v)
{
	if (cmp("lang", k)) {
		strncpy(m_lang, v, MAX_VALUE_STR);
		return NBR_OK;
	}
	else if (cmp("langopt", k)) {
		strncpy(m_langopt, v, MAX_VALUE_STR);
		return NBR_OK;
	}
	else if (cmp("kvs", k)) {
		strncpy(m_kvs, v, MAX_VALUE_STR);
		return NBR_OK;
	}
	else if (cmp("kvsopt", k)) {
		strncpy(m_kvsopt, v, MAX_VALUE_STR);
		return NBR_OK;
	}
	else if (cmp("max_object", k)) {
		SAFETY_ATOI(v, m_max_object, U32);
	}
	else if (cmp("rpc_entry", k)) {
		SAFETY_ATOI(v, m_rpc_entry, U32);
	}
	else if (cmp("rpc_ongoing", k)) {
		SAFETY_ATOI(v, m_rpc_ongoing, U32);
	}
	return config::set(k, v);
}

vmd::vmdconfig::vmdconfig(BASE_CONFIG_PLIST,
		char *lang, char *lopt,
		char *kvs, char *kopt,
		int max_object, int rpc_entry, int rpc_ongoing)
	: config(BASE_CONFIG_CALL),
	  m_max_object(max_object), m_rpc_entry(rpc_entry),
	  m_rpc_ongoing(rpc_ongoing)
{
	strcpy(m_lang, lang);
	strcpy(m_langopt, lopt);
	strcpy(m_kvs, kvs);
	strcpy(m_kvsopt, kopt);
}



/*-------------------------------------------------------------*/
/* sfc::vmd::vmdmaster 					       */
/*-------------------------------------------------------------*/
/* static variable */
CONHASH 
	vmd::vmdmstr::m_ch = NULL;
map<vmd::vmdmstr::account_info,vmd::vmdmstr::protocol::login_id> 
	vmd::vmdmstr::m_lm;

/* methods */
int vmd::vmdmstr::init_conhash(int max_node, int max_replica)
{
	if (!(m_ch = nbr_conhash_init(NULL, max_node, max_replica))) {
		return NBR_EMALLOC;
	}
	return NBR_OK;
}

int vmd::vmdmstr::init_login_map(int max_user)
{
	if (!m_lm.init(max_user, max_user, opt_expandable | opt_threadsafe)) {
		return NBR_EMALLOC;
	}
	return NBR_OK;
}

int vmd::vmdmstr::add_conhash(const char *addr)
{
	nbr_conhash_set_node(&m_node, addr, vnode_replicate_num);
	return nbr_conhash_add_node(m_ch, &m_node);
}

int vmd::vmdmstr::del_conhash(const char *addr)
{
	return nbr_conhash_del_node(m_ch, &m_node);
}

const CHNODE *
vmd::vmdmstr::lookup_conhash(UUID &uuid)
{
	return nbr_conhash_lookup(m_ch, (char *)&uuid, sizeof(UUID));
}

int vmd::vmdmstr::load_or_create_object(U32 msgid,
		const char *acc, UUID &uuid, char *p, size_t l, loadpurpose lp)
{
	bool retry_f = false;
	const CHNODE *n;
retry:
	if (!(n = lookup_conhash(uuid))) {
		return NBR_ENOTFOUND;
	}
	vmdmstr *s = sf(*this)->pool().find(n->iden);
	if (!s) {
		del_conhash(n->iden);
		if (!retry_f) {
			retry_f = true;
			goto retry;
		}
		ASSERT(false);
		return NBR_EINVAL;
	}
	/* pack object address (temporary master address) */
	char buffer[64 * 1024];
	sr().pack_start(buffer, sizeof(buffer));
	sr().push_array_len(1);
	sr().push_array_len(1);
	sr().push_string(f()->ifaddr().a(), f()->ifaddr().len());
	/* send object create */
	querydata *q;
	if (s->send_new_object(*this, acc, msgid, uuid,
			sr().p(), sr().len(), p, l, &q) >= 0) {
		/* success. store purpose data */
		q->m_data = lp;
		return NBR_OK;
	}
	return NBR_EEXPIRE;
}

int vmd::vmdmstr::recv_cmd_new_object(U32 msgid, const char *acc,
		UUID &uuid, char *addr, size_t adrl, char *p, size_t l)
{
	int r = load_or_create_object(msgid, acc, uuid, p, l, load_purpose_create);
	if (r < 0) {
		return reply_new_object(*this, msgid, r, acc, uuid, "", 0);
	}
	return NBR_OK;	/* waiting for creation */
}

int
vmd::vmdmstr::recv_code_new_object(querydata &q, int r, const char *acc,
		UUID &uuid, char *p, size_t l)
{
	char b[256];
#if defined(_DEBUG)
	log(INFO, "code_new_object: %d %s %s dlen=%u\n",
			r, acc, uuid.to_s(b, sizeof(b)), l);
#endif
	switch(q.m_data) {
	case load_purpose_create:
		return q.sender()->reply_new_object(*this, q.msgid, r, acc, uuid, p, l);
	case load_purpose_login:
		if (r >= 0) {
			account_info *pa = m_lm.find(acc);
			ASSERT(pa && pa->m_login == 0);
			/* TODO : m_login is atomic int, so no need to use atomic instruction?
			 * or only need to set volatile? */
			if (!pa || pa->m_login) {
				q.sender()->reply_login(*this, q.msgid, NBR_EALREADY, uuid, "", 0);
				break;
			}
			pa->m_login = 1;	/* login now */
			pa->m_uuid = uuid;
			log(INFO, "login>A:%s,UUID:%s\n", acc, uuid.to_s(b, sizeof(b)));
		}
		return q.sender()->reply_login(*this, q.msgid, r, uuid, p, l);
	default:
		log(ERROR, "invalid purpose: %d\n", q.m_data);
		ASSERT(false);
	}
	return NBR_OK;
}

int
vmd::vmdmstr::recv_cmd_login(U32 msgid,
		const char *acc, char *authdata, size_t len)
{
	/* TODO: do something before do this step
	 * authenticate acc with authdata & len */
	account_info *pa = m_lm.create(acc);
	if (pa) {
		if (pa->m_login) {
			/* already another player login */
			reply_login(*this, msgid, NBR_EALREADY, pa->m_uuid, "", 0);
		}
		else {
			int r;
			/* if uuid is invalid */
			if (!protocol::is_valid_id(pa->m_uuid)) {
				pa->m_uuid = protocol::new_id();
#if defined(_DEBUG)
				char b[256];
				log(INFO, "create new_id : <%s> for <%s>\n",
						pa->m_uuid.to_s(b, sizeof(b)), acc);
#endif
			}
			/* not login now : load player object */
			if ((r = load_or_create_object(
				msgid, acc, pa->m_uuid, "", 0, load_purpose_login)) < 0) {
				reply_login(*this, msgid, r, pa->m_uuid, "", 0);
			}
		}
	}
	else {
		reply_login(*this, msgid, NBR_EEXPIRE, protocol::invalid_id(), (char*)"", 0);
	}
	return NBR_OK;
}


/*-------------------------------------------------------------*/
/* sfc::vmd::vmdservant                                        */
/*-------------------------------------------------------------*/
int
vmd::vmdsvnt::recv_cmd_new_object(U32 msgid, const char *acc,
		UUID &uuid, char *addr, size_t adrl, char *p, size_t l)
{
	int r;
#if defined(_DEBUG)
	char b[256];
	log(INFO, "svnt: new object: %u for <%s><%s>\n",
		msgid, acc, uuid.to_s(b, sizeof(b)));
#endif
	sr().unpack_start(addr, adrl);
	object *o = script::object_new(
			*(script::CF *)f(), NULL/* use main VM */, uuid, &(sr()), true);
	if (!o) {
		return reply_new_object(
				*this, msgid, NBR_EEXPIRE, acc, uuid, NULL, 0);
	}
	/* if really newly created, then call initializer with parameter */
	if (o->create_new()) {
		proc_id pid = "init_object";
		if ((r = script::call_proc(*this, cf(), msgid, *o, pid, p, l,
				rpct_global, vmprotocol::rpcopt_flag_not_set)) < 0) {
			return reply_new_object(
					*this, msgid, r, acc, uuid, NULL, 0);
		}
	}
	/* restart pack */
	char buffer[64 * 1024];
	sr().pack_start(buffer, sizeof(buffer));
	/* pack with object value */
	if ((r = script::pack_object(sr(), *o, true)) < 0) {
		return reply_new_object(
				*this, msgid, r, acc, uuid, NULL, 0);
	}
	return reply_new_object(
			*this, msgid, NBR_OK, acc, uuid, sr().p(), sr().len());
}

int
vmd::vmdsvnt::recv_code_new_object(querydata &q, int r, const char *acc,
		UUID &uuid, char *p, size_t l)
{
	/* lua script resume the point attempt to create pfm object */
	sr().unpack_start(p, l);
	script::resume_create(*(q.sender()), cf(), q.vm(), uuid, sr());
	return NBR_OK;
}

int
vmd::vmdsvnt::recv_cmd_login(U32 msgid,
		const char *acc, char *authdata, size_t len)
{
	/* just forward to master (with log) */
	log(INFO, "login_attempt>A:%s\n", acc);
	return backend_conn()->send_login(*this, msgid, acc, authdata, len);
}

int
vmd::vmdsvnt::recv_code_login(querydata &q, int r, UUID &uuid, char *p, size_t l)
{
	char buf[256];
	log(INFO, "login>R:%d,UUID:%s\n", r, uuid.to_s(buf,sizeof(buf)));
	if (r < 0) {
		return q.sender()->reply_login(*this, q.msgid, r, uuid, "", 0);
	}
	if (!protocol::is_valid_id(uuid)) {
		return q.sender()->reply_login(*this, q.msgid, NBR_EINVAL, uuid, "", 0);
	}
	else {
		sr().unpack_start(p, l);
		/* when login, create local object */
		object *o = script::object_new(cf(), NULL/*main VM*/, uuid, &(sr()), true);
		if (o) {
			m_session_uuid = uuid;
			proc_id pid = "load_player";
			/* pid, "", 0 means no argument */
			r = script::call_proc(*this, cf(), q.msgid, *o, pid, "", 0,
					rpct_global, vmprotocol::rpcopt_flag_not_set);
			return q.sender()->reply_login(*this, q.msgid, r, uuid, "", 0);
		}
		return q.sender()->reply_login(*this, q.msgid, NBR_EEXPIRE, uuid, "", 0);
	}
}


/*-------------------------------------------------------------*/
/* sfc::vmd::vmdclient                                         */
/*-------------------------------------------------------------*/
int
vmd::vmdclnt::recv_code_login(querydata &q, int r, UUID &uuid, char *p, size_t l)
{
	if (r < 0) {
		log(ERROR, "login fail: %d\n", r);
		return r;
	}
	ASSERT(l == 0);	/* client does not need any additional object data */
	/* create remote object */
	object *o = script::object_new(cf(), NULL/*use main VM*/, uuid, NULL, false);
	if (o) {
		ASSERT(!o->connection());
		/* client always use backend connection */
		o->set_connection(backend_conn());
		proc_id pid = "main";
		/* pid, "", 0 means no argument */
		script::call_proc(*this, cf(), q.msgid, *o, pid, "", 0,
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
			10000, 10000, 10000
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
			10000, 10000, 10000
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
			10000, 10000, 10000
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
			if ((r = vmdmstr::init_vm(
				vc->m_max_object, vc->m_rpc_entry, vc->m_rpc_ongoing)) < 0) {
				return r;
			}
			if ((r = vmdmstr::init_conhash(1000, 50)) < 0) {
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
			if ((r = vmdsvnt::init_vm(
				vc->m_max_object, vc->m_rpc_entry, vc->m_rpc_ongoing)) < 0) {
				return r;
			}
			if (!vmdsvnt::load("./scp/svt.lua")) {
				return NBR_ESYSCALL;
			}
			vmdsvnt::factory::connector *c =
					svnt->backend_connect("localhost:8000");
			if (!c) {
				return NBR_EEXPIRE;
			}
		}
		else {
			return NBR_EINVAL;
		}
	}
	vmdclnt::factory *clnt = find_factory<vmdclnt::factory>("clnt");
	if (clnt) {
		if ((vc = find_config<vmdconfig>("clnt"))) {
			if ((r = vmdclnt::init_vm(
				vc->m_max_object, vc->m_rpc_entry, vc->m_rpc_ongoing)) < 0) {
				return r;
			}
			if (!vmdclnt::load("./scp/clt.lua")) {
				return NBR_ESYSCALL;
			}
			vmdclnt::factory::connector *c =
					clnt->backend_connect("localhost:9000");
			if (!c) {
				return NBR_EEXPIRE;
			}
			/* send login (you can send immediately :D)*/
			return c->send_login(*(c->chain()->m_s), 0,
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


