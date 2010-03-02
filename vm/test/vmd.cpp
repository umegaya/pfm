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
vmd::vmdmstr::querydata *
vmd::vmdmstr::senddata(session &s, U32 msgid, char *p, int l)
{
	querydata *q = (querydata *)sf(*this)->insert_query(msgid);
	if (!q) {
		log(ERROR, "vmdmstr:senddata(%u) fail\n", msgid);
		ASSERT(false);
		return NULL;
	}
	if (send(p, l) >= 0) {
		q->sk = s.sk();
		q->s = &s;
		return q;
	}
	log(ERROR, "vmdmstr:send fail\n");
	sf(*this)->remove_query(msgid);
	return NULL;
}


int vmd::vmdmstr::add_conhash(const char *addr)
{
	CHNODE n;
	nbr_conhash_set_node(&n, addr, vnode_replicate_num);
	return nbr_conhash_add_node(m_ch, &n);
}

int vmd::vmdmstr::del_conhash(const char *addr)
{
	CHNODE n;
	nbr_conhash_set_node(&n, addr, vnode_replicate_num);
	return nbr_conhash_del_node(m_ch, &n);
}

const CHNODE *
vmd::vmdmstr::lookup_conhash(UUID &uuid)
{
	return nbr_conhash_lookup(m_ch, (char *)&uuid, sizeof(UUID));
}

