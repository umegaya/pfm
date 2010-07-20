#include "finder.h"
#include "svnt.h"
#include "world.h"
#include "object.h"
#include "connector.h"
#include "cp.h"

using namespace pfm;

#if !defined(_TEST)
extern int custom_respond(pfm::fiber *f, bool err, pfm::serializer &sr)
{
	return ((pfm::svnt::fiber *)f)->respond(err, sr);
}
#else
extern int custom_respond(pfm::fiber *f, bool err, pfm::serializer &sr)
{
	return f->respond(err, sr);
}
#endif

namespace pfm {
namespace svnt {
/* finder */
class finder : public cluster::finder_session {
public:
	finder(SOCK s, base::factory *f) : cluster::finder_session(s,f) {}
	int on_recv(char *p, int l) {
		session::app().ff().recv(this, p, l, true);
		return NBR_OK;
	}
};
class finder_factory : public cluster::finder_factory {
	static const char *m_mstr_addr;
public:
	int init(const config &cfg) {
		return cluster::finder_factory::init(cfg, on_recv<finder>);
	}
	static void set_mstr_addr(const char *p) { m_mstr_addr = p; }
	static int node_inquiry_cb(serializer &sr) { 
		TRACE("node_inquery finish \n");
		return NBR_OK;
	}
	void poll(UTIME ut) {
		if (session::app().ff().wf().cf()->backend_enable()) {
			return;
		}
		if (m_mstr_addr) {
			session::app().ff().wf().cf()->backend_connect(address(m_mstr_addr));
			return;
		}
		serializer &sr = session::app().sr();
		PREPARE_PACK(sr);
		rpc::node_inquiry_request::pack_header(
			sr, session::app().ff().new_msgid(), servant_node);
		session::app().ff().run_fiber(node_inquiry_cb, sr.p(), sr.len());
	}
};

/* config */
class config : public util::config {
public:
	char m_dbmopt[256];
	const char* m_mstr_addr;
	typedef util::config super;
	config(BASE_CONFIG_PLIST, const char *dbmopt, const char *maddr);
	int set(const char *k, const char *v);
};

config::config(BASE_CONFIG_PLIST,
	const char *dbmopt, const char *mstr_addr) : super(BASE_CONFIG_CALL) {
	nbr_str_copy(m_dbmopt, sizeof(m_dbmopt), dbmopt, sizeof(m_dbmopt));
	m_mstr_addr = mstr_addr;
}
int config::set(const char *k, const char *v) {
	if (strcmp(k, "dbmopt") == 0) {
		nbr_str_copy(m_dbmopt, sizeof(m_dbmopt), v, sizeof (m_dbmopt));
		return NBR_OK;
	}
	if (strcmp(k, "master_addr") == 0) {
		m_mstr_addr = strndup(v, MAX_VALUE_STR);
		return NBR_OK;
	}
	return super::set(k, v);
}
}
pfms *svnt::session::m_daemon = NULL;
const char *svnt::finder_factory::m_mstr_addr = NULL;
#if defined(_DEBUG)
bool svnt::session::m_test_mode = true;
#else
bool svnt::session::m_test_mode = false;
#endif
}

base::factory *
pfms::create_factory(const char *sname)
{
	if (strcmp(sname, "clnt") == 0) {
		return new base::factory_impl<svnt::csession>;
	}
	if (strcmp(sname, "svnt") == 0) {
		return new base::factory_impl<svnt::session>;
	}
	if (strcmp(sname, "be") == 0) {
		return new base::factory_impl<svnt::besession>;
	}
	if (strcmp(sname, "finder") == 0) {
		return new svnt::finder_factory;
	}
	ASSERT(false);
	return NULL;
}

int
pfms::create_config(config* cl[], int size)
{
	CONF_START(cl);
	CONF_ADD(base::config, (
			"clnt",
			"0.0.0.0:8100",
			500,
			60, opt_not_set,
			64 * 1024, 64 * 1024,
			0, 0,
			-1,	0,
			"TCP", "eth0",
			1 * 100 * 1000/* 100msec task span */,
			1 * 1000 * 1000/* after 1s, again try to connect */,
			kernel::INFO,
			nbr_sock_rparser_bin16,
			nbr_sock_send_bin16,
			util::config::cfg_flag_server));
	CONF_ADD(svnt::config, (
			"be",
			"",
			10,
			60, opt_expandable,
			64 * 1024, 64 * 1024,
			100 * 1000 * 1000, 5 * 1000 * 1000,
			-1,	0,
			"TCP", "eth0",
			1 * 100 * 1000/* 100msec task span */,
			1 * 1000 * 1000/* after 1s, again try to connect */,
			kernel::INFO,
			nbr_sock_rparser_bin16,
			nbr_sock_send_bin16,
			util::config::cfg_flag_not_set, 
			"svnt/db",
			NULL));
	CONF_ADD(svnt::config, (
			"svnt",
			"0.0.0.0:8200",
			10,
			60, opt_expandable,
			64 * 1024, 64 * 1024,
			100 * 1000 * 1000, 5 * 1000 * 1000,
			-1,	0,
			"TCP", "eth0",
			1 * 100 * 1000/* 100msec task span */,
			1 * 1000 * 1000/* after 1s, again try to connect */,
			kernel::INFO,
			nbr_sock_rparser_bin16,
			nbr_sock_send_bin16,
			util::config::cfg_flag_server, 
			"svnt/db",
			""));
	CONF_ADD(cluster::finder_property, (
			"finder",
			"0.0.0.0:9999",
			10,
			30, opt_expandable,/* max 10 session/30sec timeout */
			256, 2048, /* send 256b,recv2kb */
			0, 0,/* no ping */
			-1,0,/* no query buffer */
			"UDP", "eth0",
			1 * 1000 * 1000/* every 10 sec, send probe command */,
			0/* never wait ld recovery */,
			kernel::INFO,
			nbr_sock_rparser_raw,
			nbr_sock_send_raw,
			config::cfg_flag_server,
			finder_property::MCAST_GROUP,
			8888, 1/* ttl = 1 */));
	CONF_END();
}

