#if !defined(__FIBER_H__)
#define __FIBER_H__

#include "common.h"
#include "proto.h"
#include "ll.h"
#include "yield.h"

namespace pfm {
using namespace sfc;
using namespace sfc::util;
using namespace sfc::base;
using namespace pfm::cluster;

template <class FB>
class fiber_factory;

class fiber {
public:
	enum {
		start = 0,		/* for all status */
		/* ll_exec, create_object */
			/* have no fiber status */
		/* create_world  */
			create_world_object = 1,
		/* login */
			login_wait_object_create = 1,
		/* replicate */
			/* no sub status */
		/* node_ctrl */
			/* common */
			ncc_wait_global_commit = 1,
			ncc_common_end = 2,
			/* add */
			ncc_add_world_object = ncc_common_end,
			/* del */
			/* list */
			/* deploy */
			/* vm_init */
	};
	enum {
		from_thread = 1,
		from_socket = 2,
		from_fncall = 3,
		from_mcastr = 4,
		from_mcasts = 5,
		from_fiber = 6,
		from_app = 7,
	};
	typedef int (*callback)(serializer &sr);
protected:
	union {
		THREAD		m_thrd;
		class conn 	*m_socket;
		callback	m_cb;
		class finder_session *m_finder_r;
		class finder_factory *m_finder_s;
		class fiber *m_fiber;
		U64	m_param;
	};
	union {
		ll::coroutine *co;	/* ll_exec, creat_object */
		struct { U8 cmd, padd[3]; } nctrl; /* node ctrl */
	} m_ctx;
	U8 m_cmd, m_status, m_type, padd;
	MSGID m_msgid;
	world_id m_wid;
	yield *m_yld;
	class ffutil *m_ff;
public:
	friend class yield;
	fiber() : m_status(start), m_yld(NULL), m_ff(NULL) {m_ctx.co = NULL;}
	~fiber() {}
	world_id wid() const { return m_wid; }
	yield *yld() { return m_yld; }
	MSGID msgid() const { return m_msgid; }
#if defined(_TEST)
	static int (*m_test_respond)(fiber *, bool, serializer &);
	void set_msgid(MSGID msgid) { m_msgid = msgid; }
#endif
	void set_ff(class ffutil *ffu) { m_ff = ffu; }
	inline void fin();
	inline MSGID new_msgid();
	inline int yielding(MSGID msgid, int size = 1,
		yield::callback cb = NULL, void *p = NULL);
	static int noop(serializer &) { return NBR_OK; }
	template <class FB>
		void terminate(fiber_factory<FB> &ff, int err);
	template <class FB>
		int call(fiber_factory<FB> &ff, rpc::request &req, bool trusted);
	template <class FB>
		int resume(fiber_factory<FB> &ff, rpc::response &res);
	template <class FB>
		int timeout(fiber_factory<FB> &ff, rpc::response &res);
	int respond(bool err, serializer &sr);
protected:
	template <class FB> int call_node_ctrl(FB *fb, rpc::request &req);
	template <class FB> int resume_node_ctrl(FB *fb, rpc::response &res);
public:
	static int respond_type(THREAD) { return from_thread; }
	static int respond_type(class conn *) { return from_socket; }
	static int respond_type(callback) { return from_fncall; }
	static int respond_type(class finder_session *) { return from_mcastr; }
	static int respond_type(class finder_factory *) { return from_mcasts; }
	static int respond_type(class fiber *f) { return from_fiber; }
	static int respond_type(U64 p)  { return from_app; }
	void set_responder(SWKFROM *f) { m_thrd = f->p; m_type = (U8)f->type; }
	template <class FROM> void set_responder(FROM from) { 
		m_thrd = from; m_type = (U8)respond_type(from); }
	inline bool yielded() const { return m_yld != NULL && !m_yld->finished(); }
protected:
	world_id get_world_id(rpc::request &req) {
		return rpc::world_request::cast(req).wid();
	}
	int get_socket_address(address &a);
	inline int send_error(int err);
	class ffutil &ff() { return *m_ff; }
	int pack_cmd_add(serializer &, class world *, rpc::node_ctrl_cmd::add &, MSGID &);
	int pack_cmd_del(serializer &, class world *, rpc::node_ctrl_cmd::del &, MSGID &);
	int pack_cmd_deploy(serializer &, class world *, rpc::node_ctrl_cmd::deploy &, MSGID &);


protected:	/** DUMMY CALLBACKS **/
	static int respond_callback(U64, bool, serializer &) { return NBR_OK; }
	int call_login(rpc::request &req) { ASSERT(0); return NBR_ENOTSUPPORT; }
	int resume_login(rpc::response &res) { ASSERT(0); return NBR_ENOTSUPPORT; }
	int call_replicate(rpc::request &req) { ASSERT(0); return NBR_ENOTSUPPORT; }
	int resume_replicate(rpc::response &res) { ASSERT(0); return NBR_ENOTSUPPORT; }
	int call_node_inquiry(rpc::request &req){ ASSERT(0); return NBR_ENOTSUPPORT; }
	int resume_node_inquiry(rpc::response &res){ ASSERT(0); return NBR_ENOTSUPPORT; }
	int node_ctrl_add(class world *, rpc::node_ctrl_cmd::add &, serializer &)
	{ ASSERT(0); return NBR_ENOTSUPPORT; }
	int node_ctrl_add_resume(class world *, rpc::response &, serializer &)
	{ ASSERT(0); return NBR_ENOTSUPPORT; }
	int node_ctrl_del(class world *, rpc::node_ctrl_cmd::del &, serializer &)
	{ ASSERT(0); return NBR_ENOTSUPPORT; }
	int node_ctrl_del_resume(class world *, rpc::response &, serializer &)
	{ ASSERT(0); return NBR_ENOTSUPPORT; }
	int node_ctrl_list(class world *, rpc::node_ctrl_cmd::list &, serializer &)
	{ ASSERT(0); return NBR_ENOTSUPPORT; }
	int node_ctrl_list_resume(class world *, rpc::response &, serializer &)
	{ ASSERT(0); return NBR_ENOTSUPPORT; }
	int node_ctrl_deploy(class world *, rpc::node_ctrl_cmd::deploy &, serializer &)
	{ ASSERT(0); return NBR_ENOTSUPPORT; }
	int node_ctrl_deploy_resume(class world *, rpc::response &, serializer &)
	{ ASSERT(0); return NBR_ENOTSUPPORT; }
	int node_ctrl_vm_init(class world *,
			rpc::node_ctrl_cmd::vm_init &, serializer &)
	{ ASSERT(0); return NBR_ENOTSUPPORT; }
	int node_ctrl_vm_init_resume(class world *, rpc::response &, serializer &)
	{ ASSERT(0); return NBR_ENOTSUPPORT; }
	int node_ctrl_vm_fin(class world *,
		rpc::node_ctrl_cmd::vm_fin &, serializer &)
	{ ASSERT(0); return NBR_ENOTSUPPORT; }
	int node_ctrl_vm_fin_resume(class world *, rpc::response &, serializer &)
	{ ASSERT(0); return NBR_ENOTSUPPORT; }
	int node_ctrl_vm_deploy(class world *,
		rpc::node_ctrl_cmd::vm_deploy &, serializer &)
	{ ASSERT(0); return NBR_ENOTSUPPORT; }
	int node_ctrl_vm_deploy_resume(class world *, rpc::response &, serializer &)
	{ ASSERT(0); return NBR_ENOTSUPPORT; }
	int node_ctrl_regist(class world *, rpc::node_ctrl_cmd::regist &, serializer &)
	{ ASSERT(0); return NBR_ENOTSUPPORT; }
	int node_ctrl_regist_resume(class world *, rpc::response &, serializer &)
	{ ASSERT(0); return NBR_ENOTSUPPORT; }
};
namespace rpc {
class basic_fiber : public fiber {
public:
	template <class FB> fiber_factory<FB> &custom_ff() {
		return (fiber_factory<FB> &)fiber::ff();
	}
	template <class FB>
	int respond(bool err, serializer &sr) {
		switch(m_type){
		case from_fiber: {
			rpc::response res;
			PREPARE_UNPACK(sr);
			if (sr.unpack(res) <= 0) {
				ASSERT(false);
				return NBR_EINVAL;
			}
			return custom_ff<FB>().resume(this, res, sr.p(), sr.len());
		}
		case from_app: {
			return FB::respond_callback(m_param, err, sr);
		}
		default:
			return fiber::respond(err, sr);
		}
	}
};
}

#if defined(_TEST)
#define PFM_STLS
#else
#define PFM_STLS NBR_STLS
#endif

class ffutil
{
public:
	struct quorum_context {
		address m_node_addr;
		struct reply {
			MSGID msgid;
			address node_addr;
			THREAD thrd;
		} *m_reply;
		U32 m_rep_size;
		quorum_context() : m_reply(NULL) {}
		~quorum_context() { if (m_reply) { delete []m_reply; } }
	};
protected:
	msgid_generator m_seed;
	int m_max_rpc;
	U32 m_timeout;
	int m_max_node, m_max_replica;	/* world setting */
	PFM_STLS ll *m_vm;
	PFM_STLS serializer *m_sr;
	PFM_STLS THREAD m_curr;
	PFM_STLS array<yield> *m_yields;
	PFM_STLS time_t m_last_check;
	map<fiber*, MSGID>	m_fm;
	class object_factory &m_of;
	class world_factory &m_wf;
	class finder_factory *m_finder;
	/* FIXME : now 1 world only do 1 quorum based replication concurrently */
	map<quorum_context,world_id> m_quorums;
	THREAD *m_workers;
	U16 m_wnum, m_widx;
public:
	static const U32 max_cpu_core = 256;
	typedef map<fiber, MSGID> super;
	ffutil(class object_factory &of, class world_factory &wf) :
		m_seed(), m_max_rpc(0), m_timeout(10),
		m_max_node(0), m_max_replica(0),
		m_of(of), m_wf(wf), m_finder(NULL), m_widx(0) { clear_tls(); }
	~ffutil() {}
 	inline bool initialized() { return m_vm != NULL; }
	int init(int max_node, int max_replica,
		void (*wkev)(SWKFROM*,THREAD,char*,size_t));
	void clear_tls();
	void set_timeout(int timeout) { m_timeout = timeout; }
	bool init_tls();
	void fin_tls();
	void fin();
public:
	class object_factory &of() { return m_of; }
	class world_factory &wf() { return m_wf; }
	ll *vm() { return m_vm; }
	THREAD curr() { return m_curr; }
	serializer &sr() { return *m_sr; }
	array<yield> &yields() { return *m_yields; }
	map<fiber*,MSGID> &fm() { return m_fm; }
	map<quorum_context,world_id> &quorums() { return m_quorums; }
	bool quorum_locked(world_id wid) { return quorums().find(wid) != NULL; }
	void set_finder(class finder_factory *f) { m_finder = f; }
	class finder_factory &finder() { return *m_finder; }
	MSGID new_msgid() { return m_seed.new_id(); }
	MSGID seedval() { return m_seed.seedval(); }
	class world *world_new(world_id wid);
	class world *world_create(const rpc::node_ctrl_cmd::add &req);
	int world_create_in_vm(const rpc::node_ctrl_cmd::add &req);
	void world_destroy(const class world *w);
	world *find_world(world_id wid);
	void fiber_unregister(MSGID msgid) { m_fm.erase(msgid); }
	bool fiber_register(MSGID msgid, fiber *f) {
		return m_fm.insert(f, msgid) != m_fm.end(); }
	ll::coroutine *co_create(fiber *fb) {
		ll::coroutine *co = m_vm->co_new();
		if ((co ? co->init(fb, m_vm) : NBR_EEXPIRE) < 0) {
			if (co) { m_vm->co_destroy(co); }
		}
		return co;
	}
	template <class FROM>
	int run_fiber(FROM from, char *p, int l) {
		SWKFROM f = { fiber::respond_type(from), (void *)from };
		__sync_val_compare_and_swap(&m_widx, m_wnum, 0);
		return nbr_sock_worker_event(&f,
			m_workers[__sync_fetch_and_add(&m_widx, 1)], p, l);
	}
	int run_fiber(THREAD th, char *p, int l) {
		SWKFROM from = { fiber::from_thread, th };
		return nbr_sock_worker_event(&from, th, p, l);
	}
	int run_fiber(class conn *c, char *p, int l);
};

template <class FB>
class fiber_factory : public ffutil, public array<FB>
{
public:
	typedef ffutil util;
	typedef array<FB> super;
	fiber_factory(class object_factory &of, class world_factory &wf) :
		ffutil(of, wf) {}
	~fiber_factory() { fin(); }
	int init(int max_rpc, int max_node, int max_replica) {
		int r;
		m_max_rpc = max_rpc;
		if ((r = ffutil::init(max_node, max_replica, worker_event)) < 0) { return r; }
		return super::init(m_max_rpc, -1, opt_threadsafe | opt_expandable) ? 
			NBR_OK : NBR_EMALLOC;
	}
	void fin() {
		util::fin();
		super::fin();
	}
	void poll(time_t nt);
	FB *fiber_new() {
		FB *f = super::create();
		if (f) { f->set_ff(this); }
		return f;
	}
	void fiber_destroy(FB *f) { super::destroy(f); }
	FB *find_fiber(MSGID msgid) { return (FB *)util::m_fm.find(msgid); }
	template <typename FROM> int call(FROM from, rpc::request &req, bool trusted);
	template <typename FROM> int resume_nofw(FROM from, rpc::response &res) {
		return resume(from, res, NULL, 0); 
	}
	template <typename FROM> int resume(FROM from, rpc::response &res, char *p, int l);
	template <typename FROM> int recv(FROM from, char *p, int l, bool trust) {
		/* init TLS: jemalloc flaver */
		if (!util::initialized() && !util::init_tls()) {
			ASSERT(false);
			return NBR_EINVAL;
		}
		rpc::data d;
		sr().unpack_start(p, l);
		int r = sr().unpack(d);
		if (r < 0) { return r; }
		else if (r > 0) {
			switch(r = d.elem(0)) {
			case rpc::msg_request:
				return call(from, (rpc::request &)d, trust);
			case rpc::msg_response:
				return resume(from, (rpc::response &)d, p, l);
			default:
				ASSERT(false);
				return NBR_EINVAL;
			}
		}
		else {
			return NBR_OK;
		}
	}
	static void worker_event(SWKFROM *from, THREAD to, char *p, size_t l) {
		fiber_factory<FB> *ff = (fiber_factory<FB> *)nbr_sock_get_worker_data(to);
		ff->recv(from, p, l, true);
	}
};

/* inline functions */
template <class FB>
template <typename FROM> int
fiber_factory<FB>::call(FROM from, rpc::request &req, bool trusted)
{
	FB *f = fiber_new();
	if (!f) {
		ASSERT(false);
		return NBR_EEXPIRE;
	}
	TRACE("fiber new : %p\n", f);
	f->set_responder(from);
	int r = f->call(*this, req, trusted);
	if ((r < 0) || !f->yielded()) {
//		TRACE("finish c: %u/%d/%p/%p/%p\n",req.msgid(),r,curr(),f,f->yld());
		f->fin();
		fiber_destroy(f);
	}
	return r;
}

template <class FB>
template <typename FROM> int
fiber_factory<FB>::resume(FROM from, rpc::response &res, char *p, int l)
{
	FB *f = find_fiber(res.msgid());
	if (!f) {	/* would be destroyed by timeout or error */
		return NBR_ENOTFOUND;
	}
//	TRACE("resume: %u/%p/%p/%p\n", res.msgid(), curr(), f, f->yld());
	ASSERT(f->yld());
	if (f->yld()->reply(from, res) < 0) {
		return NBR_OK;	/* wait for reply */
	}
	THREAD attach = f->yld()->attached();
	if (attach != curr()) {
//		TRACE("fw: %u/%p/%p/%p\n", res.msgid(), curr(), attach, f);
		return run_fiber(f->yld()->attached(), p, l);
	}
	fiber_unregister(res.msgid());
	int r = f->resume(*this, res);
	if ((r < 0) || !f->yielded()) {
//		TRACE("finish r: %u/%d/%p/%p/%p\n",res.msgid(),r,curr(),f,f->yld());
		f->fin();
		fiber_destroy(f);
	}
	return r;
}

template <class FB>
void fiber_factory<FB>::poll(time_t nt)
{
	/* init TLS: jemalloc flaver */
	if (!util::initialized() && !util::init_tls()) {
		ASSERT(false);
		return;
	}
	if (nt > m_last_check) {
		array<yield>::iterator yit = yields().begin(), nyit;
		for (;yit != yields().end();) {
			nyit = yit;
			yit = yields().next(yit);
			if (nyit->timeout(nt, m_timeout)) {
				TRACE("fiber timeout (%u/%p)\n", nyit->fb()->msgid(), nyit->fb());
				PREPARE_PACK(sr());
				rpc::response::pack_header(sr(), nyit->msgid());
				sr().pushnil();
				sr() << NBR_ETIMEOUT;
				rpc::response resp;
				sr().unpack_start(sr().p(), sr().len());
				if (sr().unpack(resp) <= 0) {
					ASSERT(false);
					continue;
				}
				nyit->fb()->timeout(*this, resp);	/* do error handling */
			}
		}
		m_last_check = nt;
	}
}


/* fiber */
inline int
fiber::send_error(int err)
{
	PREPARE_PACK(ff().sr());
	rpc::response::pack_header(ff().sr(), m_msgid);
	ff().sr().pushnil();
	ff().sr() << err;
	return respond(true, ff().sr());
}

inline MSGID
fiber::new_msgid()
{
	MSGID msgid = ff().new_msgid();
	TRACE("%08x:register %p to %u\n", nbr_thread_get_curid(), this, msgid);
	return ff().fiber_register(msgid, this) ? msgid : INVALID_MSGID;
}

inline int
fiber::yielding(MSGID msgid, int size, yield::callback fn, void *p)
{
	if (!m_yld) { 
		m_yld = ff().yields().create(); 
		if (!m_yld) { return NBR_EEXPIRE; }
	}
	TRACE("yield: %u/%u/%p/%p/%p\n", msgid, size, ff().curr(), this, m_yld);
	return m_yld->init(this, msgid, size, fn, p);
}

template <class FB> void
fiber::terminate(fiber_factory<FB> &ff, int err)
{
	if (err < 0) {
		send_error(err);
		fin();
		ff.fiber_destroy(this);
	}
}

template <class FB> int
fiber::timeout(fiber_factory<FB> &ff, rpc::response &res)
{
	switch(m_type) {
	case from_socket:
		return ff.resume_nofw(m_socket, res);
	case from_thread:
		return ff.resume_nofw(m_thrd, res);
	case from_fncall:
		return ff.resume_nofw(m_cb, res);
	default:
		ASSERT(false);
		return NBR_EINVAL;
	}
}

template <class FB> int
fiber::call(fiber_factory<FB> &ff, rpc::request &req, bool trusted)
{
	m_cmd = (U8)(U32)req.method();
	m_msgid = req.msgid();
	switch(m_cmd) {
	case rpc::ll_exec:
		m_wid = get_world_id(req);
		if (!(m_ctx.co = ff.co_create(this))) { return NBR_EEXPIRE; }
		return m_ctx.co->call(rpc::ll_exec_request::cast(req), trusted);
	case rpc::login:
		m_wid = get_world_id(req);
		return ((FB*)this)->call_login(req);
	default:
		if (trusted) {
			switch(m_cmd) {
			case rpc::create_object:
			case rpc::load_object:
				m_wid = get_world_id(req);
				if (!(m_ctx.co = ff.co_create(this))) { return NBR_EEXPIRE; }
				return m_ctx.co->call(rpc::create_object_request::cast(req));
			case rpc::replicate:
				/* replicate not contain world ID */
				return ((FB*)this)->call_replicate(req);
			case rpc::node_ctrl:
				m_wid = get_world_id(req);
				return call_node_ctrl((FB*)this, req);
			case rpc::node_inquiry:
				return ((FB *)this)->call_node_inquiry(req);
			default:
				break;
			}
		}
		ASSERT(false);
		return NBR_ERIGHT;
	}
}

template <class FB> int
fiber::resume(fiber_factory<FB> &ff, rpc::response &res)
{
	switch(m_cmd) {
	case rpc::ll_exec:
	case rpc::create_object:
		return m_ctx.co->resume(rpc::ll_exec_response::cast(res), false);
	case rpc::load_object:
		return m_ctx.co->resume(rpc::ll_exec_response::cast(res), true);
	case rpc::replicate:
		return ((FB*)this)->resume_replicate(res);
	case rpc::login:
		return ((FB*)this)->resume_login(res);
	case rpc::node_ctrl:
		return resume_node_ctrl((FB*)this, res);
	case rpc::node_inquiry:
		return ((FB*)this)->resume_node_inquiry(res);
	default:
		ASSERT(false);
		break;
	}
}

template <class FB>
int fiber::call_node_ctrl(FB *fb, rpc::request &req)
{
	ASSERT(this == fb);
	int r = NBR_OK;
	address a;
	switch(m_status) {
	case start: {
		rpc::node_ctrl_request &ncr = rpc::node_ctrl_request::cast(req);
		PREPARE_PACK(ff().sr());
		world *w = ff().find_world(wid());
		m_ctx.nctrl.cmd = (U32)ncr.command();
		TRACE("call_node_ctrl %u/%u/%p\n", m_msgid, m_ctx.nctrl.cmd, this);
		switch(m_ctx.nctrl.cmd) {
		case rpc::node_ctrl_request::add:
			r = fb->node_ctrl_add(w, ncr, ff().sr());
			break;
		case rpc::node_ctrl_request::del:
			r = fb->node_ctrl_del(w, ncr, ff().sr());
			break;
		case rpc::node_ctrl_request::list:
			r = fb->node_ctrl_list(w, ncr, ff().sr());
			break;
		case rpc::node_ctrl_request::deploy:
			r = fb->node_ctrl_deploy(w, ncr, ff().sr());
			break;
		case rpc::node_ctrl_request::vm_init:
			r = fb->node_ctrl_vm_init(w, ncr, ff().sr());
			break;
		case rpc::node_ctrl_request::vm_fin:
			r = fb->node_ctrl_vm_fin(w, ncr, ff().sr());
			break;
		case rpc::node_ctrl_request::vm_deploy:
			r = fb->node_ctrl_vm_deploy(w, ncr, ff().sr());
			break;
		case rpc::node_ctrl_request::regist:
			r = fb->node_ctrl_regist(w, ncr, ff().sr());
			break;
		}
		} break;
	default:
		ASSERT(false);
		r = NBR_EINVAL;
		break;
	}
	if (r < 0) {
		send_error(r);
	}
	return r;
}

template <class FB>
int fiber::resume_node_ctrl(FB *fb, rpc::response &res)
{
	ASSERT(this == fb);
	int r = NBR_OK;
	PREPARE_PACK(ff().sr());
	world *w = ff().find_world(wid());
	switch(m_ctx.nctrl.cmd) {
	case rpc::node_ctrl_request::add:
		r = fb->node_ctrl_add_resume(w, res, ff().sr());
		break;
	case rpc::node_ctrl_request::del:
		r = fb->node_ctrl_del_resume(w, res, ff().sr());
		break;
	case rpc::node_ctrl_request::list:
		r = fb->node_ctrl_list_resume(w, res, ff().sr());
		break;
	case rpc::node_ctrl_request::deploy:
		r = fb->node_ctrl_deploy_resume(w, res, ff().sr());
		break;
	case rpc::node_ctrl_request::vm_init:
		r = fb->node_ctrl_vm_init_resume(w, res, ff().sr());
		break;
	case rpc::node_ctrl_request::vm_fin:
		r = fb->node_ctrl_vm_fin_resume(w, res, ff().sr());
		break;
	case rpc::node_ctrl_request::vm_deploy:
		r = fb->node_ctrl_vm_deploy_resume(w, res, ff().sr());
		break;
	case rpc::node_ctrl_request::regist:
		r = fb->node_ctrl_regist_resume(w, res, ff().sr());
		break;
	}
	if (r < 0) {
		send_error(r);
	}
	return r;
}



inline void
fiber::fin()
{
	switch(m_cmd) {
	case rpc::ll_exec:
	case rpc::create_object:
	case rpc::load_object:
		ff().vm()->co_destroy(m_ctx.co);
		m_ctx.co = NULL;
		break;
	case rpc::replicate:
	case rpc::login:
	case rpc::node_ctrl:
	case rpc::node_inquiry:
		break;
	default:
		ASSERT(false);
		break;
	}
	m_cmd = 0;
	if (m_yld) {
		ff().yields().destroy(m_yld);
		m_yld = NULL;
	}
}
}
#endif
