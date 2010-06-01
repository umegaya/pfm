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
		flag_local = 0x00000001,
		flag_loaded = 0x00000002,
		flag_collected = 0x00000004,
	};
public:
	object() : m_uuid(), m_flag(0), m_klass("Unknown"), 
		m_vm(NULL), m_wld(NULL) {}
	~object() {}
	void set_uuid(const UUID &uuid) { m_uuid = uuid; }
	const UUID &uuid() const { return m_uuid; }
	class ll *vm() const { return m_vm; }
	class world *belong() { return m_wld; }
	const class world *belong() const { return m_wld; }
	bool thread_current(class ll *vm) { return m_vm == vm; }
	bool can_process_with(class ll *vm) { return local() && thread_current(vm); }
	void set_vm(class ll *vm) { m_vm = vm; }
	void set_klass(const char *klass) { m_klass = klass; }
	const world *belong_to(class world *w) {
		return __sync_val_compare_and_swap(&m_wld, NULL, w); }
	void set_flag(U32 f, bool on) {
		if (on) { m_flag |= f; } else { m_flag &= ~(f); } }
	bool local() const { return m_flag & flag_local; }
	bool remote() const { return !local(); }
	const char *klass() const { return m_klass; }
	bool loaded() const { return m_flag & flag_loaded; }
	bool collected() const { return m_flag & flag_collected; }
	int save(char *&p, int &l) { return l; }
	int load(const char *p, int l) { return NBR_OK; }
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
public:
	typedef pmap<object, UUID> super;
	typedef super::record record;
	typedef super::key key;
	object_factory() : super() {}
	~object_factory() {}
	record load(const UUID &uuid,
		class world *w, class ll *vm, const char *klass) {
		bool local;
		record r = super::load(uuid, local);
		if (!r) { return r; }
		/* this line will be done atomically */
		if (!r->belong_to(w)) {
			r->set_flag((object::flag_local | object::flag_loaded), local);
			r->set_uuid(uuid);
			r->set_vm(vm);
			r->set_klass(klass);
		}
		return r;
	}
	record create(const UUID &uuid,
			class world *w, class ll *vm, const char *klass) {
		/* this line will be done atomically */
		record r = super::create(uuid);
		if (r) { 
			/* so only 1 thread can come here, never caused inconsistency */
			r->set_uuid(uuid);
			r->set_flag((object::flag_local | object::flag_loaded), true); 
			r->belong_to(w);
			r->set_vm(vm);
			r->set_klass(klass);
		}
		return r;
	}
};

}

#endif
