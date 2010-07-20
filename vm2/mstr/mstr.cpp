#include "finder.h"
#include "mstr.h"
#include "world.h"
#include "object.h"
#include "connector.h"
#include "cp.h"

using namespace pfm;

#if !defined(_TEST)
extern int custom_respond(pfm::fiber *f, bool err, pfm::serializer &sr)
{
	return ((pfm::mstr::fiber *)f)->respond(err, sr);
}
#endif

namespace pfm {
namespace mstr {
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
	void poll(UTIME ut) {

	}
};


/* config */
class config : public util::config {
public:
	char m_dbmopt[256];
	typedef util::config super;
	config(BASE_CONFIG_PLIST, const char *dbmopt);
	int set(const char *k, const char *v);
};

config::config(BASE_CONFIG_PLIST, const char *dbmopt) : super(BASE_CONFIG_CALL) {
	nbr_str_copy(m_dbmopt, sizeof(m_dbmopt), dbmopt, sizeof(m_dbmopt));
}
int config::set(const char *k, const char *v) {
	if (strcmp(k, "dbmopt") == 0) {
		nbr_str_copy(m_dbmopt, sizeof(m_dbmopt), v, sizeof(m_dbmopt));
		return NBR_OK;
	}
	return super::set(k, v);
}
}
pfmm *mstr::session::m_daemon = NULL;
#if defined(_DEBUG)
bool mstr::session::m_test_mode = true;
#else
bool mstr::session::m_test_mode = false;
#endif
}

base::factory *
pfmm::create_factory(const char *sname)
{
	if (strcmp(sname, "be") == 0) {
		return new base::factory_impl<mstr::msession>;
	}
	if (strcmp(sname, "mstr") == 0) {
		return new base::factory_impl<mstr::session>;
	}
	if (strcmp(sname, "finder") == 0) {
		return new mstr::finder_factory;
	}
	ASSERT(false);
	return NULL;
}

int
pfmm::create_config(config* cl[], int size)
{
	CONF_START(cl);
	CONF_ADD(base::config, (
			"be",
			"",
			10,
			60, opt_expandable,
			64 * 1024, 64 * 1024,
			0, 0,
			-1, 0,
			"TCP", "eth0",
			1 * 100 * 1000/* 100msec task span */,
			1 * 1000 * 1000/* after 1s, again try to connect */,
			kernel::INFO,
			nbr_sock_rparser_bin16,
			nbr_sock_send_bin16,
			util::config::cfg_flag_not_set));
	CONF_ADD(mstr::config, (
			"mstr",
			"0.0.0.0:9000",
			10,
			60, opt_expandable,
			64 * 1024, 64 * 1024,
			0, 0,
			-1, 0,
			"TCP", "eth0",
			1 * 100 * 1000/* 100msec task span */,
			1 * 1000 * 1000/* after 1s, again try to connect */,
			kernel::INFO,
			nbr_sock_rparser_bin16,
			nbr_sock_send_bin16,
			util::config::cfg_flag_server, 
			"./mstr/db"));
	CONF_ADD(cluster::finder_property, (
			"finder",
			"0.0.0.0:8888",
			10,
			30, opt_expandable,/* max 10 session/30sec timeout */
			256, 2048, /* send 256b,recv2kb */
			0, 0,/* no ping */
			-1,0,/* no query buffer */
			"UDP", "eth0",
			10 * 1000 * 1000/* every 10 sec, send probe command */,
			0/* never wait ld recovery */,
			kernel::INFO,
			nbr_sock_rparser_raw,
			nbr_sock_send_raw,
			config::cfg_flag_server,
			finder_property::MCAST_GROUP,
			9999, 1/* ttl = 1 */));
	CONF_END();
}