int
pfms::boot(int argc, char *argv[])
{
	svnt::session::m_daemon = this;
	if (argc > 1 && 0 == strncmp(argv[1], "--test=", sizeof("--test="))) {
		int tmode;
		SAFETY_ATOI(argv[1] + sizeof("--test="), tmode, int);
		svnt::session::m_test_mode = (tmode != 0);
	}
	int r;
	char path[256]; 
	conn_pool::svnt_cp *fc;
	base::factory_impl<svnt::session> *fc_svnt;
	svnt::finder_factory *fdr;
	svnt::config *c = find_config<svnt::config>("svnt");
	svnt::config *c_be = find_config<svnt::config>("be");
	if (!c || !c_be) { return NBR_ENOTFOUND; }
	INIT_OR_DIE((r = m_db.init(config::makepath(path, sizeof(path), c->m_dbmopt, "uuid.tch"))) < 0, r,
		"uuid DB init fail (%d)\n", r);
	INIT_OR_DIE((r = UUID::init(m_db)) < 0, r,
		"UUID init fail (%d)\n", r);
	INIT_OR_DIE((r = svnt::fiber::init_global(10000)) < 0, r,
		"svnt::fiber::init fails(%d)\n", r);
	INIT_OR_DIE((r = ff().init(100, 100, 10)) < 0, r,
		"fiber_factory init fails(%d)\n", r);
	INIT_OR_DIE(!(fc = find_factory<conn_pool::svnt_cp>("be")), NBR_ENOTFOUND,
		"conn_pool not found (%p)\n", fc);
	svnt::finder_factory::set_mstr_addr(c_be->m_mstr_addr);
	INIT_OR_DIE(!(fc_svnt = find_factory<conn_pool::svnt_cp>("svnt")), NBR_ENOTFOUND,
			"svnt factory not found (%p)\n", fc_svnt);
	INIT_OR_DIE(!(fdr = find_factory<svnt::finder_factory>("finder")), NBR_ENOTFOUND,
		"conn_pool not found (%p)\n", fdr);
	ff().cp()->set_pool(fc);
	INIT_OR_DIE((r = ff().wf().cf()->init(
		ff().cp(), fc_svnt->ifaddr(), 100, 100, 100)) < 0, r,
		"init connector factory fails (%d)\n", r);
	ff().set_finder(fdr);
	INIT_OR_DIE((r = ff().of().init(10000, 1000, 0, 
		config::makepath(path, sizeof(path), c->m_dbmopt, "of.tch"))) < 0, r,
		"object factory creation fail (%d)\n", r);
	INIT_OR_DIE((r = ff().wf().init(
		256, 256, opt_threadsafe | opt_expandable, 
		config::makepath(path, sizeof(path), c->m_dbmopt, "wf.tch"))) < 0, r,
		"object factory creation fail (%d)\n", r);
	return NBR_OK;
}

void
pfms::shutdown()
{
	UUID::fin(m_db);
	m_db.fin();
	svnt::fiber::fin_global();
	ff().wf().cf()->fin();
	ff().of().fin();
	ff().wf().fin();
	ff().fin();
}

/* fiber logic (servant mode) */
map<svnt::fiber::csession_entry, UUID> svnt::fiber::m_sm;

int svnt::fiber::quorum_vote_commit(MSGID msgid,
		quorum_context *ctx, serializer &sr)
{
	return super::thrd_quorum_vote_commit(msgid, ctx, sr);
}
svnt::fiber::quorum_context *svnt::fiber::init_context(world *w)
{
	return super::thrd_quorum_init_context(w, NULL);
}

int svnt::fiber::quorum_global_commit(world *w, quorum_context *ctx, int result)
{
	return super::thrd_quorum_global_commit(w, ctx, result);
}

int svnt::fiber::quorum_vote_callback(rpc::response &r, THREAD t, void *p)
{
	return super::thrd_quorum_vote_callback(r, t, p);
}

