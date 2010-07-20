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

typedef class __thread_object {
	operator THREAD() { return (THREAD)this; }
private:
	__thread_object() {}
	__thread_object(const __thread_object&) {}
} *thread_ptr;

class fiber {
public:
	enum {
		start = 0,		/* for all status */
		/* ll_exec, create_object */
			/* have no fiber status */
		/* create_world  */
			create_world_object = 1,
		/* login */
			login_authentication = 1,
			login_wait_object_create = 2,
			login_enter_world = 3,
			login_wait_init_vm = 4,
		/* replicate */
			/* no sub status */
		/* node_ctrl */
			/* common */
			ncc_wait_global_commit = 1,
			ncc_common_end = 2,
			/* add */
			ncc_add_world_object = ncc_common_end,
			ncc_add_wait_rehash,
			/* del */
			/* list */
			/* deploy */
			/* vm_init */
	};
	enum {
		from_invalid = 0,
		from_thread = 1,
		from_socket = 2,
		from_fncall = 3,
		from_mcastr = 4,
		from_mcasts = 5,
		from_fiber = 6,
		from_app = 7,
		from_client = 8,
	};
	typedef int (*callback)(serializer &sr);
	typedef U64 app_param;
protected:
	union {
		void *p;
		thread_ptr m_thrd;
		class conn 	*m_socket;
		class svnt_csession *m_client;
		callback	m_cb;
		class finder_session *m_finder_r;
		class finder_factory *m_finder_s;
		class fiber *m_fiber;
		app_param	m_param;
	};
	union {
		SOCK m_sk;		/* socket,client,mcastr */
		MSGID m_msgid;	/* fiber */
	} m_validity;
	union {
		ll::coroutine *co;	/* ll_exec, creat_object */
		struct { U8 cmd, padd[3]; } nctrl; /* node ctrl, replicate */
		MSGID climsgid; /* ll_exec_client */
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
	int type() const { return m_type; }
#if defined(_TEST)
	static int (*m_test_respond)(fiber *, bool, serializer &);
	void set_msgid(MSGID msgid) { m_msgid = msgid; }
#endif
	void set_ff(class ffutil *ffu) { m_ff = ffu; }
	inline void fin(bool term = false);
	inline MSGID new_msgid();
	inline MSGID new_watcher_msgid(class watcher **ppw);
	inline int yielding(MSGID msgid, int size = 1,
		yield::callback cb = NULL, void *p = NULL);
	bool valid() const;
	static int noop(serializer &) { return NBR_OK; }
	static ll *from_object(object *o);
	int check_forwarding_and_notice_intr(bool check_rpc, const UUID &uuid,
		char *p, int l, class ll **pvm);
	template <typename FROM> bool
	check_forwarding_and_notice(FROM from, bool check_rpc, const UUID &uuid,
		char *p, int l);
	template <class FB>
		void terminate(fiber_factory<FB> &ff, int err);
	template <class FB, typename FROM>
		int call(fiber_factory<FB> &ff, rpc::request &req, bool trusted,
				FROM from, char *p, int l);
	template <class FB>
		int resume(fiber_factory<FB> &ff, rpc::response &res, char *p, int l);
	template <class FB>
		int timeout(fiber_factory<FB> &ff, rpc::response &res);
	int respond(bool err, serializer &sr);
protected:
	template <class FB> int call_node_ctrl(FB *fb, rpc::request &req);
	template <class FB> int resume_node_ctrl(FB *fb, rpc::response &res);
public:
	template <typename DATA> static app_param to_prm(DATA d) { 
		return (app_param)d; }
	static thread_ptr to_thrd(THREAD th) { return (thread_ptr)th; } 
	static int respond_type(class conn *) { return from_socket; }
	static int respond_type(callback) { return from_fncall; }
	static int respond_type(class finder_session *) { return from_mcastr; }
	static int respond_type(class finder_factory *) { return from_mcasts; }
	static int respond_type(class fiber *) { return from_fiber; }
	static int respond_type(app_param) { return from_app; }
	static int respond_type(class svnt_csession *) { return from_client; }
	static int respond_type(thread_ptr) { return from_thread; }
	static int respond_type(SWKFROM *f) { return f->type; }
	static void *respond_ptr(class conn *p) { return (void *)p; }
	static void *respond_ptr(callback p) { return (void *)p; }
	static void *respond_ptr(class finder_session *p) { return (void *)p; }
	static void *respond_ptr(class finder_factory *p) { return (void *)p; }
	static void *respond_ptr(class fiber *p) { return (void *)p; }
	static void *respond_ptr(app_param p) { return (void *)p; }
	static void *respond_ptr(class svnt_csession *p) { return (void *)p; }
	static void *respond_ptr(thread_ptr p) { return (void *)p; }
	static void *respond_ptr(SWKFROM *f) { return f->p; }
	static bool has_stored_packet(class conn *) { return true; }
	static bool has_stored_packet(class svnt_csession *) { return true; }
	static bool has_stored_packet(callback) { return false; }
	static bool has_stored_packet(class finder_session *) { return false; }
	static bool has_stored_packet(class finder_factory *) { return false; }
	static bool has_stored_packet(class fiber *) { return false; }
	static bool has_stored_packet(app_param) { return false; }
	static bool has_stored_packet(thread_ptr) { return false; }
	static bool has_stored_packet(SWKFROM *f) { 
		return f->type == from_client || f->type == from_socket; }
	void set_validity(class conn *c);
	void set_validity(callback) {}
	void set_validity(class finder_session *s);
	void set_validity(class finder_factory *) {}
	void set_validity(class fiber *f) { m_validity.m_msgid = f->msgid();
		ASSERT(f->msgid() != INVALID_MSGID); }
	void set_validity(app_param)  {}
	void set_validity(class svnt_csession *s);
	void set_validity(thread_ptr) {}
	void set_validity(SWKFROM *);
	template <class FROM> void set_responder(FROM from) {
		set_validity(from);
		p = respond_ptr(from);
		m_type = (U8)respond_type(from);
	}
	inline bool yielded() const { return m_yld != NULL && !m_yld->finished(); }
protected:
	void set_world_id_from(class world *w);
	world_id get_world_id(rpc::request &req);
	object *get_object(rpc::ll_exec_request &req);
	int get_socket_address(address &a);
	class svnt_csession *get_client_conn() {return m_type==from_client?m_client:NULL;}
	inline int send_error(int err);
	class ffutil &ff() { return *m_ff; }
	int pack_cmd_add(serializer &,class world *, rpc::node_ctrl_cmd::add &, MSGID &);
	int pack_cmd_del(serializer &,class world *, rpc::node_ctrl_cmd::del &, MSGID &);
	int pack_cmd_deploy(serializer&,class world *,rpc::node_ctrl_cmd::deploy&,MSGID&);


protected:	/** DUMMY CALLBACKS **/
	static int respond_callback(U64, bool, serializer &) { return NBR_OK; }
	template <typename FROM> int call_ll_exec(FROM from, rpc::request &req);
	int call_ll_exec_client(rpc::request &req, object *o, bool trusted,
		char *p, int l){ ASSERT(0); return NBR_ENOTSUPPORT; }
	int call_login(rpc::request &req) { ASSERT(0); return NBR_ENOTSUPPORT; }
	int resume_login(rpc::response &res)
		{ ASSERT(0); return NBR_ENOTSUPPORT; }
	int call_logout(rpc::request &req) { ASSERT(0); return NBR_ENOTSUPPORT; }
	int resume_logout(rpc::response &res) { ASSERT(0); return NBR_ENOTSUPPORT; }
	int call_replicate(rpc::request &req, char *p, int l)
	{ ASSERT(0); return NBR_ENOTSUPPORT; }
	int resume_replicate(rpc::response &res) { ASSERT(0); return NBR_ENOTSUPPORT; }
	int call_node_inquiry(rpc::request &req){ ASSERT(0); return NBR_ENOTSUPPORT; }
	int resume_node_inquiry(rpc::response &res){ ASSERT(0); return NBR_ENOTSUPPORT; }
	int call_node_regist(rpc::request &req) { ASSERT(0); return NBR_ENOTSUPPORT; }
	int resume_node_regist(rpc::response &res){ ASSERT(0); return NBR_ENOTSUPPORT; }
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
};
namespace rpc {
class basic_fiber : public fiber {
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
public:	/* quorum base replication (thrd) */
	int thrd_quorum_vote_commit(MSGID msgid, quorum_context *ctx, serializer &sr);
	int thrd_quorum_global_commit(world *w, quorum_context *ctx, int result);
	quorum_context *thrd_quorum_init_context(world *w, quorum_context *qc = NULL);
	static int thrd_quorum_vote_callback(rpc::response &r, THREAD thrd, void *ctx);
public:
	typedef fiber super;
	template <class FB> fiber_factory<FB> &custom_ff() {
		return (fiber_factory<FB> &)fiber::ff();
	}
	template <class FB>
	int respond(bool err, serializer &sr) {
		switch(m_type){
		case from_fiber: {
			rpc::response res;
			PREPARE_UNPACK(sr);
			if (sr.unpack(res, sr.p(), sr.len()) <= 0) {
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
	template <class FB>
	int send_error(int err);
};
}

class watcher  {
protected:
	union {
		void *m_p;
		ll::watcher *m_w;
	};
	MSGID m_msgid;
public:
	watcher() : m_p(NULL), m_msgid(INVALID_MSGID) {}
	watcher(MSGID msgid) : m_p(NULL), m_msgid(msgid) {}
	MSGID msgid() const { return m_msgid; }
	void set_watcher(fiber *) {}
};

#if defined(_TEST)
#define PFM_STLS
#else
#define PFM_STLS NBR_STLS
#endif

class ffutil
{
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
	map<watcher, MSGID> m_wm;
	class object_factory &m_of;
	class world_factory &m_wf;
	class finder_factory *m_finder;
	class conn_pool *m_cp;
	/* FIXME : now 1 world only do 1 quorum based replication concurrently */
	map<rpc::basic_fiber::quorum_context,world_id> m_quorums;
	THREAD *m_workers;
	U16 m_wnum, m_widx;
public:
	static const U32 max_cpu_core = 256;
	typedef map<fiber, MSGID> super;
	ffutil(class object_factory &of, class world_factory &wf) :
		m_seed(), m_max_rpc(0), m_timeout(10),
		m_max_node(0), m_max_replica(0),
		m_of(of), m_wf(wf),
		m_finder(NULL), m_cp(NULL), m_widx(0) { clear_tls(); }
	~ffutil() {}
 	inline bool initialized() { return m_vm != NULL; }
	int init(int max_node, int max_replica,
		void (*wkev)(SWKFROM*,THREAD,char*,size_t));
	void clear_tls();
	void set_timeout(int timeout) { m_timeout = timeout; }
#if !defined(_TEST)
	bool init_tls();
#else
	bool init_tls(THREAD attached = NULL);
#endif
	void fin_tls();
	void fin();
public:
	class object_factory &of() { return m_of; }
	class world_factory &wf() { return m_wf; }
	class conn_pool *cp() { return m_cp; }
	ll *vm() { return m_vm; }
	THREAD curr() { return m_curr; }
	serializer &sr() { return *m_sr; }
	array<yield> &yields() { return *m_yields; }
	map<fiber*,MSGID> &fm() { return m_fm; }
	map<rpc::basic_fiber::quorum_context,world_id> &quorums() { return m_quorums; }
	bool quorum_locked(world_id wid) { return quorums().find(wid) != NULL; }
	void set_finder(class finder_factory *f) { m_finder = f; }
	class finder_factory &finder() { return *m_finder; }
	MSGID new_msgid() { return m_seed.new_id(); }
	MSGID seedval() { return m_seed.seedval(); }
	class world *world_new(world_id wid);
	class world *world_create(const rpc::node_ctrl_cmd::add &req, bool save);
	int world_create_in_vm(const rpc::node_ctrl_cmd::add &req);
	void world_destroy(const class world *w);
	world *find_world(world_id wid);
	void fiber_unregister(MSGID msgid) {
		m_fm.erase(msgid);
	}
	bool fiber_register(MSGID msgid, fiber *f) {
		return m_fm.insert(f, msgid) != m_fm.end(); }
	void watcher_unregister(MSGID msgid) {
		m_wm.erase(msgid);
	}
	void watcher_unregister(watcher *w) {
		watcher_unregister(w->msgid());
	}
	bool watcher_register(MSGID msgid, fiber *f, watcher **ppw) {
		watcher w(msgid);
		w.set_watcher(f);
		map<watcher, MSGID>::iterator i = m_wm.insert(w, msgid);
		if (ppw) { *ppw = &(*i); }
		return i != m_wm.end();
	}
	watcher *find_watcher(MSGID msgid) { return m_wm.find(msgid); }
	ll::coroutine *co_create(fiber *fb) {
		ll::coroutine *co = m_vm->co_new();
		if ((co ? co->init(fb, m_vm) : NBR_EEXPIRE) < 0) {
			if (co) { m_vm->co_destroy(co); }
		}
		return co;
	}
	void remove_stored_packet(MSGID msgid);
	void connector_factory_poll(UTIME ut);
	template <class FROM>
	int run_fiber(FROM from, char *p, int l) {
		SWKFROM f = { fiber::respond_type(from), fiber::respond_ptr(from) };
//		TRACE("p[7] = %u\n", p[7]);
		__sync_val_compare_and_swap(&m_widx, m_wnum, 0);
		return nbr_sock_worker_event(&f,
			m_workers[__sync_fetch_and_add(&m_widx, 1)], p, l);
	}
	template <class FROM>
	int run_fiber(FROM fr, THREAD to, char *p, int l) {
		SWKFROM from = { fiber::respond_type(fr), fiber::respond_ptr(fr) };
//		TRACE("2:p[7] = %u\n", p[7]);
		return nbr_sock_worker_event(&from, to, p, l);
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
	template <typename FROM> int call(FROM from, rpc::request &req, bool trusted,
			char *p, int l);
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
//		TRACE("unpacked size (%d)\n", sr().unpack_remain());
		int r = sr().unpack(d, p, l);
		if (r < 0) { return r; }
		else if (r > 0) {
			switch(r = d.elem(0)) {
			case rpc::msg_request:
//				TRACE("method = %u\n", (int)((rpc::request &)d).method());
				r = call(from, (rpc::request &)d, trust, p, l);
				break;
			case rpc::msg_response:
				r = resume(from, (rpc::response &)d, p, l);
				break;
			default:
				ASSERT(false);
				return NBR_EINVAL;
			}
			if (r < 0) { LOG("RPC error %d\n", r); }
			return NBR_OK;
		}
		else {
			ASSERT(false);
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
fiber_factory<FB>::call(FROM from, rpc::request &req, bool trusted, char *p, int l)
{
	FB *f = fiber_new();
	if (!f) {
		ASSERT(false);
		return NBR_EEXPIRE;
	}
//	TRACE("fiber new : %p\n", f);
	f->set_responder(from);
	int r = f->call(*this, req, trusted, from, p, l);
	if ((r < 0) || !f->yielded()) {
//		TRACE("finish c: %u/%u/%d/%p/%p/%p\n",req.msgid(),(U32)req.method(),r,curr(),f,f->yld());
		f->fin(r < 0);
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
		watcher *w = find_watcher(res.msgid());
		if (w) {
			watcher_unregister(res.msgid());
			return NBR_OK;
		}
		TRACE("msgid: %u not found\n", res.msgid());
		return NBR_ENOTFOUND;
	}
//	TRACE("resume: %u/%p/%p/%p\n", res.msgid(), curr(), f, f->yld());
	ASSERT(f->yld());
	THREAD attach = f->yld()->attached();
	if (attach != curr()) {
//		TRACE("fw: %u/%p/%p/%p\n", res.msgid(), curr(), attach, f);
		ASSERT(f->yielded());
		return run_fiber(from, f->yld()->attached(), p, l);
	}
	if (!f->valid()) {
		LOG("fiber invalid : %p %u %u\n", f, f->msgid(), f->type());
		f->fin(true);
		fiber_destroy(f);
		return NBR_EINVAL;
	}
	if (f->yld()->reply(from, res) < 0) {
		return NBR_OK;	/* wait for reply */
	}
	fiber_unregister(res.msgid());
	if (fiber::has_stored_packet(from)) {
		remove_stored_packet(res.msgid());
	}
	int r = f->resume(*this, res, p, l);
	if ((r < 0) || !f->yielded()) {
//		TRACE("finish r: %u/%d/%p/%p/%p\n",res.msgid(),r,curr(),f,f->yld());
		f->fin(r < 0);
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
	return;
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
				if (sr().unpack(resp, sr().p(), sr().len()) <= 0) {
					ASSERT(false);
					continue;
				}
				nyit->fb()->timeout(*this, resp);	/* do error handling */
			}
		}
		m_last_check = nt;
		connector_factory_poll(nbr_clock());
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

inline MSGID
fiber::new_watcher_msgid(watcher **ppw)
{
	MSGID msgid = ff().new_msgid();
	TRACE("%08x:watch %p to %u\n", nbr_thread_get_curid(), this, msgid);
	return ff().watcher_register(msgid, this, ppw) ? msgid : INVALID_MSGID;
}

inline int
fiber::yielding(MSGID msgid, int size, yield::callback fn, void *p)
{
	if (!m_yld) { 
		m_yld = ff().yields().create(); 
		if (!m_yld) { return NBR_EEXPIRE; }
	}
//	TRACE("yield: %u/%u/%p/%p/%p\n", msgid, size, ff().curr(), this, m_yld);
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
	case from_mcastr:
		return ff.resume_nofw(m_finder_r, res);
	case from_mcasts:
		return ff.resume_nofw(m_finder_s, res);
	case from_fiber:
		return ff.resume_nofw(m_fiber, res);
	case from_app:
		return ff.resume_nofw(m_param, res);
	case from_client:
		return ff.resume_nofw(m_client, res);
	default:
		ASSERT(false);
		return NBR_EINVAL;
	}
}

template <typename FROM> bool
fiber::check_forwarding_and_notice(FROM from, bool check_rpc, const UUID &uuid,
	char *p, int l)
{
	int r;
	class ll *ovm;
	if ((r = check_forwarding_and_notice_intr(check_rpc, uuid, p, l, &ovm)) < 0) {
		if (r == NBR_EINVAL &&
			(r = ff().run_fiber(from, ovm->attached_thrd(), p, l)) < 0) {
			send_error(r);
		}
		return true;
	}
	return false;
}


template <class FB, typename FROM> int
fiber::call(fiber_factory<FB> &ff, rpc::request &req, bool trusted,
		FROM from, char *p, int l)
{
	m_cmd = (U8)(U32)req.method();
	m_msgid = req.msgid();
	PREPARE_PACK(ff.sr());
	switch(m_cmd) {
	case rpc::ll_exec: {
		m_wid = get_world_id(req);
		rpc::ll_exec_request &rq = rpc::ll_exec_request::cast(req);
		if (check_forwarding_and_notice(from, true, rq.rcvr_uuid(), p, l)) {
			return NBR_OK; 
		}
		if (!(m_ctx.co = ff.co_create(this))) {
			send_error(NBR_EEXPIRE); return NBR_EEXPIRE; 
		}
		return m_ctx.co->call(rpc::ll_exec_request::cast(req), trusted);
	}
	case rpc::ll_exec_client: {
		m_wid = get_world_id(req);
		TRACE("ll_exec_client recv msgid = %u\n", req.msgid());
		object *o = get_object(rpc::ll_exec_request::cast(req));
		return ((FB *)this)->call_ll_exec_client(req, o, trusted, p, l);
	}
	case rpc::login:
		m_wid = get_world_id(req);
		return ((FB*)this)->call_login(req);
	case rpc::authentication: {
		rpc::authentication_request &rq = rpc::authentication_request::cast(req);
		m_wid = get_world_id(rq);
		/* TODO : if for different thread, switch thread */
		if (!(m_ctx.co = ff.co_create(this))) {
			send_error(NBR_EEXPIRE); return NBR_EEXPIRE;
		}
		return m_ctx.co->call(rq);
	}
	default:
		if (trusted) {
			switch(m_cmd) {
			case rpc::ll_exec_local: {
				m_wid = get_world_id(req);
				rpc::ll_exec_request &rq = rpc::ll_exec_request::cast(req);
				if (check_forwarding_and_notice(from, false, rq.rcvr_uuid(), p, l)) {
					return NBR_OK;
				}
				if (!(m_ctx.co = ff.co_create(this))) {
					send_error(NBR_EEXPIRE); return NBR_EEXPIRE;
				}
				return m_ctx.co->call(rq, (const char *)rq.method());
			}
			case rpc::create_object:
			case rpc::load_object:
				/* FIXME : load balance of number object assigned to each thread */
				m_wid = get_world_id(req);
				if (!(m_ctx.co = ff.co_create(this))) {
					send_error(NBR_EEXPIRE); return NBR_EEXPIRE;
				}
				return m_ctx.co->call(rpc::create_object_request::cast(req));
			case rpc::replicate:
			case rpc::start_replicate: {
				rpc::replicate_request &rq = rpc::replicate_request::cast(req);
				/* TODO : if for different thread, switch thread */
				m_wid = get_world_id(rq);
				if (check_forwarding_and_notice(from, false, rq.uuid(), p, l)) {
					return NBR_OK;
				}
				return ((FB*)this)->call_replicate(req, p, l);
			}
			case rpc::node_ctrl:
				return call_node_ctrl((FB*)this, req);
			case rpc::node_inquiry:
				return ((FB *)this)->call_node_inquiry(req);
			case rpc::logout:
				return ((FB *)this)->call_logout(req);
			case rpc::node_regist:
				return ((FB *)this)->call_node_regist(req);
			default:
				break;
			}
		}
		ASSERT(false);
		return NBR_ERIGHT;
	}
}

template <class FB> int
fiber::resume(fiber_factory<FB> &ff, rpc::response &res, char *p, int l)
{
	PREPARE_PACK(ff.sr());
	switch(m_cmd) {
	case rpc::ll_exec:
	case rpc::create_object:
	case rpc::authentication:
	case rpc::ll_exec_local:
		return m_ctx.co->resume(rpc::ll_exec_response::cast(res), false);
	case rpc::load_object:
		return m_ctx.co->resume(rpc::ll_exec_response::cast(res), false);
	case rpc::ll_exec_client:
		TRACE("msgid = %u, climsgid = %u\n", res.msgid(), m_ctx.climsgid);
		rpc::request::replace_msgid(m_ctx.climsgid, p, l);
		ff.sr().pack_start(p, l);
		ff.sr().set_curpos(l);
		return respond(!res.success(), ff.sr());
	case rpc::start_replicate:
	case rpc::replicate:
		return ((FB*)this)->resume_replicate(res);
	case rpc::login:
		return ((FB*)this)->resume_login(res);
	case rpc::node_ctrl:
		return resume_node_ctrl((FB*)this, res);
	case rpc::node_inquiry:
		return ((FB*)this)->resume_node_inquiry(res);
	case rpc::logout:
		return ((FB*)this)->resume_logout(res);
	case rpc::node_regist:
		return ((FB*)this)->resume_node_regist(res);
	default:
		ASSERT(false);
		return NBR_ENOTFOUND;
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
		world *w = ff().find_world(ncr.wid());
		set_world_id_from(w);
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
	}
	if (r < 0) {
		send_error(r);
	}
	return r;
}



inline void
fiber::fin(bool term)
{
	switch(m_cmd) {
	case rpc::ll_exec:
	case rpc::create_object:
	case rpc::load_object:
	case rpc::authentication:
	case rpc::ll_exec_local:
		if (m_ctx.co) {
			if (term) { m_ctx.co->fin(); }
			ff().vm()->co_destroy(m_ctx.co);
			m_ctx.co = NULL;
		}
		break;
	case rpc::replicate:
	case rpc::start_replicate:
	case rpc::login:
	case rpc::logout:
	case rpc::node_ctrl:
	case rpc::node_inquiry:
	case rpc::node_regist:
	case rpc::ll_exec_client:
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

template <class FB>
int rpc::basic_fiber::send_error(int err)
{
	PREPARE_PACK(super::ff().sr());
	rpc::response::pack_header(super::ff().sr(), m_msgid);
	super::ff().sr().pushnil();
	super::ff().sr() << err;
	return respond<FB>(true, super::ff().sr());
}

}
#endif