int
pfmm::boot(int argc, char *argv[])
{
	mstr::session::m_daemon = this;
	if (argc > 1 && 0 == strncmp(argv[1], "--test=", sizeof("--test="))) {
		int tmode;
		SAFETY_ATOI(argv[1] + sizeof("--test="), tmode, int);
		mstr::session::m_test_mode = (tmode != 0);
	}
	mstr::config *c = find_config<mstr::config>("mstr");
	if (!c) { return  NBR_ENOTFOUND; }
	int r;
	char path[256];
	conn_pool::mstr_cp *fc;
	mstr::finder_factory *fdr;
	INIT_OR_DIE((r = m_db.init(config::makepath(path, sizeof(path), c->m_dbmopt, "uuid.tch"))) < 0, r,
		"uuid DB init fail (%d)\n", r);
	INIT_OR_DIE((r = UUID::init(m_db)) < 0, r,
		"UUID init fail (%d)\n", r);
	INIT_OR_DIE((r = mstr::fiber::init_global(10000, 
		config::makepath(path, sizeof(path), c->m_dbmopt, "al.tch"))) < 0, r,
		"mstr::fiber::init fails(%d)\n", r);
	INIT_OR_DIE((r = ff().init(100, 100, 10)) < 0, r,
		"fiber_factory init fails(%d)\n", r);
	INIT_OR_DIE(!(fc = find_factory<conn_pool::mstr_cp>("mstr")), NBR_ENOTFOUND,
		"conn_pool not found (%p)\n", fc);
	INIT_OR_DIE(!(fdr = find_factory<mstr::finder_factory>("finder")), NBR_ENOTFOUND,
		"conn_pool not found (%p)\n", fc);
	ff().cp()->set_pool(fc);
	INIT_OR_DIE((r = ff().wf().cf()->init(
		ff().cp(), fc->ifaddr(), 100, 100, 100)) < 0, r,
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
pfmm::shutdown()
{
	UUID::fin(m_db);
	m_db.fin();
	mstr::fiber::fin_global();
	ff().wf().cf()->fin();
	ff().of().fin();
	ff().wf().fin();
	ff().fin();
}

/* fiber logic (master mode) */
mstr::fiber::account_list mstr::fiber::m_al;

int mstr::fiber::init_global(int max_account, const char *dbpath)
{
	if (!m_al.init(max_account, max_account,
		opt_threadsafe | opt_expandable, dbpath)) {
		return NBR_EMALLOC;
	}
	return NBR_OK;
}

int mstr::fiber::quorum_vote_commit(world *w, MSGID msgid,
		quorum_context *ctx, serializer &sr)
{
	conn *c;
	const char *addr;
	quorum_context::reply *rep;
	rep = ctx->m_reply;
	int r;
	world::iterator i;
	if ((r = yielding(msgid, ctx->m_rep_size,
			yield::get_cb(quorum_vote_callback), ctx)) < 0) {
		return r;
	}
	for (i = w->nodes().begin();
		i != w->nodes().end(); i = w->nodes().next(i)) {
		if (((U32)(rep - ctx->m_reply)) >= ctx->m_rep_size) {
			/* during commit, new node is added */
			return NBR_ESHORT;
		}
		addr = world::node_addr(*i);
		rep->node_addr.from(addr);
		rep++;
		if (!(c = w->cf().get_by(addr))) {
			return NBR_EEXPIRE;
		}
		if ((r = c->send(sr.p(), sr.len())) < 0) {
			return r;
		}
	}
	return NBR_OK;
}
mstr::fiber::quorum_context *mstr::fiber::init_context(world *w)
{
	quorum_context *ctx;
	if (!(ctx = ff().quorums().create_if_not_exist(w->id()))) {
		return NULL; /* already used */
	}
	TRACE("mstr::init_conext %p\n", ctx);
	int size = w->nodes().use();
	if (!(ctx->m_reply = new quorum_context::reply[size])) {
		ff().quorums().erase(w->id());
		return NULL;
	}
	ctx->m_rep_size = size;
	return ctx;
}

int mstr::fiber::quorum_global_commit(world *w, quorum_context *ctx, int result)
{
	if (!w) {
		ASSERT(false);
		return NBR_ENOTFOUND;
	}
	for (int i = 0; i < (int)ctx->m_rep_size; i++) {
		conn *c = w->cf().get_by(ctx->m_reply[i].node_addr);
		/* remove from member ship? */
		if (!c) { continue; }
		rpc::response::pack_header(ff().sr(), ctx->m_reply[i].msgid);
		if (result < 0) {
			ff().sr().pushnil();
			ff().sr() << result;
		}
		else {
			ff().sr() << NBR_OK;
			ff().sr().pushnil();
		}
		if (c->send(ff().sr().p(), ff().sr().len()) < 0) {
			continue;
		}
	}
	TRACE("quorum released %p/%p/%s\n", &ff().quorums(), ctx, w->id());
	ff().quorums().erase(w->id());
	return NBR_OK;
}

int mstr::fiber::quorum_vote_callback(rpc::response &r, class conn *c, void *p)
{
	quorum_context *ctx = (quorum_context *)p;
	address a;
#if !defined(_TEST)
	if (!c->valid()) {
		ASSERT(false);
		return NBR_EINVAL;
	}
#endif
	if (!r.success()) {
		return NBR_OK;
	}
	for (U32 i = 0; i < ctx->m_rep_size; i++) {
		if (0 == nbr_str_cmp(c->node_data()->iden, address::SIZE,
			ctx->m_reply[i].node_addr, address::SIZE)) {
			ctx->m_reply[i].msgid = r.ret();
			ASSERT(ctx->m_reply[i].msgid != 0);
			return NBR_OK;
		}
	}
	ASSERT(false);
	return NBR_ENOTFOUND;
}

int mstr::fiber::call_login(rpc::request &p)
{
	int r;
	bool exist;
	rpc::login_request &req = rpc::login_request::cast(p);
	account_list::record rec = m_al.load(req.account(), exist);
	LOG("login attempt : %s/%u\n", (const char *)req.account(), req.authdata().len());
	if (!rec) {
		r = NBR_ENOTFOUND;
		goto error;
	}
	if (exist) {
		/* already have an entry or login by another thread */
		if (!rec->login(req.wid())) {
			r = NBR_EALREADY;
			goto error;
		}
	}
	else {
		if (!rec->login(req.wid())) {
			r = NBR_EALREADY;
			goto error;
		}
		rec->m_uuid.assign();
		if ((r = m_al.save(rec, req.account(), true)) < 0) {
			m_al.unload(req.account(), NULL);
			goto error;
		}
	}
	char b[256];
	LOG("login success : UUID=<%s>\n", rec->uuid().to_s(b, sizeof(b)));
	rpc::login_response::pack_header(ff().sr(), m_msgid, rec->uuid());
	if ((r = respond(false, ff().sr())) < 0) {
		m_al.unload(req.account(), NULL);
		goto error;
	}
	return NBR_OK;
error:
	if (r < 0) {
		send_error(r);
	}
	return r;
}

int mstr::fiber::resume_login(rpc::response &res)
{
	return NBR_OK;
}

int mstr::fiber::call_logout(rpc::request &p)
{
	int r;
	rpc::logout_request &req = rpc::logout_request::cast(p);
	m_al.unload(req.account(), NULL);
	LOG("logout: %s\n", (const char*)req.account());
	rpc::response::pack_header(ff().sr(), m_msgid);
	ff().sr() << NBR_OK;
	ff().sr().pushnil();
	if ((r = respond(false, ff().sr())) < 0) {
		goto error;
	}
	return NBR_OK;
error:
	if (r < 0) { return send_error(r); }
	return r;
}
int mstr::fiber::resume_logout(rpc::response &res)
{
	return NBR_OK;
}

int mstr::fiber::call_node_inquiry(rpc::request &rq)
{
	int r;
	address a, da;
	MSGID msgid = INVALID_MSGID;
	rpc::node_inquiry_request &req = rpc::node_inquiry_request::cast(rq);
	if (get_socket_address(da) >= 0) {
		LOG("node_inquiry : from %s\n", (const char *)da);
	}
	a = ff().wf().cf()->pool().bind_addr(a);
	if (req.node_type() == master_node) {
		/* master find another master */
		msgid = new_msgid();
		if (msgid == INVALID_MSGID) {
			r = NBR_EEXPIRE;
			goto error;
		}
		if ((r = yielding(msgid)) < 0) {
			goto error;
		}
		rpc::node_inquiry_response::pack_header(ff().sr(),msgid,a,a.len());
		if ((r = ff().finder().send(ff().sr().p(), ff().sr().len())) < 0) {
			goto error;
		}
	}
	else {
		rpc::node_inquiry_response::pack_header(ff().sr(),m_msgid,a,a.len());
		if ((r = ff().finder().send(ff().sr().p(), ff().sr().len())) < 0) {
			goto error;
		}
	}
	return NBR_OK;
error:
	if (r < 0) {
		send_error(r);
	}
	return r;
}

int mstr::fiber::resume_node_inquiry(rpc::response &r)
{
	if (r.success()) {
		rpc::node_inquiry_response &res = rpc::node_inquiry_response::cast(r);
		/* FIXME : prepare another connector_factory to manage master connection */
		LOG("node_inquiry : connect to (%s)\n", (const char *)res.node_addr());
	}
	return NBR_OK;
}

int mstr::fiber::node_ctrl_add(world *w,
		rpc::node_ctrl_cmd::add &req, serializer &sr)
{
	int r;
	MSGID msgid = INVALID_MSGID;
	world::iterator i;
	conn *c = NULL;
	quorum_context *ctx = NULL;
	if (!w) {
		if (!(w = ff().world_create(req, true))) {
			r = NBR_EEXPIRE;
			goto error;
		}
		ASSERT(m_wid == NULL);
		m_wid = w->id();
	}
	if (!(c = w->cf().get_by((const char *)req.node_addr()))) {
		r = NBR_ENOTFOUND;
		goto error;
	}
	if (!w->add_node(*c)) {
		r = NBR_EEXPIRE;
		goto error;
	}
	if ((r = pack_cmd_add(sr, w, req, msgid)) < 0) {
		goto error;
	}
	if (!(ctx = init_context(w))) {
		r = NBR_EMALLOC;
		goto error;
	}
	ctx->m_node_addr.from(req.node_addr(), req.node_addr().len());
	/* it will yields inside of it (if possible) */
	if ((r = quorum_vote_commit(w, msgid, ctx, sr)) < 0) {
		goto error;
	}
	return NBR_OK;
error:
	if (msgid != INVALID_MSGID) {
		ff().fiber_unregister(msgid);
	}
	if (ctx) { ff().quorums().erase(w->id()); }
	w->del_node(req.node_addr());
	return r;
}
int mstr::fiber::node_ctrl_add_resume(world *w, rpc::response &res, serializer &sr)
{
	quorum_context *p = yld()->p<quorum_context>();

	if (res.success()) {
		quorum_global_commit(w, p, NBR_OK);
		rpc::response::pack_header(sr, m_msgid);
		sr << NBR_OK;
		sr.pushnil();
		return respond(false, sr);
	}
	else {
		w->del_node(p->m_node_addr);
		quorum_global_commit(w, p, res.err());
		return res.err();
	}
}

int mstr::fiber::call_node_regist(rpc::request &rq)
{
	int r;
	address a;
	conn *c;
	rpc::node_ctrl_cmd::regist &req = (rpc::node_ctrl_cmd::regist &)rq;
	if ((r = get_socket_address(a)) < 0) {
		goto error;
	}
	if (req.node_type() != master_node) {
		if (!(c = ff().wf().cf()->get_by_local(a))) {
			r = NBR_ENOTFOUND;
			goto error;
		}
		if (!ff().wf().cf()->insert(address(req.node_server_addr()), c)) {
			r = NBR_EINVAL;
			goto error;
		}
		c->set_node_data(req.node_server_addr(),
				world::vnode_replicate_num);
		/* for test, node is attached to rtkonline immediately. */
		if (req.node_type() == test_servant_node) {
			serializer &sr = ff().sr();
			UUID uuid;
			if ((r = rpc::node_ctrl_cmd::add::pack_header(
				sr, ff().new_msgid(),
				"test", sizeof("test") - 1,
				c->node_data()->iden,
				strlen(c->node_data()->iden),
				"", uuid, "svnt/ll/test/main.lua",
				0, NULL)) < 0) {
				ASSERT(false);
				goto error;
			}
			if ((r = ff().run_fiber(fiber::to_thrd(ff().curr()), sr.p(), sr.len())) < 0) {
				ASSERT(false);
				goto error;
			}
		}
	}
error:
	if (r < 0) {
		address a(req.node_server_addr());
		ff().wf().cf()->del_failover_chain(a);
		send_error(r);
	}
	else {
		serializer &sr = ff().sr();
		PREPARE_PACK(sr);
		rpc::response::pack_header(sr, m_msgid);
		sr << NBR_OK;
		sr.pushnil();
		return respond(false, sr);
	}
	return r;
}

int mstr::fiber::node_ctrl_del(world *w,
		rpc::node_ctrl_cmd::del &req, serializer &sr)
{
	int r;
	quorum_context *ctx = NULL;
	world::iterator i;
	MSGID msgid = INVALID_MSGID;
	if (!w) {
		r = NBR_ENOTFOUND;
		goto error;
	}
	if ((r = pack_cmd_del(sr, w, req, msgid)) < 0) {
		goto error;
	}
	if (!(ctx = init_context(w))) {
		r = NBR_EMALLOC;
		goto error;
	}
	ctx->m_node_addr.from(req.node_addr(), req.node_addr().len());
	/* it will yields inside of it (if possible) */
	if ((r = quorum_vote_commit(w, msgid, ctx, sr)) < 0) {
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
int mstr::fiber::node_ctrl_del_resume(world *w, rpc::response &res, serializer &sr)
{
	quorum_context *p = yld()->p<quorum_context>();
	if (res.success()) {
		quorum_global_commit(w, p, NBR_OK);
		rpc::response::pack_header(sr, m_msgid);
		sr << NBR_OK;
		sr.pushnil();
		return respond(false, sr);
	}
	else {
		w->del_node(p->m_node_addr);
		quorum_global_commit(w, p, res.err());
		return res.err();
	}
}


int mstr::fiber::node_ctrl_list(world *w,
		rpc::node_ctrl_cmd::list &req, serializer &sr)
{
	return NBR_OK;
}
int mstr::fiber::node_ctrl_list_resume(world *w,
		rpc::response &res, serializer &sr)
{
	if (res.success()) {
		rpc::response::pack_header(sr, m_msgid);
		sr << NBR_OK;
		sr.pushnil();
		return respond(false, sr);
	}
	else {
		/* TODO: rollback master node status */
		return res.err();
	}
}


int mstr::fiber::node_ctrl_deploy(world *w,
		rpc::node_ctrl_cmd::deploy &req, serializer &sr)
{
	int r;
	world::iterator i;
	quorum_context *ctx = NULL;
	MSGID msgid = INVALID_MSGID;
	if (!w) {
		r = NBR_ENOTFOUND;
		goto error;
	}
	if ((r = pack_cmd_deploy(sr, w, req, msgid)) < 0) {
		goto error;
	}
	if (!(ctx = init_context(w))) {
		r = NBR_EMALLOC;
		goto error;
	}
	/* it will yields inside of it (if possible) */
	if ((r = quorum_vote_commit(w, msgid, ctx, sr)) < 0) {
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
int mstr::fiber::node_ctrl_deploy_resume(world *w,
		rpc::response &res, serializer &sr)
{
	quorum_context *p = yld()->p<quorum_context>();
	if (res.success()) {
		quorum_global_commit(w, p, NBR_OK);
		rpc::response::pack_header(sr, m_msgid);
		sr << NBR_OK;
		sr.pushnil();
		return respond(false, sr);
	}
	else {
		quorum_global_commit(w, p, res.err());
		return res.err();
	}
}