int svnt::fiber::call_ll_exec_client(rpc::request &req,
		object *o, bool trusted, char *p, int l)
{
	int r;
	MSGID msgid = INVALID_MSGID;
	TRACE("call_ll_exec_client %u byte\n", l);
	csession *cs = find_session(o->uuid());
	if (!cs) { r = NBR_ENOTFOUND; goto error; }
	if ((msgid = new_msgid()) == INVALID_MSGID) {
		r = NBR_EEXPIRE;
		goto error;
	}
	TRACE("new assigned msgid = %u, climsgid = %u\n", msgid, req.msgid());
	if ((r = yielding(msgid)) < 0) {
		goto error;
	}
	m_ctx.climsgid = req.msgid();
	/* TODO : remove magic? but its super faster... */
	rpc::request::replace_msgid(msgid, p, l);
	if ((r = cs->send(p, l)) < 0) {
		goto error;
	}	
	return r;
error:
	if (msgid != INVALID_MSGID) { ff().fiber_unregister(msgid); }
	if (r < 0) { send_error(r); }
	return r;
}

int svnt::fiber::call_login(rpc::request &p)
{
	int r;
	world *w;
	MSGID msgid = INVALID_MSGID;
	csession *c;
	rpc::login_request &req = rpc::login_request::cast(p);
	switch(m_status) {
	case start:
		if (!(w = ff().find_world(req.wid()))) {
			r = NBR_ENOTFOUND;
			goto error;
		}
		if (!(c = get_client_conn())) {
			r = NBR_EINVAL;
			goto error;
		}
		if ((r = c->set_account_info(req.wid(), 
			req.account(), req.authdata(), req.authdata().len())) < 0) {
			goto error;
		}
		LOG("login attempt : %s/%u\n",
			(const char *)req.account(), req.authdata().len());
		msgid = new_msgid();
		if (msgid == INVALID_MSGID) {
			r = NBR_EEXPIRE;
			goto error;
		}
		if ((r = login_request::pack_header(ff().sr(), msgid,
			req.wid(), req.wid().len(), req.account(),
			req.authdata(), req.authdata().len())) < 0) {
			goto error;
		}
		if ((r = yielding(msgid)) < 0) {
			goto error;
		}
		if ((r = w->cf().backend_conn()->send(
			msgid, ff().sr().p(), ff().sr().len())) < 0) {
			goto error;
		}
		break;
	default:
		ASSERT(false);
		r = NBR_EINVAL;
		goto error;
	}
	return NBR_OK;
error:
	if (msgid != INVALID_MSGID) { ff().fiber_unregister(msgid); }
	if (r < 0) {
		send_error(r);
	}
	return r;
}

int svnt::fiber::resume_login(rpc::response &p)
{
	int r;
	MSGID msgid = INVALID_MSGID;
	world *w;
	csession *c;
	ll::coroutine *co = NULL;
	if (!(w = ff().find_world(wid()))) {
		r = NBR_ENOTFOUND;
		goto error;
	}
	if (!(c = get_client_conn())) {
		r = NBR_EINVAL;
		goto error;
	}
	if (p.success()) {
		switch(m_status) {
		case start: {
			rpc::login_response &res = rpc::login_response::cast(p);
			c->set_uuid(res.object_id());
			msgid = new_msgid();
			if (msgid == INVALID_MSGID) {
				r = NBR_EEXPIRE;
				goto error;
			}
			if ((r = rpc::authentication_request::pack_header(
				ff().sr(), msgid, w->id(), nbr_str_length(w->id(), max_wid),
				c->account(), c->authdata(), c->alen())) < 0) {
				goto error;
			}
			if ((r = yielding(msgid)) < 0) {
				goto error;
			}
			if ((r = ff().run_fiber(this, ff().sr().p(), ff().sr().len())) < 0) {
				goto error;
			}
			m_status = login_authentication;
		} break;
		case login_authentication: {
			if ((r = svnt::fiber::register_session(c, c->player_id())) < 0) {
				goto error;
			}
			msgid = new_msgid();
			if (msgid == INVALID_MSGID) {
				r = NBR_EEXPIRE;
				goto error;
			}
			if ((r = rpc::create_object_request::pack_header(
				ff().sr(), msgid, c->player_id(),
				ll::player_klass_name, ll::player_klass_name_len,
				w->id(), nbr_str_length(w->id(), max_wid),
				(const char *)c->addr(), 0)) < 0) {
				goto error;
			}
			if ((r = yielding(msgid)) < 0) {
				goto error;
			}
			if ((r = w->request(msgid, c->player_id(), ff().sr())) < 0) {
				goto error;
			}
			m_status = login_wait_object_create;
		} break;
		case login_wait_object_create: {
			object *o;
			char b[256];
			TRACE("login : object create %s\n", c->player_id().to_s(b, sizeof(b)));
			if (!(o = ff().of().find(c->player_id()))) {
				rpc::ll_exec_response &res = rpc::ll_exec_response::cast(p);
				if (!(co = ff().co_create(this))) {
					r = NBR_EEXPIRE;
					goto error;
				}
				if ((r = co->to_stack(res)) < 0) {
					goto error;
				}
				ff().vm()->co_destroy(co);
				co = NULL;
				if (!(o = ff().of().find(c->player_id()))) {
					goto error;
				}
			}
			msgid = new_msgid();
			if (msgid == INVALID_MSGID) {
				r = NBR_EEXPIRE;
				goto error;
			}
			if ((r = rpc::ll_exec_request::pack_header(
				ff().sr(), msgid, *o,
				ll::enter_world_proc_name, ll::enter_world_proc_name_len,
				w->id(), nbr_str_length(w->id(), max_wid), rpc::ll_exec, 0)) < 0) {
				goto error;
			}
			if ((r = yielding(msgid)) < 0) {
				goto error;
			}
			if ((r = ff().run_fiber(this, o->vm()->attached_thrd(), 
				ff().sr().p(), ff().sr().len())) < 0) {
				goto error;
			}
			m_status = login_enter_world;
		} break;
		case login_enter_world: {
			object *o;
			if (!(o = ff().of().find(c->player_id()))) {
				r = NBR_ENOTFOUND;
				goto error;
			}
			rpc::response::pack_header(ff().sr(), m_msgid);
//			ff().sr().push_raw((char *)&(o->uuid()), sizeof(UUID));
			rpc::world_request::pack_object(ff().sr(), *o);
			ff().sr().pushnil();
			if ((r = respond(false, ff().sr())) < 0) {
				goto error;
			}
		} break;
		default:
			ASSERT(false);
			r = NBR_EINVAL;
			goto error;
		}
		return NBR_OK;
	}
	else {
		rpc::response::pack_header(ff().sr(), m_msgid);
		ff().sr().pushnil();
		switch(p.err().type()) {
		case datatype::BLOB: {
			ff().sr().push_string(p.err(), p.err().len());
		} break;
		case datatype::INTEGER: {
			ff().sr() << (int)p.err();
		} break;
		default:
			ASSERT(false);
			r = NBR_EINVAL;
			goto error;
		}
		if ((r = respond(true, ff().sr())) < 0) { ASSERT(false); }
	}
error:
	if (msgid != INVALID_MSGID) { ff().fiber_unregister(msgid); }
	if (r < 0) { send_error(r); }
	if (co) { ff().vm()->co_destroy(co); }
	if (c) {
		if (c->valid()) { c->close(); }
		if (c->player_id().valid()) {
			svnt::fiber::unregister_session(c->player_id());
		}
	}
	return r;
}

