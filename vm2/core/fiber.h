#if !defined(__FIBER_H__)
#define __FIBER_H__

#include "common.h"
#include "proto.h"
#include "ll.h"

namespace pfm {
using namespace sfc;
using namespace sfc::util;

template <class FB>
class fiber_factory;

class fiber {
protected:
	enum {
		start = 0,		/* for all status */
		/* ll_exec, create_object */
			/*have no fiber status */
		/* create_world  */
			create_world_initialize = 1,
		/* login */
		/* replicate */
		end = 255,	/* for all status */
	};
	union {
		ll::coroutine *co;	/* ll_exec, creat_object */
	} m_ctx;
	U8 m_cmd, m_status, padd[2];
	MSGID m_msgid;
	world_id m_wid;
	time_t m_start;
	class ffutil *m_ff;
public:
	fiber() : m_status(start), m_start(time(NULL)), m_ff(NULL) {}
	TEST_VIRTUAL ~fiber() {}
	world_id wid() const { return m_wid; }
	MSGID msgid() const { return m_msgid; }
	void set_ff(class ffutil *ffu) { m_ff = ffu; }
	class ffutil &ff() { return *m_ff; }
	template <class FB> operator FB*() { return (FB *)this; }
	inline void fin();
	inline bool timeout(time_t now, time_t span) const {
		return (now >= (m_start + span)); }
	inline void finish() { m_status = end; }
	inline bool finished() const { return m_status == end; }
	inline int send_error(int err);
	inline MSGID new_msgid();
	template <class FB>
		void terminate(fiber_factory<FB> &ff, int err);
	template <class FB>
		int call(fiber_factory<FB> &ff, rpc::request &req, bool trusted);
	template <class FB>
		int resume(fiber_factory<FB> &ff, rpc::response &res);
	TEST_VIRTUAL int respond(bool err, serializer &sr) { return NBR_OK; }
protected:
	int call_create_world(rpc::request &req);
	int resume_create_world(rpc::response &res);
	int call_login(rpc::request &req) { ASSERT(false); return NBR_ENOTSUPPORT; }
	int resume_login(rpc::response &res) { ASSERT(false); return NBR_ENOTSUPPORT; }
	int call_replicate(rpc::request &req) { ASSERT(false); return NBR_ENOTSUPPORT; }
	int resume_replicate(rpc::response &res) { ASSERT(false); return NBR_ENOTSUPPORT; }
protected:
	world_id get_world_id(rpc::request &req) {
		return rpc::world_request::cast(req).wid();
	}
};

class ffutil
{
protected:
	msgid_generator m_seed;
	int m_max_rpc;
	int m_max_node, m_max_replica;	/* world setting */
	NBR_STLS ll *m_vm;
	NBR_STLS serializer *m_sr;
	map<fiber*, MSGID>	m_fm;
	class object_factory &m_of;
	class world_factory &m_wf;
public:
	typedef map<fiber, MSGID> super;
	ffutil(class object_factory &of, class world_factory &wf) :
		m_seed(), m_max_rpc(0), m_max_node(0), m_max_replica(0),
		m_of(of), m_wf(wf) {}
	~ffutil() {}
 	inline bool initialized() { return m_vm != NULL; }
	int init(int max_node, int max_replica);
	bool init_tls();
	void fin_tls();
	void fin();
public:
	class object_factory &of() { return m_of; }
	ll *vm() { return m_vm; }
	serializer &sr() { return *m_sr; }
	MSGID new_msgid() { return m_seed.new_id(); }
	class world *world_new(const rpc::create_world_request &req);
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
};

template <class FB>
class fiber_factory : public array<FB>, public ffutil
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
		if ((r = ffutil::init(max_node, max_replica)) < 0) { return r; }
		return super::init(m_max_rpc, -1, opt_threadsafe | opt_expandable) ? 
			NBR_OK : NBR_EMALLOC;
	}
	void fin() {
		super::fin();
	}
	FB *fiber_new() {
		FB *f = super::create();
		if (f) { f->set_ff(this); }
		return f;
	}
	void fiber_destroy(FB *f) { super::destroy(f); }
	FB *find_fiber(MSGID msgid) { return (FB *)util::m_fm.find(msgid); }
	int call(rpc::request &req, bool trusted);
	int resume(rpc::response &res);
};

/* inline functions */
template <class FB> int
fiber_factory<FB>::call(rpc::request &req, bool trusted)
{
	/* init TLS: jemalloc flaver */
	if (!util::initialized() && !util::init_tls()) {
		ASSERT(false);
		return NBR_EINVAL;
	}
	FB *f = fiber_new();
	if (!f) {
		ASSERT(false);
		return NBR_EEXPIRE;
	}
	int r = f->call(*this, req, trusted);
	if (f->finished()) {
		f->fin();
		fiber_destroy(f);
	}
	return r;
}

template <class FB> int
fiber_factory<FB>::resume(rpc::response &res)
{
	FB *f = find_fiber(res.msgid());
	if (!f) {	/* would be destroyed by timeout */
		return NBR_ENOTFOUND;
	}
	fiber_unregister(res.msgid());
	int r = f->resume(*this, res);
	if (f->finished()) {
		f->fin();
		fiber_destroy(f);
	}
	return r;
}

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
	return ff().fiber_register(msgid, this) ? msgid : INVALID_MSGID;
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
		return ((FB*)*this)->call_login(req);
	default:
		if (trusted) {
			switch(m_cmd) {
			case rpc::create_object:
				m_wid = get_world_id(req);
				if (!(m_ctx.co = ff.co_create(this))) { return NBR_EEXPIRE; }
				return m_ctx.co->call(rpc::create_object_request::cast(req));
			case rpc::replicate:
				/* replicate not contain world ID */
				return ((FB*)*this)->call_replicate(req);
			case rpc::create_world:
				m_wid = get_world_id(req);
				return ((FB*)*this)->call_create_world(req);
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
		return m_ctx.co->resume(rpc::ll_exec_response::cast(res));
	case rpc::replicate:
		return ((FB*)*this)->resume_replicate(res);
	case rpc::login:
		return ((FB*)*this)->resume_login(res);
	case rpc::create_world:
		return ((FB*)*this)->resume_create_world(res);
	default:
		ASSERT(false);
		break;
	}
}


inline void
fiber::fin()
{
	switch(m_cmd) {
	case rpc::ll_exec:
	case rpc::create_object:
		ff().vm()->co_destroy(m_ctx.co);
		m_ctx.co = NULL;
		break;
	case rpc::replicate:
	case rpc::login:
	case rpc::create_world:
		break;
	default:
		ASSERT(false);
		break;
	}
}



}
#endif
