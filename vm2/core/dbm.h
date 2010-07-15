#if !defined(__DBM_H__)
#define __DBM_H__

#include "common.h"
#include "tc.h"
namespace pfm {
typedef tc dbm_impl;
class dbm : public dbm_impl {};
using namespace sfc;
/* persistent map with dbm */
template <class V, class K>
class pmap : protected sfc::util::map<V,K> {
protected:
	dbm	m_db;
public:
	pmap() : m_db() {}
	~pmap() {}
	bool init(int max, int hashsz, int opt, const char *dbmopt) {
		if (!util::map<V,K>::init(max, hashsz, -1, opt)) {
			return false; 
		}
		if (m_db.init(dbmopt) < 0) {
			fin();
			return false;
		}
		return true;
	}
	void fin() {
		sfc::util::map<V,K>::fin();
		m_db.fin();
	}
	typedef sfc::util::map<V,K> super;
	typedef typename super::key key;
	typedef typename super::value value;
	typedef typename super::element element;
	typedef typename super::retval retval;
	typedef typename super::key_traits key_traits;
	static const U32 save_work_buffer_size = 16 * 1024;
	class record {
	protected:
		element *m_e;
	public:
		record(element *e) : m_e(e) {}
		inline int load(const char *p, int l, void *ctx) { return m_e->get()->load(p, l, ctx); }
		inline int save(char *&p, int &l, void *ctx) { return m_e->get()->save(p, l, ctx); }
		inline bool operator == (element *e) const { return m_e == e; }
		inline bool operator != (element *e) const { return m_e != e; }
		inline bool operator ! () const { return !m_e; }
		inline operator bool () const { return m_e != NULL; }
		inline retval &operator * () { return *(m_e->get()); }
		inline retval *operator -> () { return m_e->get(); }
		inline operator retval *() { return m_e->get();}
	};
	static inline record to_r(retval *v) { return record(element::to_e(v)); }
	int cachesize() const { return super::use(); }
	int rnum() const { return m_db.rnum(); }
	void clear() { m_db.clear(); }
	inline record find(key k) {	/* find on memory (loaded record) */
		return record(super::findelem(k));
	}
	inline record load(key k, void *ctx = NULL) {
		/* find disk and load it and cache on memory */
		bool exists;
		element *e;
		if (!(e = super::rawalloc(k, false, &exists))) { return record(NULL); }
		if (exists) { return record(e); }
		record rec(e);
		dbm::record r;
		m_db.select(r, key_traits::kp(k), key_traits::kl(k));
		if (!r || rec.load(r.p<char>(), r.len(), ctx) < 0) {
			erase(k);
			return record(NULL);
		}
		return rec;
	}
	inline record load(key k, bool &exist_on_disk, void *ctx = NULL) {
		/* find disk and load it and cache on memory */
		bool exists;
		element *e;
		if (!(e = super::rawalloc(k, false, &exists))) { return record(NULL); }
		if (exists) { return record(e); }
		record rec(e);
		dbm::record r;
		m_db.select(r, key_traits::kp(k), key_traits::kl(k));
		exist_on_disk = (r && rec.load(r.p<char>(), r.len(), ctx) >= 0);
		return rec;
	}
	inline bool save_from_ptr(retval *v, key k,
		bool insertion = false, void *ctx = NULL) {
		record r = to_r(v);
		return save(r, k, insertion, ctx);
	}
	inline bool save_raw(key k, const char *v, int vl) {
		return m_db.replace(key_traits::kp(k), key_traits::kl(k), v, vl);
	}
	inline bool save(record &r, key k,
		bool insertion = false, void *ctx = NULL) {
		char buffer[save_work_buffer_size];
		char *v = buffer; int vl = sizeof(buffer);
		bool exists, res = (r.save(v, vl, ctx) >= 0 && (insertion ?
			m_db.insert(key_traits::kp(k), key_traits::kl(k), v, vl, exists) :
			m_db.replace(key_traits::kp(k), key_traits::kl(k), v, vl)));
		if (buffer != v) {
			nbr_free(v);
		}
		return res;
	}
	inline record insert(value v, key k, void *ctx) {
		/* insert new record with initialization */
		element *e;
		if (!(e = super::rawalloc(k, true, NULL))) { return record(NULL); }
		e->set(v);
		record r(e);
		if (!save(r, k, true, ctx)) {
			super::erase(k);
			return record(NULL);
		}
		return r;
	}
	inline record create(key k, void *ctx) {
		/* create record (if already exists, return NULL) */
		element *e;
		if (!(e = super::rawalloc(k, true, NULL))) { return record(NULL); }
		record r(e);
		if (!save(r, k, true, ctx)) {
			super::erase(k);
			return record(NULL);
		}
		return r;
	}
	inline void destroy(key k) {	/* destroy record */
		m_db.del(key_traits::kp(k), key_traits::kl(k));
		super::erase(k);
	}
	inline void unload(key k, void *ctx) {
		/* remove record from memory (still exist on disk) */
		record r = find(k);
		if (r) {
			save(r, k, false, ctx);
			super::erase(k);
		}
	}
	template <class FUNC>
	bool iterate(FUNC fn) {
		return m_db.iterate<FUNC>(fn);
	}
};

}

#endif