int svnt::fiber::call_logout(rpc::request &rq)
{
	int r;
	rpc::logout_request &req = rpc::logout_request::cast(rq);
	MSGID msgid = new_msgid();
	if (msgid == INVALID_MSGID) {
		r = NBR_EEXPIRE;
		goto error;
	}
	if ((r = logout_request::pack_header(ff().sr(), msgid,
		req.wid(), req.wid().len(), req.account())) < 0) {
		goto error;
	}
	if ((r = yielding(msgid)) < 0) {
		goto error;
	}
	if ((r = ff().wf().cf()->backend_conn()->send(
		msgid, ff().sr().p(), ff().sr().len())) < 0) {
		goto error;
	}
	return NBR_OK;
error:
	if (r < 0) { send_error(r); }
	return r;
}

int svnt::fiber::resume_logout(rpc::response &res)
{
	int r;
	if (res.success()) {
		/* TODO : call Player:logout callback */
		serializer &sr = ff().sr();
		rpc::response::pack_header(sr, m_msgid);
		sr << NBR_OK;
		sr.pushnil();
		if ((r = respond(false, sr)) < 0) {
			goto error;
		}
		return NBR_OK;
	}
	else {
		r = res.err();
		goto error;
	}
error:
	if (r < 0) { send_error(r); }
	return r;
}


int svnt::fiber::call_replicate(rpc::request &rq, char *p, int l)
{
	int r;
	char b[256];
	ll::coroutine *co = NULL;
	MSGID msgid = INVALID_MSGID;
	rpc::replicate_request &req = rpc::replicate_request::cast(rq);
	const UUID &uuid = req.uuid();
	m_ctx.nctrl.cmd = (U32)MAKE_REPL_CMD(req.method(), req.type());
	switch(m_ctx.nctrl.cmd) {
	case MAKE_REPL_CMD(rpc::replicate, replicate_move_to):
		LOG("rehash %s replicate move_to\n", uuid.to_s(b, sizeof(b)));
		/* object should move by rehash */ {
		if (!(co = ff().co_create(this))) {
			r = NBR_EEXPIRE; goto error;
		}
		if ((r = co->call(req)) < 0) {
			LOG("rehash %s replicate call fails (%d)",
				uuid.to_s(b, sizeof(b)), r);
			goto error;
		}
		ff().vm()->co_destroy(co);
		co = NULL;
		serializer &sr = ff().sr();
		response::pack_header(sr, m_msgid);
		sr.push_raw((char *)&uuid, sizeof(UUID));
		sr.pushnil();
		if ((r = respond(false, sr)) < 0) {
			LOG("rehash %s respond fails (%d)",
				uuid.to_s(b, sizeof(b)), r);
			goto error;
		}
		return NBR_OK;
	} break;
	case MAKE_REPL_CMD(rpc::start_replicate, replicate_move_to):
		/* start 1 rehash replicate */ {
		LOG("rehash start %s/%s\n", m_wid, uuid.to_s(b, sizeof(b)));
		ASSERT(uuid.id2 < 0x00700000);
		world *w = ff().find_world(m_wid);
		if (!w) { r = NBR_ENOTFOUND; goto error; }
		if ((msgid = new_msgid()) == INVALID_MSGID) {
			r = NBR_EEXPIRE;
			goto error;
		}
		serializer &sr = ff().sr();
		rpc::request::replace_msgid(msgid, p, l);
		rpc::request::replace_method(replicate, p, l);
		sr.pack_start(p, l);
		sr.set_curpos(l);
		if ((r = w->request(msgid, uuid, sr, true)) < 0) { goto error; }
		if ((r = yielding(msgid)) < 0) { goto error; }
		return NBR_OK;
	} break;
	case MAKE_REPL_CMD(rpc::replicate, replicate_normal):
		/* normal replication (apply update) */
	case MAKE_REPL_CMD(rpc::start_replicate, replicate_normal):
		/* start 1 normal replicate */
	default:
		ASSERT(false);
		r = NBR_EINVAL;
		goto error;
	}
error:
ASSERT(false);
	if (co) { ff().vm()->co_destroy(co); }
	if (msgid != INVALID_MSGID) { ff().fiber_unregister(msgid); }
	if (r < 0) { send_error(r); }
	return r;
}

