#if !defined(__FIBER_H__)
#define __FIBER_H__

#include "common.h"
#include "proto.h"
#include "ll.h"

namespace pfm {
using namespace sfc;
using namespace sfc::util;

class fiber {
protected:
	union {
		ll::coroutine *co;
	} m_ctx;
	U8 m_cmd, padd[3];
public:
	fiber() {}
	~fiber() {}
	inline int call(rpc::request &req);
	inline int resume(rpc::response &res);
	inline int respond(bool err, serializer &sr);
};

class fiber_factory : public map<fiber, MSGID>
{
protected:
	msgid_generator m_seed;
	int m_max_rpc;
	NBR_STLS ll *m_vm;
	NBR_STLS serializer *m_sr;
	class object_factory &m_of;
	class world_factory &m_wf;
public:
	typedef map<fiber, MSGID> super;
	fiber_factory(class object_factory &of, class world_factory &wf) :
		m_seed(), m_max_rpc(0), m_of(of), m_wf(wf) {}
	~fiber_factory() { fin(); }
 	inline bool initialized() { return m_vm != NULL; }
	bool init(int max_rpc);
	bool init_tls();
	void fin();
	void fin_tls();
	MSGID new_msgid() { return m_seed.new_id(); }
	fiber *fiber_new(MSGID msgid) { return super::create(msgid); }
	fiber *find_fiber(MSGID msgid) { return super::find(msgid); }
	inline int call(rpc::request &req);
	inline int resume(rpc::response &res);
};


/* inline functions */
inline int
fiber_factory::call(rpc::request &req)
{
	/* init TLS: jemalloc flaver */
	if (!initialized() && !init_tls()) {
		ASSERT(false);
		return NBR_EINVAL;
	}
	fiber *f = fiber_new(req.msgid());
	if (!f) {
		ASSERT(false);
		return NBR_EEXPIRE;
	}
	return f->call(req);
}

inline int
fiber_factory::resume(rpc::response &res)
{
	fiber *f = find_fiber(res.msgid());
	if (!f) {	/* would be destroyed by timeout */
		return NBR_ENOTFOUND;
	}
	return f->resume(res);
}

inline int
fiber::call(rpc::request &req)
{
	m_cmd = (U8)(U32)req.method();
	switch(m_cmd) {
	case rpc::ll_exec:
	case rpc::create_object:
	case rpc::replicate:
	case rpc::login:
	case rpc::create_world:
	default:
		ASSERT(false);
		break;
	}
}

inline int
fiber::resume(rpc::response &res)
{
	switch(m_cmd) {
	case rpc::ll_exec:
	case rpc::create_object:
	case rpc::replicate:
	case rpc::login:
	case rpc::create_world:
	default:
		ASSERT(false);
		break;
	}
}

inline int
fiber::respond(bool err, serializer &sr)
{
	switch(m_cmd) {
	case rpc::ll_exec:
	case rpc::create_object:
	case rpc::replicate:
	case rpc::login:
	case rpc::create_world:
	default:
		ASSERT(false);
		break;
	}
}


}
#endif
