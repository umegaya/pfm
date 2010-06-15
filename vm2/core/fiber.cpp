#include "finder.h"
#include "fiber.h"
#include "world.h"
#include "object.h"
#include "connector.h"

using namespace pfm;
using namespace pfm::cluster;

#if !defined(_TEST)
NBR_TLS ll *ffutil::m_vm = NULL;
NBR_TLS serializer *ffutil::m_sr = NULL;
NBR_TLS THREAD ffutil::m_curr = NULL;
NBR_TLS array<yield> *ffutil::m_yields = NULL;
NBR_TLS time_t ffutil::m_last_check = 0;
void ffutil::clear_tls() {}
#else
int (*fiber::m_test_respond)(fiber *, bool, serializer &) = NULL;
void ffutil::clear_tls() {
	m_vm = NULL;
	m_sr = NULL;
	m_curr = NULL;
	m_yields = NULL;
	m_last_check = 0;
}
#endif

/* ffutil */
int ffutil::init(int max_node, int max_replica,
		void (*wkev)(THREAD,THREAD,char*,size_t)) {
	m_max_node = max_node;
	m_max_replica = max_replica;
	if (!m_quorums.init(world::max_world, world::max_world,
		-1, opt_threadsafe | opt_expandable)) {
		return NBR_EMALLOC;
	}
	int n_th = max_cpu_core;
	THREAD a_th[n_th];
	if ((n_th = nbr_sock_get_worker(a_th, n_th)) < 0) {
		return n_th;
	}
	m_wnum = (U16)n_th;
	if (!(m_workers = new THREAD[n_th])) {
		return NBR_EMALLOC;
	}
	if (wkev) {
		for (int i = 0; i < n_th; i++) {
			nbr_sock_set_worker_data(a_th[i], this, wkev);
		}
	}
	nbr_mem_copy(m_workers, a_th, n_th * sizeof(THREAD));
	return m_fm.init(m_max_rpc, m_max_rpc, -1, opt_threadsafe | opt_expandable) ? 
		NBR_OK : NBR_EMALLOC;
}

bool ffutil::init_tls()
{
	if (m_vm) { return true; }
	if (!(m_sr = new serializer)) {
		fin_tls();
		ASSERT(false);
		return false;
	}
	if (!(m_vm = new ll(m_of, m_wf, *m_sr, nbr_thread_get_current()))) {
		fin_tls();
		ASSERT(false);
		return false;
	}
	if (m_vm->init(m_max_rpc) < 0) {
		fin_tls();
		ASSERT(false);
		return false;
	}
	if (!(m_curr = nbr_thread_get_current())) {
		fin_tls();
		ASSERT(false);
		return false;
	}
	if (!(m_yields = new array<yield>) || 
		!m_yields->init(m_max_rpc, -1, opt_expandable)) {
		fin_tls();
		ASSERT(false);
		return false;
	}
	m_last_check = time(NULL);
	TRACE("ffutil:vm=%p/sr=%p/curr=%p\n", m_vm, m_sr, m_curr);
	return true;
}

void ffutil::fin()
{
	m_fm.fin();
	m_quorums.fin();
}

void ffutil::fin_tls()
{
	if (m_vm) {
		delete m_vm;
		m_vm = NULL;
	}
	if (m_sr) {
		delete m_sr;
		m_sr = NULL;
	}
	if (m_yields) {
		delete m_yields;
		m_yields = NULL;
	}
	m_curr = NULL;
}

world *ffutil::world_new(world_id wid)
{
	return m_wf.create(wid, m_max_node, m_max_replica);
}

void ffutil::world_destroy(const class world *w)
{
	m_vm->fin_world(w->id());
	m_wf.unload(w->id());
}

world *ffutil::find_world(world_id wid)
{
	return m_wf.find(wid);
}

int ffutil::world_create_in_vm(const rpc::node_ctrl_cmd::add &req)
{
	return m_vm->init_world(req.wid(), req.from(), req.srcfile());
}

world *ffutil::world_create(const rpc::node_ctrl_cmd::add &req)
{
	world *w = world_new(req.wid());
	if (!w) { return NULL; }
	w->set_world_object_uuid(req.world_object_id());
	for (int i = 0; i < req.n_node(); i++) {
		if (!w->add_node((const char *)req.addr(i))) {
			world_destroy(w);
			return NULL;
		}
		LOG("add node (%s) for (%s)\n", (const char *)req.addr(i), w->id());
	}
	if (m_wf.save_from_ptr(w, (const char *)req.wid()) < 0) {
		world_destroy(w);
		return NULL;
	}
	return w;
}


/* fiber */
int fiber::respond(bool err, serializer &sr)
{
#if defined(_TEST)
	if (m_test_respond) { return m_test_respond(this, err, sr); }
#endif
	switch(m_type) {
	case from_thread:
		return nbr_sock_worker_event(ff().curr(), m_thrd, sr.p(), sr.len());
	case from_socket:
		return m_socket->send(sr.p(), sr.len());
	case from_fncall:
		return m_cb(sr);
	case from_mcastr:
		return m_finder_r->send(sr.p(), sr.len());
	case from_mcasts:
		return m_finder_s->send(sr.p(), sr.len());
	case from_fiber:
		ASSERT(false);
		return NBR_ENOTSUPPORT;
	default:
		ASSERT(false);
		return NBR_ENOTFOUND;
	}
}