int svnt::fiber::resume_replicate(rpc::response &re)
{
	char b[256];
	int r;
	rpc::replicate_response &res = rpc::replicate_response::cast(re);
	const UUID &uuid = res.uuid();
	switch(m_ctx.nctrl.cmd) {
	case MAKE_REPL_CMD(rpc::replicate, replicate_move_to): {
	} break;
	case MAKE_REPL_CMD(rpc::start_replicate, replicate_move_to): {
		TRACE("resume rehash for %s\n", uuid.to_s(b, sizeof(b)));
		world *w = ff().find_world(m_wid);
		if (!w) { ASSERT(false); r = NBR_ENOTFOUND; goto error; }
		object *o = ff().of().find(uuid);
		ASSERT(o);
		if (res.success()) {
			bool node_for = w->node_for(uuid);
			if (o) {
				o->set_flag(object::flag_local, false);
				o->set_flag(object::flag_replica, node_for);
				LOG("rehash %s to %s\n", uuid.to_s(b, sizeof(b)),
						node_for ? "replica" : "not used");
			}
		}
		else {
			LOG("rehash %s fails!", uuid.to_s(b, sizeof(b)));
			ASSERT(false);
		}
		TRACE("rehash resume thrd: %p %p\n",
			nbr_thread_get_current(), w->rh().curr());
		w->rh().resume();	/* do next move to */
		return NBR_OK;
	} break;
	case MAKE_REPL_CMD(rpc::replicate, replicate_normal):
		/* normal replication (apply update) */
	case MAKE_REPL_CMD(rpc::start_replicate, replicate_normal):
		/* start 1 normal replicate */
	default:
		ASSERT(false);
		r = NBR_EINVAL;
		goto error;
	}
error:
	return r;
}


int svnt::fiber::call_node_inquiry(rpc::request &rq)
{
	int r;
	rpc::node_inquiry_request &req = rpc::node_inquiry_request::cast(rq);
	MSGID msgid = new_msgid();
	if (msgid == INVALID_MSGID) {
		r = NBR_EEXPIRE;
		goto error;
	}
	if ((r = rpc::node_inquiry_request::pack_header(
		ff().sr(), msgid, req.node_type())) < 0) {
		goto error;
	}
	if ((r = yielding(msgid)) < 0) {
		goto error;
	}
	if ((r = ff().finder().send(ff().sr().p(), ff().sr().len())) < 0) {
		goto error;
	}
	return NBR_OK;
error:
	if (msgid != INVALID_MSGID) { ff().fiber_unregister(msgid); }
	if (r < 0) {
		send_error(r);
	}
	return r;
}

int svnt::fiber::resume_node_inquiry(rpc::response &rs)
{
	int r;
	connector *ct;
	if (rs.success()) {
		rpc::node_inquiry_response &res = rpc::node_inquiry_response::cast(rs);
		if ((ct = ff().wf().cf()->backend_connect(address(res.node_addr()))) < 0) {
			r = NBR_EEXPIRE;
			LOG("node_inquiry : connect(%s) fail (%d)\n",
				(const char *)res.node_addr(), r);
		}
		LOG("node_inquiry : connect to (%s)\n", (const char *)res.node_addr());
		return NBR_OK;
	}
	else {
		r = rs.err();
	}
	return r;
}

