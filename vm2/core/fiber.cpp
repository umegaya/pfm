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

