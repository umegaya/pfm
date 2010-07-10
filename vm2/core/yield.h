#if !defined(__YIELD_H__)
#define __YIELD_H__

#include "proto.h"
#include <stdarg.h>

namespace pfm {

class yield {
public:
	typedef int (*callback)(rpc::response &, void *, void *);
protected:
	U16 m_size, m_reply;
	class fiber *m_fb;
	time_t m_start;
	MSGID m_msgid;
	callback m_fn;
	void *m_p;
	THREAD m_thrd;
public:
	yield() {}
	int init(class fiber *fb, MSGID id, int size,
			callback fn = NULL, void *p = NULL) {
		m_fb = fb;
		m_size = size;
		m_reply = 0;
		m_msgid = id;
		m_start = time(NULL);
		m_fn = fn;
		m_p = p;
		m_thrd = get_current_thrd(fb);
		return NBR_OK;
	}
	template <class FROM>
	static callback get_cb(int (*fn)(rpc::response &, FROM, void *)) {
		return (callback)fn;
	}
	int call_cb(rpc::response &res, SWKFROM *f) {
		return m_fn(res, f->p, m_p);
	}
	template <class FROM>
	int call_cb(rpc::response &res, FROM f) {
		return m_fn(res, (void *)f, m_p);
	}
	template <class FROM>
	int reply(FROM from, rpc::response &res) {
		if (finished()) { return NBR_OK; }
		ASSERT(m_reply < m_size);
		/* failure -> quit waiting all reply */
		if (!res.success()) {
			force_finish();
			return NBR_OK;
		}
		if (m_fn && call_cb(res, from) < 0) {
			return NBR_OK;
		}
		m_reply++;
		return (m_reply >= m_size ? NBR_OK : NBR_ESHORT);
	}
	THREAD get_current_thrd(class fiber *fb);
	bool finished() { return (m_reply >= m_size); }
	void force_finish() { m_reply = m_size; }
	class fiber *fb() { return m_fb; }
	int size() const { return m_size; }
	MSGID msgid() const { return m_msgid; }
	THREAD attached() const { return m_thrd; }
	inline bool timeout(time_t now, time_t span) const {
		return (now >= (m_start + span)); }
	template <typename T> T *p() { return (T *)m_p; }
};

}

#endif