int svnt::fiber::node_ctrl_add(world *w,
		rpc::node_ctrl_cmd::add &req, serializer &sr)
{
	int r;
	MSGID msgid = INVALID_MSGID;
	quorum_context *ctx = NULL;
	bool world_exist = true;
	switch(m_status) {
	case start:
		if (!w) {
			world_exist = false;
			if (!req.world_object_id().valid()) {
				r = NBR_EINVAL;
				goto error;
			}
			/* servant never preserve world information */
			if (!(w = ff().world_create(req, false))) {
				r = NBR_EEXPIRE;
				goto error;
			}
			m_wid = w->id();
			if (!(ctx = init_context(w))) {
				r = NBR_EMALLOC;
				goto error;
			}
			ctx->m_node_addr.from(req.node_addr(), req.node_addr().len());
			msgid = new_msgid();
			if (msgid == INVALID_MSGID) {
				r = NBR_EEXPIRE;
				ff().world_destroy(w);
				goto error;
			}
			if ((r = node_ctrl_cmd::vm_init::pack_header(ff().sr(), msgid,
				req.wid(), req.wid().len(), req.from(), req.srcfile())) < 0) {
				ff().world_destroy(w);
				goto error;
			}
			if ((r = quorum_vote_commit(msgid, ctx, ff().sr())) < 0) {
				goto error;
			}
		}
		else {
			if (!(ctx = init_context(w))) {
				ASSERT(false);
				r = NBR_EMALLOC;
				goto error;
			}
			ctx->m_node_addr.from(req.node_addr(), req.node_addr().len());
			/* because no quorum used */
			ctx->m_rep_size = 0;
			if (!w->add_node(ctx->m_node_addr)) {
				r = NBR_EEXPIRE;
				goto error;
			}
			msgid = new_msgid();
			if (msgid == INVALID_MSGID) {
				r = NBR_EEXPIRE;
				ff().world_destroy(w);
				goto error;
			}
			if ((r = yielding(msgid, 1, NULL, ctx)) < 0) {
				goto error;
			}
			rpc::response::pack_header(sr, m_msgid);
			sr << msgid;
			sr.pushnil();
			if ((r = respond(false, sr)) < 0) {
				goto error;
			}
			m_status = ncc_wait_global_commit;
		}
		break;
	default:
		ASSERT(false);
		r = NBR_EINVAL;
		goto error;
	}
	return NBR_OK;
error:
	if (msgid != INVALID_MSGID) { ff().fiber_unregister(msgid); }
	if (ctx) { ff().quorums().erase(w->id()); }
	if (world_exist && ctx) {
		w->del_node(ctx->m_node_addr);
	}
	else if (w) {
		ff().wf().destroy(w->id());
	}
	return r;
}
int svnt::fiber::node_ctrl_add_resume(world *w, rpc::response &res, serializer &sr)
{
	int r;
	rehasher *rh;
	MSGID msgid = INVALID_MSGID;
	bool add_master = false;
	quorum_context *ctx = yld()->p<quorum_context>();
	if (res.success()) {
		switch(m_status) {
		case start:
			if (!w) {
				r = NBR_ENOTFOUND;
				goto error;
			}
			msgid = new_msgid();
			if (msgid == INVALID_MSGID) {
				r = NBR_EEXPIRE;
				goto error;
			}
			if ((r = yielding(msgid, 1, NULL, ctx)) < 0) {
				goto error;
			}
			rpc::response::pack_header(sr, m_msgid);
			sr << msgid;
			sr.pushnil();
			if ((r = respond(false, sr)) < 0) {
				goto error;
			}
			m_status = ncc_wait_global_commit;
			break;
		case ncc_wait_global_commit:
			add_master = true;
			msgid = new_msgid();
			if (msgid == INVALID_MSGID) {
				r = NBR_EEXPIRE;
				goto error;
			}
			if ((r = yielding(msgid, 1, NULL, ctx)) < 0) {
				goto error;
			}
			rh = &(w->rh());
			rh->init(&ff(), w, ff().curr(), msgid);
			if (!ff().of().start_rehasher(rh) < 0) {
				ASSERT(false);
				r = NBR_EPTHREAD;
				goto error;
			}
			m_status = ncc_add_wait_rehash;
			break;
		case ncc_add_wait_rehash:
			/* if vm_init is skipped (because of already world created)
			 * then ctx->m_rep_size == 0 so nothing happen */
			add_master = true;
			if ((r = quorum_global_commit(w, ctx, NBR_OK)) < 0) {
				goto error;
			}
			break;
		default:
			ASSERT(false);
			r = NBR_EINVAL;
			goto error;
		}
		return NBR_OK;
	}
	else {
		r = res.err();
		goto error;
	}
error:
	if (msgid != INVALID_MSGID) { ff().fiber_unregister(msgid); }
	quorum_global_commit(w, ctx, r);
	if (add_master) {
		/* TODO : send del node to master */
		assert(false);
	}
	else {
		w->del_node(ctx->m_node_addr);
	}
	return r;
}


int svnt::fiber::call_node_regist(rpc::request &rq)
{
	int r;
	rpc::node_ctrl_cmd::regist &req = (rpc::node_ctrl_cmd::regist &)rq;
	MSGID msgid = INVALID_MSGID;
	msgid = new_msgid();
	if (msgid == INVALID_MSGID) {
		r = NBR_EEXPIRE;
		goto error;
	}
	if ((r = rpc::node_ctrl_cmd::regist::pack_header(
		ff().sr(), msgid, req.node_server_addr(),
		req.node_server_addr_len(), req.node_type())) < 0) {
		goto error;
	}
	if ((r = yielding(msgid)) < 0) {
		goto error;
	}
	if ((r = ff().wf().cf()->backend_conn()->send(msgid,
		ff().sr().p(), ff().sr().len())) < 0) {
		goto error;
	}
	return NBR_OK;
error:
	if (msgid != INVALID_MSGID) { ff().fiber_unregister(msgid); }
	if (r < 0) {
		send_error(r);
	}
	return r;
}
int svnt::fiber::resume_node_regist(rpc::response &res)
{
	if (res.success()) {
		return NBR_OK;
	}
	else {
		ASSERT(false);
		exit(1);
		return res.err();
	}
}


