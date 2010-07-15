#if !defined(__OBJECT_H__)
#define __OBJECT_H__

#include "common.h"
#include "uuid.h"
#include "msgid.h"

namespace pfm {
using namespace sfc;
class object {
	UUID		m_uuid;		/* unique ID for each object */
	U32		m_flag;		/* object flag */
	const char 	*m_klass;	/* object class */
	class ll 		*m_vm;		/* vm which assigned to this object */
	class world 	*m_wld;		/* WORLD which this object belongs to */
public:
	enum {
		flag_local 			= 0x00000001,/* this node is to handle this object
										decided by consistent hash */
		flag_loaded 		= 0x00000002,/* usually used with
										flag_local or flag_cached_local
										means need to setup big metatable
										for its property. once metatable setup,
										this flag will be off. */
		flag_collected 		= 0x00000004,/* when garbage collected,
										this flag on */
		flag_cached_local 	= 0x00000008,/* actually this node not assigned to
										this object, but for performance issue,
										it loads all data into here and treated as
										local. */
		flag_has_localdata	= 0x00000010,/* basically used by client framework.
										if this flag on, object can store its local
										property in this node (but never saved)
										when referring property to
										has_localdata object occur, first search for
										its local property, and if not found, then
										rpc call happen. client object should have
										this attribute. */
		flag_replica = 0x00000020,		/* means replicated */
	};
public:
	object() : m_uuid(), m_flag(0), m_klass("Unknown"), 
		m_vm(NULL), m_wld(NULL) {}
	~object() {}
public:
	const UUID &uuid() const { return m_uuid; }
	class ll *vm() const { return m_vm; }
	class world *belong() { return m_wld; }
	const char *klass() const { return m_klass; }
	const class world *belong() const { return m_wld; }
	const world *belong_to(class world *w) {
		return __sync_val_compare_and_swap(&m_wld, NULL, w); }
	void set_uuid(const UUID &uuid) { m_uuid = uuid; }
	void set_vm(class ll *vm) { m_vm = vm; }
	void set_klass(const char *klass) { m_klass = klass; }
public:
	bool thread_current(class ll *vm) { return m_vm == vm; }
	bool method_callable(class ll *vm) { return local() && thread_current(vm); }
	bool attr_accesible(class ll *vm) {
		return (local() || has_localdata()) && thread_current(vm);
	}
	void set_flag(U32 f, bool on) {
		if (on) { m_flag |= f; } else { m_flag &= ~(f); } }
	bool local() const { return m_flag & flag_local; }
	bool cached_local() const { return m_flag & flag_cached_local; }
	bool has_localdata() const { return m_flag & flag_has_localdata; }
	bool loaded() const { return m_flag & flag_loaded; }
	bool collected() const { return m_flag & flag_collected; }
	bool replica() const { return m_flag & flag_replica; }
public:
	int save(char *&p, int &l, void *ctx);
	int load(const char *p, int l, void *ctx);
	int request(MSGID msgid, class ll *vm, class serializer &sr);
#if defined(_TEST)
	char m_buffer[256];
	static int (*m_test_save)(class object *, char *&, int &);
	static int (*m_test_load)(class object *, const char *, int);
	static int (*m_test_request)
		(class object *, MSGID, class ll *, class serializer &);
	char *buffer() { return m_buffer; }
#endif
};

using namespace sfc::util;
class object_factory : public pmap<object, UUID> {
protected:
	static const size_t max_klass_symbol = 64;
	map<const char *, char[max_klass_symbol]> m_syms;
	const char *create_symbol(const char *sym) {
		const char *p = m_syms.find(sym);
		if (p) { return p; }
		if (!(p = strndup(sym, max_klass_symbol))) {
			ASSERT(false); return p;
		}
		if (m_syms.insert(p, sym) == m_syms.end()) {
			ASSERT(false); return p;
		}
		TRACE("allocate symbol (%s/%p)\n", p, p);
		return p;
	}
	THPOOL m_replacer;
public:
	typedef pmap<object, UUID> super;
	typedef super::record record;
	typedef super::key key;
	object_factory() : super(), m_syms() {}
	~object_factory() {}
	bool init(int max, int hashsz, int opt, const char *dbmopt);
	void fin();
	record load(const UUID &uuid, void *co,
		class world *w, class ll *vm, const char *klass) {
		super::element *e;
		bool exists;
		if (!(e = super::rawalloc(uuid, false, &exists))) { return record(NULL); }
		if (exists) { return record(e); }
		record rec(e);
		dbm::record r;
		bool ret = m_db.select(r,(void *)&uuid, sizeof(uuid));
		rec->belong_to(w);
		rec->set_uuid(uuid);
		rec->set_vm(vm);
		rec->set_klass(create_symbol(klass));
		if (ret) {
			rec->set_flag((object::flag_local | object::flag_loaded), true);
			if (rec.load(r.p<char>(), r.len(), co) < 0) {
				erase(uuid);
				return record(NULL);
			}
		}
		return rec;
	}
	record create(const UUID &uuid, void *co,
			class world *w, class ll *vm, const char *klass) {
		/* this line will be done atomically */
		super::element *e;
		if (!(e = super::rawalloc(uuid, true, NULL))) { return record(NULL); }
		record r(e);
		/* so only 1 thread can come here, never caused inconsistency */
		r->set_uuid(uuid);
		r->belong_to(w);
		r->set_vm(vm);
		r->set_klass(create_symbol(klass));
		if (!super::save(r, uuid, true, co)) {
			super::erase(uuid);
			return record(NULL);
		}
		r->set_flag((object::flag_local | object::flag_loaded), true);
		return r;
	}
	bool save(object *o, void *co) {
		record r((super::element *)o);
		return super::save(r, o->uuid(), false, co);
	}
	bool save_raw(const UUID &uuid, char *p, int l) {
		return super::save_raw(uuid, p, l);
	}
	bool start_rehasher(class rehasher *param);
};
}

#endif
