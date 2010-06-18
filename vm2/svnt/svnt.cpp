#include "finder.h"
#include "svnt.h"
#include "world.h"
#include "object.h"
#include "connector.h"

using namespace pfm;

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
public:
	int init(const config &cfg) {
		return cluster::finder_factory::init(cfg, on_recv<finder>);
	}
	static int node_inquiry_cb(serializer &sr) { 
		TRACE("node_inquery finish \n");
		return NBR_OK;
	}
	void poll(UTIME ut) {
		if (session::app().ff().wf().cf()->backend_enable()) {
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
	typedef util::config super;
	config(BASE_CONFIG_PLIST);
};

config::config(BASE_CONFIG_PLIST) : super(BASE_CONFIG_CALL) {}
}
pfms *svnt::session::m_daemon = NULL;
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
		base::factory_impl<svnt::besession> *fc =
				new base::factory_impl<svnt::besession>;
		conn_pool_impl *cpi = (conn_pool_impl *)fc;
		ff().wf().cf()->set_pool(conn_pool::cast(cpi));
		return fc;
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
			10,
			60, opt_not_set,
			64 * 1024, 64 * 1024,
			0, 0,
			10000,	-1,
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
			10000,	-1,
			"TCP", "eth0",
			1 * 100 * 1000/* 100msec task span */,
			1 * 1000 * 1000/* after 1s, again try to connect */,
			kernel::INFO,
			nbr_sock_rparser_bin16,
			nbr_sock_send_bin16,
			util::config::cfg_flag_not_set));
	CONF_ADD(svnt::config, (
			"svnt",
			"0.0.0.0:8200",
			10,
			60, opt_expandable,
			64 * 1024, 64 * 1024,
			100 * 1000 * 1000, 5 * 1000 * 1000,
			10000,	-1,
			"TCP", "eth0",
			1 * 100 * 1000/* 100msec task span */,
			1 * 1000 * 1000/* after 1s, again try to connect */,
			kernel::INFO,
			nbr_sock_rparser_bin16,
			nbr_sock_send_bin16,
			util::config::cfg_flag_server));
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
	conn_pool_impl *fc;
	svnt::finder_factory *fdr;
	INIT_OR_DIE((r = m_db.init("svnt/db/uuid.tch")) < 0, r,
		"uuid DB init fail (%d)\n", r);
	INIT_OR_DIE((r = UUID::init(m_db)) < 0, r,
		"UUID init fail (%d)\n", r);
	INIT_OR_DIE((r = svnt::fiber::init_global(10000)) < 0, r,
		"svnt::fiber::init fails(%d)\n", r);
	INIT_OR_DIE(!(fc = find_factory<conn_pool_impl>("be")), NBR_ENOTFOUND,
		"conn_pool not found (%p)\n", fc);
	INIT_OR_DIE(!(fdr = find_factory<svnt::finder_factory>("finder")), NBR_ENOTFOUND,
		"conn_pool not found (%p)\n", fc);
	INIT_OR_DIE((r = ff().wf().cf()->init(conn_pool::cast(fc), 100, 100, 100)) < 0, r,
		"init connector factory fails (%d)\n", r);
	ff().set_finder(fdr);
	INIT_OR_DIE((r = ff().of().init(10000, 1000, 0, "svnt/db/of.tch")) < 0, r,
		"object factory creation fail (%d)\n", r);
	INIT_OR_DIE((r = ff().wf().init(
		256, 256, opt_threadsafe | opt_expandable, "svnt/db/wf.tch")) < 0, r,
		"object factory creation fail (%d)\n", r);
	INIT_OR_DIE((r = ff().init(100, 100, 10)) < 0, r,
		"fiber_factory init fails(%d)\n", r);
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
map<svnt::csession*, UUID> svnt::fiber::m_sm;

int svnt::fiber::quorum_vote_commit(MSGID msgid,
		quorum_context *ctx, serializer &sr)
{
	int r;
	if ((r =yielding(msgid, ctx->m_rep_size,
		yield::get_cb(quorum_vote_callback), ctx)) < 0) {
		return r;
	}
	SWKFROM from = { from_thread, ff().curr() };
	if ((r = nbr_sock_worker_bcast_event(&from, sr.p(), sr.len())) < 0) {
		return r;
	}
	return NBR_OK;
}
svnt::fiber::quorum_context *svnt::fiber::init_context(world *w)
{
	quorum_context *ctx;
	TRACE("try allocate quorum : %p/%s\n", &ff().quorums(), w->id());
	if (!(ctx = ff().quorums().create_if_not_exist(w->id()))) {
		return NULL; /* already used */
	}
	TRACE("svnt::init_context %p\n", ctx);
	THREAD ath[ffutil::max_cpu_core];
	int n_thread = ffutil::max_cpu_core;
	if ((n_thread = nbr_sock_get_worker(ath, n_thread)) < 0) {
		ff().quorums().erase(w->id());
		return NULL;
	}
	if (!(ctx->m_reply = new quorum_context::reply[n_thread])) {
		ff().quorums().erase(w->id());
		return NULL;
	}
	for (int i = 0; i < n_thread; i++) {
		ctx->m_reply[i].thrd = ath[i];
	}
	ctx->m_rep_size = n_thread;
	return ctx;
}

