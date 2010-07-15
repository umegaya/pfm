#if !defined(__TC_H__)
#define __TC_H__

#include <tchdb.h>
#include "nbr.h"
#include <tcutil.h>

/* tokyocabinet */
class tc {
protected:
	TCHDB *m_db;
public:
	struct record {
		void *m_p;
		int m_len;
	public:
		record() {}
		~record() { nbr_free(m_p); }
		operator bool () const { return m_p != NULL; }
		template <typename T> T *p() { return (T *)m_p; }
		int len() const { return m_len; }
	};
	~tc() { fin(); }
	int init(const char *opt) {
		if (!(m_db = tchdbnew())) {
			return NBR_EMALLOC;
		}
		if (!(tchdbopen(m_db, opt, 
			HDBOCREAT | HDBOREADER | HDBOWRITER))) {
			return NBR_ESYSCALL;
		}
		return NBR_OK;
	}
	void fin() {
		if (m_db) {
			tchdbclose(m_db);
			tchdbdel(m_db);
			m_db = NULL;
		}
	}
	inline int rnum() const {
		ASSERT(m_db);
		return tchdbrnum(m_db);
	}
	void clear() {
		if (m_db) {
			tchdbvanish(m_db);
			tchdbsync(m_db);
		}
	}
	inline void *select(const void *k, int kl, int &vl) {
		ASSERT(m_db);
		return tchdbget(m_db, k, kl, &vl);
	}
	inline int select(const void *k, int kl, void *v, int vl) {
		ASSERT(m_db);
		return tchdbget3(m_db, k, kl, v, vl);
	}
	inline bool select(record &r, const void *k, int kl) {
		ASSERT(m_db);
		r.m_p = select(k, kl, r.m_len);
		return r;
	}
	inline void free(void *p) { nbr_free(p); }
	template <class T> T *select(const void *k, int kl, int &vl) {
		ASSERT(m_db);
		return (T *)select(k, kl, vl);
	}
	inline bool replace(const void *v, int vl, const void *k, int kl) {
		ASSERT(m_db);
		return tchdbput(m_db, v, vl, k, kl);
	}
	inline bool insert(const void *v, int vl, const void *k, int kl, bool &exists) {
		ASSERT(m_db);
		bool b = tchdbputkeep(m_db, v, vl, k, kl);
		if (!b) { exists = (tchdbecode(m_db) == TCEKEEP); }
		else { exists = false; }
		return b;
	}
	inline bool del(const void *k, int kl) {
		ASSERT(m_db);
		return tchdbout(m_db, k, kl);
	}
	template <class FUNC>
	bool iterate(FUNC fn) {
		if (!tchdbiterinit(m_db)) { return false; }
		char *k; int ksz;
		while ((k = (char *)tchdbiternext(m_db, &ksz))) {
			if (fn(this, k, ksz) < 0) {
				return false;
			}
			tcfree(k);
		}
		return true;
	}
};

#endif