int svnt::fiber::node_ctrl_del(world *w,
		rpc::node_ctrl_cmd::del &req, serializer &sr)
{
	int r;
	MSGID msgid = INVALID_MSGID;
	quorum_context *ctx;

	if (!w) {
		r = NBR_ENOTFOUND;
		goto error;
	}
	if (!(ctx = init_context(w))) {
		r = NBR_EALREADY;
		goto error;
	}
	msgid = new_msgid();
	if (msgid == INVALID_MSGID) {
		r = NBR_EEXPIRE;
		goto error;
	}
	if ((r = node_ctrl_cmd::vm_fin::pack_header(ff().sr(), msgid,
		req.wid(), req.wid().len())) < 0) {
		goto error;
	}
	if ((r = quorum_vote_commit(msgid, ctx, ff().sr())) < 0) {
		goto error;
	}
	return NBR_OK;
error:
	if (msgid != INVALID_MSGID) {
		ff().fiber_unregister(msgid);
	}
	if (ctx) { ff().quorums().erase(w->id()); }
	return r;
}
int svnt::fiber::node_ctrl_del_resume(world *w, rpc::response &res, serializer &sr)
{
	quorum_context *ctx = yld()->p<quorum_context>();
	MSGID msgid = INVALID_MSGID;
	int r;
	if (res.success()) {
		if (!w || !w->del_node(ctx->m_node_addr)) {
			quorum_global_commit(w, ctx, NBR_ENOTFOUND);
			return NBR_ENOTFOUND;
		}
		switch(m_status) {
		case start:
			msgid = new_msgid();
			if (msgid == INVALID_MSGID) {
				r = NBR_EEXPIRE;
				goto error;
			}
			if ((r = yielding(msgid, 1, NULL, ctx)) < 0) {
				goto error;
			}
			rpc::response::pack_header(sr, m_msgid);
			sr << msgid;
			sr.pushnil();
			if ((r = respond(false, sr)) < 0) {
				goto error;
			}
			m_status = ncc_wait_global_commit;
			break;
		case ncc_wait_global_commit:
			quorum_global_commit(w, ctx, NBR_OK);
			break;
		}
		return NBR_OK;
	}
	else {
		r = res.err();
		goto error;
	}
error:
	if (msgid != INVALID_MSGID) { ff().fiber_unregister(msgid); }
	/* rollback node status */
	w->add_node(ctx->m_node_addr);
	quorum_global_commit(w, ctx, r);
	return r;
}


int svnt::fiber::node_ctrl_list(world *w,
		rpc::node_ctrl_cmd::list &req, serializer &sr)
{
	return NBR_OK;
}
int svnt::fiber::node_ctrl_list_resume(world *w,
		rpc::response &res, serializer &sr)
{
	if (res.success()) {
		rpc::response::pack_header(sr, m_msgid);
		sr << NBR_OK;
		sr.pushnil();
		return NBR_OK;
	}
	else {
		/* TODO: rollback master node status */
		return res.err();
	}
}


int svnt::fiber::node_ctrl_deploy(world *w,
		rpc::node_ctrl_cmd::deploy &req, serializer &sr)
{
	int r;
	MSGID msgid;
	quorum_context *ctx;

	if (!w) {
		r = NBR_ENOTFOUND;
		goto error;
	}
	if (!(ctx = init_context(w))) {
		r = NBR_EALREADY;
		goto error;
	}
	msgid = new_msgid();
	if (msgid == INVALID_MSGID) {
		r = NBR_EEXPIRE;
		goto error;
	}
	if ((r = node_ctrl_cmd::vm_deploy::pack_header(ff().sr(), msgid,
		req.wid(), req.wid().len(), req.srcfile())) < 0) {
		goto error;
	}
	if ((r = quorum_vote_commit(msgid, ctx, ff().sr()))) {
		goto error;
	}
	return NBR_OK;
error:
	if (msgid != INVALID_MSGID) {
		ff().fiber_unregister(msgid);
	}
	if (ctx) { ff().quorums().erase(w->id()); }
	return r;
}
int svnt::fiber::node_ctrl_deploy_resume(world *w,
		rpc::response &res, serializer &sr)
{
	quorum_context *ctx = yld()->p<quorum_context>();
	MSGID msgid = INVALID_MSGID;
	int r;
	if (res.success()) {
		switch(m_status) {
		case start:
			msgid = new_msgid();
			if (msgid == INVALID_MSGID) {
				r = NBR_EEXPIRE;
				goto error;
			}
			if ((r = yielding(msgid, 1, NULL, ctx)) < 0) {
				goto error;
			}
			rpc::response::pack_header(sr, m_msgid);
			sr << msgid;
			sr.pushnil();
			if ((r = respond(false, sr)) < 0) {
				goto error;
			}
			m_status = ncc_wait_global_commit;
			break;
		case ncc_wait_global_commit:
			quorum_global_commit(w, ctx, NBR_OK);
			break;
		}
		return NBR_OK;
	}
	else {
		/* TODO: rollback vm status */
		r = res.err();
		goto error;
	}
error:
	if (msgid != INVALID_MSGID) { ff().fiber_unregister(msgid); }
	quorum_global_commit(w, ctx, r);
	return r;
}