int vmd::vmdmstr::load_or_create_object(U32 msgid, UUID &uuid, loadpurpose lp)
{
	bool retry_f = false;
	const CHNODE *n;
retry:
	if (!(n = nbr_conhash_lookup(m_ch, (char *)&uuid, sizeof(UUID)))) {
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
	querydata *q;
	if (s->send_new_object(*this, "", msgid, uuid, &q) >= 0) {
		/* success. store purpose data */
		q->m_data = lp;
		return NBR_OK;
	}
	return NBR_EEXPIRE;
}

int vmd::vmdmstr::recv_cmd_new_object(U32 msgid, const char *acc, UUID &uuid)
{
	int r = load_or_create_object(msgid, uuid, load_purpose_create);
	if (r < 0) {
		return reply_new_object(*this, msgid, r, acc, uuid, "", 0);
	}
	return NBR_OK;	/* waiting for creation */
}

int
vmd::vmdmstr::recv_code_new_object(querydata &q, int r, const char *acc,
		UUID &uuid, char *p, size_t l)
{
	switch(q.m_data) {
	case load_purpose_create:
		return q.sender()->reply_new_object(*this, q.msgid, r, acc, uuid, p, l);
	case load_purpose_login:
		if (r >= 0) {
			account_info *pa = m_lm.find(acc);
			ASSERT(pa->m_login == 0);
			/* TODO : m_login is atomic int, so no need to use atomic instruction?
			 * or need to set volatile? */
			if (pa->m_login) {
				q.sender()->reply_login(*this, q.msgid, NBR_EALREADY, uuid, "", 0);
				break;
			}
			pa->m_login = 1;	/* login now */
			pa->m_uuid = uuid;
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
	/* maybe do something before do this step
	 * authentication with authdata&len */
	account_info *pa = m_lm.create(acc);
	if (pa) {
		if (pa->m_login) {
			/* already another player login */
			reply_login(*this, msgid, NBR_EALREADY, pa->m_uuid, "", 0);
		}
		else {
			int r;
			/* not login now : load player object */
			if ((r = load_or_create_object(
				msgid, pa->m_uuid, load_purpose_login)) < 0) {
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
vmd::vmdsvnt::querydata *
vmd::vmdsvnt::senddata(session &s, U32 msgid, char *p, int l)
{
	querydata *q = (querydata *)sf(*this)->insert_query(msgid);
	if (!q) {
		log(ERROR, "vmdsvnt:senddata(%u) fail\n", msgid);
		ASSERT(false);
		return NULL;
	}
	if (send(p, l) >= 0) {
		q->sk = s.sk();
		q->s = &s;
		return q;
	}        
	log(ERROR, "vmdsnvt:send fail\n");
	sf(*this)->remove_query(msgid);
	return NULL;
}

int
vmd::vmdsvnt::recv_cmd_new_object(U32 msgid, const char *acc, UUID &uuid)
{
	/* its a real object */
	object *o = script::object_new(*this, NULL/* use main VM */, uuid, NULL);
	if (!o) {
		return reply_new_object(*this, msgid, NBR_EEXPIRE, acc, uuid, NULL, 0);
	}
	char buffer[64 * 1024];
	sr().pack_start(buffer, sizeof(buffer));
	int r;
	if ((r = script::pack_object(sr(), *o)) < 0) {
		return reply_new_object(*this, msgid, r, acc, uuid, NULL, 0);
	}
	return reply_new_object(*this, msgid, NBR_OK, acc, uuid, sr().p(), sr().len());
}

int
vmd::vmdsvnt::recv_code_new_object(querydata &q, int r, const char *acc,
		UUID &uuid, char *p, size_t l)
{
	/* lua script resume the point attempt to create pfm object */
	sr().unpack_start(p, l);
	script::resume_create(*(q.sender()), q.vm(), uuid, sr());
	return NBR_OK;
}

int
vmd::vmdsvnt::recv_cmd_login(U32 msgid,
		const char *acc, char *authdata, size_t len)
{
	/* just forward to master (with log) */
	log(INFO, "login attempt (%s:%u)\n", acc, len);
	return send_login(*this, msgid, acc, authdata, len);
}

int
vmd::vmdsvnt::recv_code_login(querydata &q, int r, UUID &uuid, char *p, size_t l)
{
	char buf[256];
	log(INFO, "login result (%s:%d)\n", uuid.to_s(buf,sizeof(buf)), r);
	if (r < 0) {
		return q.sender()->reply_login(*this, q.msgid, r, uuid, "", 0);
	}
	if (!protocol::is_valid_id(uuid)) {
		return q.sender()->reply_login(*this, q.msgid, NBR_EINVAL, uuid, "", 0);
	}
	else {
		sr().unpack_start(p, l);
		object *o = script::object_new(*this, NULL/* use main VM */, uuid, &(sr()));
		if (o) {
			m_session_uuid = uuid;
			return q.sender()->reply_login(*this, q.msgid, NBR_OK, uuid, p, l);
		}
		return q.sender()->reply_login(*this, q.msgid, NBR_EEXPIRE, uuid, "", 0);
	}
}


/*-------------------------------------------------------------*/
/* sfc::vmd::vmdclient                                         */
/*-------------------------------------------------------------*/
int
vmd::vmdclnt::on_open(const config &cfg)
{
	return send_login(*this, 0, "umegaya", "iyatomi", sizeof("iyatomi"));
}

vmd::vmdclnt::querydata *
vmd::vmdclnt::senddata(session &s, U32 msgid, char *p, int l)
{
	querydata *q = (querydata *)sf(*this)->insert_query(msgid);
	if (!q) { 
		log(ERROR, "vmdclnt:senddata(%u) fail\n", msgid);
		ASSERT(false); 
		return NULL;
	}
	if (send(p, l) >= 0) {
		q->sk = s.sk();
		q->s = &s;
		return q;
	}
	log(ERROR, "vmdclnt:send fail\n");
	sf(*this)->remove_query(msgid);
	return NULL;
}

int
vmd::vmdclnt::recv_code_login(querydata &q, int r, UUID &uuid, char *p, size_t l)
{
	if (r < 0) {
		log(ERROR, "login fail: %d\n", r);
		return r;
	}
	sr().unpack_start(p, l);
	object *o = script::object_new(*this, NULL/*use main VM*/, uuid, &(sr()));
	if (o) {
		proc_id pid = "main";
		script::call_proc(*this, q.msgid, *o, pid, p, l, rpct_rpc);
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
			"mstr",
			"localhost:9000",
			10,
			60, opt_expandable,
			64 * 1024, 64 * 1024,
			0, 0,
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
	cl[2] = new vmdconfig ("mstr",
			"",
			10,
			60, opt_not_set,
			64 * 1024, 64 * 1024,
			0, 0,
			10000,	sizeof(vmdclnt::querydata),
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
	vmdmstr::factory *mstr =
				find_factory<vmdmstr::factory>("mstr");
	if (mstr) {
		if ((vc = find_config<vmdconfig>("mstr"))) {
			return vmdmstr::init_vm(
				vc->m_max_object, vc->m_rpc_entry, vc->m_rpc_ongoing);
		}
		return NBR_ENOTFOUND;
	}
	vmdsvnt::factory *svnt =
			find_factory<vmdsvnt::factory>("svnt");
	if (svnt) {
		if ((vc = find_config<vmdconfig>("svnt"))) {
			if ((r = vmdsvnt::init_vm(
				vc->m_max_object, vc->m_rpc_entry, vc->m_rpc_ongoing)) < 0) {
				return r;
			}
			vmdsvnt::factory::connector *c =
					svnt->backend_connect("localhost:8000");
			if (!c) {
				return NBR_EEXPIRE;
			}
		}
		return NBR_EINVAL;
	}
	vmdclnt::factory *clnt = find_factory<vmdclnt::factory>("clnt");
	if (clnt) {
		if ((vc = find_config<vmdconfig>("clnt"))) {
			if ((r = vmdclnt::init_vm(
				vc->m_max_object, vc->m_rpc_entry, vc->m_rpc_ongoing)) < 0) {
				return r;
			}
			vmdclnt::factory::connector *c =
					clnt->backend_connect("localhost:9000");
			if (!c) {
				return NBR_EEXPIRE;
			}
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


