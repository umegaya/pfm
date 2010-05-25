#if !defined(__TC_H__)
#define __TC_H__

#include <tcadb.h>
#include "nbr.h"

/* tokyocabinet */
class tc {
protected:
	static TCADB *m_db;
public:
	static int init(const char *opt) {
		if (!(m_db = tcadbnew())) {
			return NBR_EMALLOC;
		}
		if (!(tcadbopen(m_db, opt))) {
			return NBR_ESYSCALL;
		}
		return NBR_OK;
	}
	static void fin() {
		if (m_db) {
			tcadbclose(m_db);
			tcadbdel(m_db);
			m_db = NULL;
		}
	}
	static void *get(const void *k, int kl, int &vl) {
		return tcadbget(m_db, k, kl, &vl);
	}
	static bool put(const void *v, int vl, const void *k, int kl) {
		return tcadbput(m_db, v, vl, k, kl);
	}
};

#endif