int fiber::pack_cmd_add(serializer &sr, world *w,
				rpc::node_ctrl_cmd::add &req,
				MSGID &msgid)
{
	msgid = new_msgid();
	if (msgid == INVALID_MSGID) { return NBR_EEXPIRE; }
	int sz = w->nodes().use(), n;
	world::iterator i;
	const char *nodes[sz];
	/* FIXME : exclude control with add_node, del_node */
	for (i = w->nodes().begin(), n = 0;
		i != w->nodes().end() && n < sz; i = w->nodes().next(i), n++) {
		nodes[n] = world::node_addr(*i);
		TRACE("node[%u]=%s\n", n, nodes[n]);
	}
	if (node_ctrl_cmd::add::pack_header(sr, msgid,
			req.wid(), req.wid().len(),
			req.node_addr(), req.node_addr().len(),
			req.from(), w->world_object_uuid(),
			req.srcfile(), sz, nodes) < 0) {
		ff().fiber_unregister(msgid);
		return NBR_ESHORT;
	}
	return NBR_OK;
}

int fiber::pack_cmd_del(serializer &sr, world *w,
						rpc::node_ctrl_cmd::del &req,
						MSGID &msgid)
{
	msgid = new_msgid();
	if (msgid == INVALID_MSGID) { return NBR_EEXPIRE; }
	if (node_ctrl_cmd::del::pack_header(sr, msgid,
			req.wid(), req.wid().len(),
			req.node_addr(), req.node_addr().len()) < 0) {
		ff().fiber_unregister(msgid);
		return NBR_ESHORT;
	}
	return NBR_OK;
}

int fiber::pack_cmd_deploy(serializer &sr, world *w,
						rpc::node_ctrl_cmd::deploy &req,
						MSGID &msgid)
{
	msgid = new_msgid();
	if (msgid == INVALID_MSGID) { return NBR_EEXPIRE; }
	if (node_ctrl_cmd::deploy::pack_header(sr, msgid,
			req.wid(), req.wid().len(),
			req.srcfile()) < 0) {
		ff().fiber_unregister(msgid);
		return NBR_ESHORT;
	}
	return NBR_OK;
}

inline int
fiber::get_socket_address(address &a)
{
	switch(m_type) {
	case from_socket:
		a = m_socket->get_addr(a);
		break;
	case from_mcastr:
		a = m_finder_r->get_addr(a);
		break;
	default:
		ASSERT(false);
		return NBR_EINVAL;
	}
	return NBR_OK;
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
			m_al.unload(req.account());
			goto error;
		}
	}
	char b[256];
	LOG("login success : UUID=<%s>\n", rec->uuid().to_s(b, sizeof(b)));
	PREPARE_PACK(ff().sr());
	rpc::login_response::pack_header(ff().sr(), m_msgid, rec->uuid());
	if ((r = respond(false, ff().sr())) < 0) {
		m_al.unload(req.account());
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

int mstr::fiber::call_node_inquiry(rpc::request &rq)
{
	int r;
	address a, da;
	MSGID msgid = INVALID_MSGID;
	rpc::node_inquiry_request &req = rpc::node_inquiry_request::cast(rq);
	if (get_socket_address(da) >= 0) {
		LOG("node_inquiry : from %s\n", (const char *)da);
	}
	PREPARE_PACK(ff().sr());
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
	char b[256];
	quorum_context *ctx = NULL;
	if (!w) {
		if (!req.world_object_id().valid()) {
			req.assign_world_object_id();
			LOG("world UUID assigned(%s/%s)\n", (const char *)req.wid(), 
				req.world_object_id().to_s(b, sizeof(b)));
		}
		if (!(w = ff().world_create(req))) {
			r = NBR_EEXPIRE;
			goto error;
		}
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

int mstr::fiber::node_ctrl_regist(world *w,
		rpc::node_ctrl_cmd::regist &req, serializer &sr)
{
	int r;
	address a;
	conn *c;
	ASSERT(!w);
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
			PREPARE_PACK(sr);
			if ((r = rpc::node_ctrl_cmd::add::pack_header(
				sr, ff().new_msgid(),
				"rtkonline", sizeof("rtkonline") - 1,
				c->node_data()->iden,
				strlen(c->node_data()->iden),
				"", uuid, "svnt/ll/rtkonline/main.lua",
				0, NULL)) < 0) {
				ASSERT(false);
				goto error;
			}
			if ((r = ff().run_fiber(sr.p(), sr.len())) < 0) {
				ASSERT(false);
				goto error;
			}
		}
	}
	rpc::response::pack_header(sr, m_msgid);
	sr << NBR_OK;
	sr.pushnil();
	return respond(false, sr);
error:
	if (r < 0) {
		address a(req.node_server_addr());
		ff().wf().cf()->del_failover_chain(a);
		send_error(r);
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

/* fiber logic (servant mode) */
map<class conn*, UUID> svnt::fiber::m_sm;

int svnt::fiber::quorum_vote_commit(MSGID msgid,
		quorum_context *ctx, serializer &sr)
{
	int r;
	if ((r =yielding(msgid, ctx->m_rep_size,
		yield::get_cb(quorum_vote_callback), ctx)) < 0) {
		return r;
	}
	if ((r = nbr_sock_worker_bcast_event(ff().curr(), sr.p(), sr.len())) < 0) {
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
		if (nbr_sock_worker_event(ff().curr(), ctx->m_reply[i].thrd,
			ff().sr().p(), ff().sr().len()) < 0) {
			continue;
		}
	}
	TRACE("quorum released %p/%p/%s\n", &ff().quorums(), ctx, w->id());
	ff().quorums().erase(w->id());
	return NBR_OK;
}

int svnt::fiber::quorum_vote_callback(rpc::response &r, THREAD t, void *p)
{
	quorum_context *ctx = (quorum_context *)p;
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