int svnt::fiber::node_ctrl_vm_init(
		class world *w, rpc::node_ctrl_cmd::vm_init &req, serializer &sr)
{
	int r;
	MSGID msgid = INVALID_MSGID;
	if (!w) { return NBR_ENOTFOUND; }
	if ((r = ff().vm()->init_world(req.wid(), req.from(), req.srcfile())) < 0) {
		goto error;
	}
	msgid = new_msgid();
	if (msgid == INVALID_MSGID) {
		goto error;
	}
	if ((r = rpc::create_object_request::pack_header(
		sr, msgid, w->world_object_uuid(),
		ll::world_klass_name, ll::world_klass_name_len,
		w->id(), nbr_str_length(w->id(), max_wid), NULL, 0)) < 0) {
		goto error;
	}
	if ((r = yielding(msgid)) < 0) {
		goto error;
	}
	if ((r = w->request(msgid, w->world_object_uuid(), sr)) < 0) {
		goto error;
	}
	return NBR_OK;
error:
	if (msgid != INVALID_MSGID) {
		ff().fiber_unregister(msgid);
	}
	ff().vm()->fin_world(req.wid());
	return r;
}
int svnt::fiber::node_ctrl_vm_init_resume(
		class world *w, rpc::response &res, serializer &sr)
{
	int r;
	MSGID msgid = INVALID_MSGID;
	ll::coroutine *co = NULL;
	object *o = NULL;
	if (res.success()) {
		switch(m_status) {
		case start:
			if (!w) {
				r = NBR_ENOTFOUND;
				goto error;
			}
			/* add this object as global variable */
			if (!(co = ff().co_create(this))) {
				r = NBR_EEXPIRE;
				goto error;
			}
			if (!(o = ff().of().load(w->world_object_uuid(), co,
				w, ff().vm(), ll::world_klass_name))) {
				r = NBR_EEXPIRE;
				goto error;
			}
			if ((r = co->push_world_object(o)) < 0) {
				goto error;
			}
			ff().vm()->co_destroy(co);
			co = NULL;
			msgid = new_msgid();
			if (msgid == INVALID_MSGID) {
				r = NBR_EEXPIRE;
				goto error;
			}
			if ((r = yielding(msgid)) < 0) {
				goto error;
			}
			rpc::response::pack_header(sr, m_msgid);
			sr << msgid;
			sr.pushnil();
			if ((r = respond(false, sr)) < 0) {
				goto error;
			}
			m_status = ncc_wait_global_commit;
			return NBR_OK;
		case ncc_wait_global_commit:
			return NBR_OK;
		}
	}
	else {
		/* rollback vm status */
		ff().vm()->fin_world(wid());
		if (res.err().type() == datatype::INTEGER) {
			r = res.err();
		}
		else {
			r = NBR_EINVAL;
		}
		goto error;
	}
error:
	if (o) { ff().of().unload(o->uuid(), co); }
	if (co) { ff().vm()->co_destroy(co); }
	if (msgid != INVALID_MSGID) { ff().fiber_unregister(msgid); }
	return r;
}

int svnt::fiber::node_ctrl_vm_fin(
		class world *w, rpc::node_ctrl_cmd::vm_fin &req, serializer &sr)
{
	int r;
	if (!w) { return NBR_ENOTFOUND; }
	ff().vm()->fin_world(req.wid());
	MSGID msgid = new_msgid();
	if (msgid == INVALID_MSGID) {
		return NBR_EEXPIRE;
	}
	if ((r = yielding(msgid)) < 0) {
		ff().fiber_unregister(msgid);
		return r;
	}
	rpc::response::pack_header(sr, m_msgid);
	sr << msgid;
	sr.pushnil();
	if ((r = respond(false, sr)) < 0) {
		ff().fiber_unregister(msgid);
		return r;
	}
	return NBR_OK;
}
int svnt::fiber::node_ctrl_vm_fin_resume(
		class world *w, rpc::response &res, serializer &sr)
{
	if (res.success()) {
		return NBR_OK;
	}
	else {
		/* TODO: rollback vm status */
		return res.err();
	}
}

int svnt::fiber::node_ctrl_vm_deploy(
		class world *w, rpc::node_ctrl_cmd::vm_deploy &req, serializer &sr)
{
	int r;
	if (!w) { return NBR_ENOTFOUND; }
	if ((r = ff().vm()->load_module(req.wid(), req.srcfile())) < 0) {
		return r;
	}
	MSGID msgid = new_msgid();
	if (msgid == INVALID_MSGID) {
		return NBR_EEXPIRE;
	}
	if ((r = yielding(msgid)) < 0) {
		ff().fiber_unregister(msgid);
		return r;
	}
	rpc::response::pack_header(sr, m_msgid);
	sr << msgid;
	sr.pushnil();
	if ((r = respond(false, sr)) < 0) {
		ff().fiber_unregister(msgid);
		return r;
	}
	return NBR_OK;
}

int svnt::fiber::node_ctrl_vm_deploy_resume(
		class world *w, rpc::response &res, serializer &sr)
{
	if (res.success()) {
		return NBR_OK;
	}
	else {
		/* TODO: rollback vm status */
		return res.err();
	}
}

