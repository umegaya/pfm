#if !defined(__MSGID_H__)
#define __MSGID_H__

namespace pfm {

class msgid_generator {
protected:
	volatile U32 m_msgid_seed;
	static const U32 MSGID_NORMAL_LIMIT = 2000000000;
	static const U32 MSGID_COMPACT_LIMIT = 60000;
public:
	typedef U32 NMSGID;
	typedef U16 CMSGID;
	msgid_generator() : m_msgid_seed(0) {}
	inline NMSGID normal_new_id() {
		__sync_val_compare_and_swap(&m_msgid_seed, MSGID_NORMAL_LIMIT, 0);
		return __sync_add_and_fetch(&m_msgid_seed, 1);
	}
	inline CMSGID compact_new_id() {
		__sync_val_compare_and_swap(&m_msgid_seed, MSGID_COMPACT_LIMIT, 0);
		return __sync_add_and_fetch(&m_msgid_seed, 1);
	}
#if !defined(_USE_COMPACT_MSGID)
	typedef NMSGID MSGID;
	static const U32 MSGID_LIMIT = MSGID_NORMAL_LIMIT;
	inline MSGID new_id() { return normal_new_id(); }
#else
	typedef CMSGID MSGID;
	static const U32 MSGID_LIMIT = MSGID_COMPACT_LIMIT;
	inline CMSGID new_id() { return compact_new_id(); }
#endif
	static inline int compare_msgid(MSGID msgid1, MSGID msgid2) {
		if (msgid1 < msgid2) {
			/* if diff of msgid2 and msgid1, then msgid must be turn around */
			return ((msgid2 - msgid1) < (MSGID_LIMIT / 2)) ? -1 : 1;
		}
		else if (msgid1 > msgid2){
			return ((msgid1 - msgid2) <= (MSGID_LIMIT / 2)) ? 1 : -1;
		}
		return 0;
	}
	MSGID seedval() { return m_msgid_seed; }
};
/* typedefs */
typedef msgid_generator::MSGID MSGID;
static const MSGID INVALID_MSGID = 0;
}

#endif
