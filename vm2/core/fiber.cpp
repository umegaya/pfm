#include "finder.h"
#include "fiber.h"
#include "world.h"
#include "object.h"
#include "connector.h"
#include "cp.h"

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
		void (*wkev)(SWKFROM*,THREAD,char*,size_t)) {
	if (!(m_cp = new conn_pool)) {
		return NBR_EMALLOC;
	}
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

#if !defined(_TEST)
bool ffutil::init_tls()
#else
bool ffutil::init_tls(THREAD curr)
#endif
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
#if defined(_TEST)
	if (curr) { m_curr = curr; }
	else
#endif
	if (!(m_curr = nbr_thread_get_current())) {
		fin_tls();
		ASSERT(false);
		return false;
	}
	if (m_vm->init(m_max_rpc, m_curr) < 0) {
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
	if (m_cp) { delete m_cp; m_cp = NULL; }
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

int ffutil::run_fiber(conn *c, char *p, int l)
{
	return c->event(p, l);
}

world *ffutil::world_new(world_id wid)
{
	return m_wf.create(wid, m_max_node, m_max_replica);
}

void ffutil::world_destroy(const class world *w)
{
	m_vm->fin_world(w->id());
	m_wf.unload(w->id(), NULL);
}

void ffutil::remove_stored_packet(MSGID msgid)
{
	m_wf.cf()->remove_query(msgid);
}

void ffutil::connector_factory_poll(UTIME ut)
{
	m_wf.cf()->poll(ut);
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
	if (m_wf.save_from_ptr(w, (const char *)req.wid(), false, NULL) < 0) {
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
	sr_disposer srd(sr);
	switch(m_type) {
	case from_thread: {
		SWKFROM from = { from_thread, ff().curr() };
		return nbr_sock_worker_event(&from, m_thrd, sr.p(), sr.len());
	}
	case from_socket:
		return m_socket->send(sr.p(), sr.len());
	case from_fncall:
		return m_cb(sr);
	case from_mcastr:
		return m_finder_r->send(sr.p(), sr.len());
	case from_mcasts:
		return m_finder_s->send(sr.p(), sr.len());
	case from_fiber:
	case from_app:
		ASSERT(false);
		return NBR_ENOTSUPPORT;
	case from_client:
		return m_client->send(sr.p(), sr.len());
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

int
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

world_id
fiber::get_world_id(rpc::request &req)
{
	world *w = ff().wf().find(rpc::world_request::cast(req).wid());
	ASSERT(w);
	return w->id();
}

void
fiber::set_world_id_from(world *w)
{
	m_wid = w ? w->id() : NULL;
}

int basic_fiber::thrd_quorum_vote_commit(MSGID msgid,
		quorum_context *ctx, serializer &sr)
{
	int r;
	if ((r =yielding(msgid, ctx->m_rep_size,
		yield::get_cb(thrd_quorum_vote_callback), ctx)) < 0) {
		return r;
	}
	SWKFROM from = { from_thread, ff().curr() };
	if ((r = nbr_sock_worker_bcast_event(&from, sr.p(), sr.len())) < 0) {
		return r;
	}
	return NBR_OK;
}

basic_fiber::quorum_context *basic_fiber::thrd_quorum_init_context(world *w)
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

int basic_fiber::thrd_quorum_global_commit(world *w, quorum_context *ctx, int result)
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

int basic_fiber::thrd_quorum_vote_callback(rpc::response &r, THREAD t, void *p)
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


#include "svnt/svnt.h"
void fiber::set_validity(class conn *c){ m_validity.m_sk = c->sk(); }
void fiber::set_validity(class finder_session *s){ m_validity.m_sk = s->sk(); }
void fiber::set_validity(class svnt_csession *s){ m_validity.m_sk = s->sk(); }
bool fiber::valid() const
{
	switch(m_type) {
	case from_thread:
		return true;
	case from_socket:
		return nbr_sock_is_same(m_socket->sk(), m_validity.m_sk);
	case from_fncall:
		return true;
	case from_mcastr:
		return nbr_sock_is_same(m_finder_r->sk(), m_validity.m_sk);
	case from_mcasts:
		return true;
	case from_fiber:
		return m_fiber->msgid() == m_validity.m_msgid;
	case from_app:
		return true;
	case from_client:
		return nbr_sock_is_same(m_client->sk(), m_validity.m_sk);
	default:
		return false;
	}
}

