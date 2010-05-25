#if !defined(__FIBER_H__)
#define __FIBER_H__

#include "nbr.h"
#include "proto.h"
#include "ll.h"

namespace pfm {
using namespace sfc;

class msgid_generator {
protected:
	U32 m_msgid_seed;
	static const U32 MSGID_LIMIT = 2000000000;
	static const U32 MSGID_COMPACT_LIMIT = 60000;
public:
	msgid_generator() : m_msgid_seed(0) {}
	inline MSGID new_id() {
		__sync_val_compare_and_swap(&m_msgid_seed, MSGID_LIMIT, 0);
		return __sync_add_and_fetch(&m_msgid_seed, 1);
	}
	inline CMSGID compact_new_id() {
		__sync_val_compare_and_swap(&m_msgid_seed, MSGID_COMPACT_LIMIT, 0);
		return __sync_add_and_fetch(&m_msgid_seed, 1);
	}
};

class fiber : public ll::coroutine {
protected:
	NBR_TLS ll *m_vm;
	ll::coroutine *m_co;
	static msgid_generator m_seed;
public:
	fiber() : m_co(NULL) {}
	~fiber() {}
	static MSGID new_msgid() { return m_seed.new_id(); }
	static CMSGID new_cmsgid() { return m_seed.compact_new_id(); }
	int call(rpc::ll_request &req) { return m_co->call(req); }
	int resume(rpc::ll_response &res) { return m_co->resume(res); }
	int response(bool err, serializer &resp) { return NBR_OK; }
};

}
#endif