int svnt::fiber::quorum_global_commit(world *w, quorum_context *ctx, int result)
{
	for (int i = 0; i < (int)ctx->m_rep_size; i++) {
		if (ctx->m_reply[i].msgid == 0) {/* means this node have trouble */
			continue;	/* ignore and continue; */
		}
		rpc::response::pack_header(ff().sr(), ctx->m_reply[i].msgid);
		if (result < 0) {
			ff().sr().pushnil();
			ff().sr() << result;
		}
		else {
			ff().sr() << NBR_OK;
			ff().sr().pushnil();
		}
		SWKFROM from = { from_thread, ff().curr() };
		if (nbr_sock_worker_event(&from, ctx->m_reply[i].thrd,
			ff().sr().p(), ff().sr().len()) < 0) {
			continue;
		}
	}
	TRACE("quorum released %p/%p/%s\n", &ff().quorums(), ctx, w->id());
	ff().quorums().erase(w->id());
	return NBR_OK;
}

int svnt::fiber::quorum_vote_callback(rpc::response &r, SWKFROM *from, void *p)
{
	quorum_context *ctx = (quorum_context *)p;
	ASSERT(from->type == fiber::from_thread);
	THREAD t = from->p;
	if (!r.success()) {
		return NBR_OK;
	}
	for (U32 i = 0; i < ctx->m_rep_size; i++) {
		if (t == ctx->m_reply[i].thrd) {
			ctx->m_reply[i].msgid = r.ret();
			return NBR_OK;
		}
	}
	ASSERT(false);
	return NBR_ENOTFOUND;
}

int svnt::fiber::call_login(rpc::request &p)
{
	int r;
	world *w;
	MSGID msgid = INVALID_MSGID;
	rpc::login_request &req = rpc::login_request::cast(p);
	switch(m_status) {
	case start:
		if (!(w = ff().find_world(req.wid()))) {
			r = NBR_ENOTFOUND;
			goto error;
		}
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
	if (!(w = ff().find_world(wid()))) {
		r = NBR_ENOTFOUND;
		goto error;
	}
	if (p.success()) {
		switch(m_status) {
		case start: {
			rpc::login_response &res = rpc::login_response::cast(p);
			msgid = new_msgid();
			if (msgid == INVALID_MSGID) {
				r = NBR_EEXPIRE;
				goto error;
			}
			if ((r = rpc::create_object_request::pack_header(
				ff().sr(), msgid, res.object_id(),
				ll::player_klass_name, ll::player_klass_name_len,
				w->id(), nbr_str_length(w->id(), max_wid), true, 0)) < 0) {
				goto error;
			}
			if ((r = yielding(msgid)) < 0) {
				goto error;
			}
			if ((r = w->request(msgid, res.object_id(), ff().sr())) < 0) {
				goto error;
			}
			m_status = login_wait_object_create;
		} break;
		case login_wait_object_create: {
			rpc::ll_exec_response &res = rpc::ll_exec_response::cast(p);
			if (!(m_ctx.co = ff().co_create(this))) {
				r = NBR_EEXPIRE;
				goto error;
			}
			if ((r = m_ctx.co->call(res, "login")) < 0) {
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
		r = p.err();
		goto error;
	}
error:
	if (m_ctx.co) {
		ff().vm()->co_destroy(m_ctx.co);
		m_ctx.co = NULL;
	}
	if (msgid != INVALID_MSGID) { ff().fiber_unregister(msgid); }
	if (r < 0) {
		send_error(r);
	}
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
	PREPARE_PACK(ff().sr());
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
			if (!(w = ff().world_create(req))) {
				r = NBR_EEXPIRE;
				goto error;
			}
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
	MSGID msgid = INVALID_MSGID;
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
			/* if vm_init is skipped (because of already world created)
			 * then ctx->m_rep_size == 0 so nothing happen */
			quorum_global_commit(w, ctx, NBR_OK);
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
	w->del_node(ctx->m_node_addr);
	return r;
}


int svnt::fiber::node_ctrl_regist(class world *w,
		rpc::node_ctrl_cmd::regist &req, serializer &sr)
{
	int r;
	MSGID msgid = INVALID_MSGID;
	msgid = new_msgid();
	if (msgid == INVALID_MSGID) {
		r = NBR_EEXPIRE;
		goto error;
	}
	PREPARE_PACK(ff().sr());
	if ((r = rpc::node_ctrl_cmd::regist::pack_header(
		ff().sr(), msgid, req.node_server_addr(),
		req.node_server_addr().len(), req.node_type())) < 0) {
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
int svnt::fiber::node_ctrl_regist_resume(class world *w,
		rpc::response &res, serializer &sr)
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
		w->id(), nbr_str_length(w->id(), max_wid), false, 0)) < 0) {
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
			if (!(o = ff().of().load(w->world_object_uuid(),
				w, ff().vm(), ll::world_klass_name))) {
				r = NBR_EEXPIRE;
				goto error;
			}
			if (!(co = ff().co_create(this))) {
				r = NBR_EEXPIRE;
				goto error;
			}
			else if ((r = co->push_world_object(o)) < 0) {
				goto error;
			}
			ff().vm()->co_destroy(co);
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
		r = res.err();
		goto error;
	}
error:
	if (o) { ff().of().unload(o->uuid()); }
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

