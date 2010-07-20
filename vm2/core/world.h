#if !defined(__WORLD_H__)
#define __WORLD_H__

#include "sfc.hpp"
#include "uuid.h"
#include "proto.h"

namespace pfm {
using namespace sfc;
using namespace sfc::util;

class rehasher {
protected:
	class world *m_wld;
	class ffutil *m_ff;
	class serializer *m_sr;
	MSGID m_msgid;
	THREAD m_invoked, m_curr;
	volatile U32 *m_flag, m_data;
public:
	rehasher() : m_curr(NULL), m_flag(NULL) {}
	void init(class ffutil *ff, class world *w,
			THREAD invoke, MSGID msgid) {
		m_ff = ff; m_wld = w;
		m_invoked = invoke;
		m_msgid = msgid;
		if (!m_flag) { m_flag = &m_data; }
		m_data = 0;
		ASSERT(msgid != INVALID_MSGID);
	}
	int start();
	int yield(int timeout) {
		time_t start = time(NULL);
		while(__sync_bool_compare_and_swap(m_flag, 1, 0) == false) {
			::sched_yield();
			nbr_osdep_sleep(1000 * 1000 * 10);
			if ((time(NULL) - start) > timeout) { return NBR_ETIMEOUT; }
		}
		return NBR_OK;
	}
	THREAD set_thrd(THREAD t) { return m_curr = t; }
	THREAD curr() const { return m_curr; }
	void resume() { m_data = 1; }
	int operator () (dbm_impl *dbm, const char *k, int ksz);
	static void *proc(void *p);
};

class world  {
protected:
	CONHASH m_ch, m_nch;
	world_id m_wid;
	UUID m_world_object;
	rehasher m_rh;
	map<const CHNODE *, address> m_nodes;
	class connector_factory *m_cf;
public:
	typedef map<const CHNODE *, char*>::iterator iterator;
	static const U32 vnode_replicate_num = 30;
	static const U32 object_multiplexity = 3;
	static const U32 max_world = 1000;
	world(class connector_factory *cf = NULL) :
		m_ch(NULL), m_wid(NULL), m_world_object(), m_rh(), m_nodes(), m_cf(cf) {}
	~world() { fin(); }
	int init(int max_node, int max_replica);
	void fin();
	world_id set_id(world_id wid) { return m_wid = strndup(wid, max_wid); }
	world_id id() const { return m_wid; }
	rehasher &rh() { return m_rh; }
	const UUID &world_object_uuid() const { return m_world_object; }
	void set_world_object_uuid() {
		ASSERT(!m_world_object.valid());
		m_world_object.assign();
	}
	void set_world_object_uuid(const UUID &uuid) { m_world_object = uuid; }
	int lookup_node(const UUID &uuid, const CHNODE *n[], int n_max) {
		*n = nbr_conhash_lookup(m_ch, (const char *)&uuid, sizeof(uuid));
		return (*n) ? 1 : NBR_ENOTFOUND;
	}
	static inline const char *node_addr(const CHNODE &n) { return n.iden; }
	map<const CHNODE *, address> &nodes() { return m_nodes; }
	class connector_factory &cf() { return *m_cf; }
	void set_cf(class connector_factory *cf) { m_cf = cf; }
	bool primary_node_for(const UUID &uuid);
	bool node_for(const UUID &uuid);
	const CHNODE *add_node(const class conn &c);
	const CHNODE *add_node(const char *a);
	int del_node(const char *a);
	int add_node(const CHNODE &n) {
		return nbr_conhash_add_node(m_ch, (CHNODE *)&n); }
	int del_node(const CHNODE &n) {
		return nbr_conhash_del_node(m_ch, (CHNODE *)&n); }
	int request(MSGID msgid, const UUID &uuid, serializer &sr, bool recon = false);
	int re_request(MSGID msgid, const UUID &uuid, bool reconnect);
#if defined(_TEST)
	static int (*m_test_request)(world *, MSGID,
			const UUID &, address &a, serializer &);
	void *_connect_assigned_node(const UUID &uuid) {
		return connect_assigned_node(uuid);
	}
#endif
	int save(char *&p, int &l, void *) {
		int thissz = (int)sizeof(*this);
		if (l <= thissz) {
			ASSERT(false);
			if (!(p = (char *)nbr_malloc(thissz))) {
				return NBR_EMALLOC;
			}
			l = thissz;
		}
		memcpy(p, (void *)&m_world_object, sizeof(UUID));
		return sizeof(UUID);
	}
	int load(const char *p, int l, void *) {
		m_world_object = *(UUID *)p;
		return NBR_OK;
	}
protected:
	void *connect_assigned_node(const UUID &uuid);
};

class world_factory : public pmap<world, world_id> {
protected:
	class connector_factory *m_cf;
public:
	typedef pmap<world, world_id> super;
	typedef super::iterator iterator;
	world_factory(class connector_factory *cf, const char *dbmopt) :
		pmap<world, world_id>(), m_cf(cf) {
		bool b = super::init(64, 64, opt_expandable | opt_threadsafe, dbmopt);
		assert(b);	/* if down here, your machine is not suitable to use it */
	}
	world_factory() : pmap<world, world_id>() {}
	~world_factory() { super::fin(); }
	inline bool load(int max_node, int max_replica);
	class connector_factory *cf() { return m_cf; }
	void set_cf(class connector_factory *cf) { m_cf = cf; }
	iterator begin() { return super::begin(); }
	iterator end() { return super::end(); }
	iterator next(iterator i) { return super::next(i); }
	void remove_node(const char *addr) {
		iterator it = begin();
		for (; it != end(); it = next(it)) {
			it->del_node(addr);
		}
	}
	world *create(world_id wid, int max_node, int max_replica) {
		world *w = super::load(wid);
		if (!w && !(w = super::create(wid, NULL))) { return NULL; }
		if (w->init(max_node, max_replica) < 0) {
			super::destroy(wid);
			return NULL;
		}
		if (!w->set_id(wid)) {
			super::destroy(wid);
			return NULL;
		}
		w->set_cf(m_cf);
		return w;
	}
	void destroy(world_id wid) { super::destroy(wid); }
	int request(world_id wid, MSGID msgid,
		const UUID &uuid, serializer &sr) {
		world *w = super::find(wid);
		if (!w) { return NBR_ENOTFOUND; }
		return w->request(msgid, uuid, sr);
	}
	int re_request(world_id wid, MSGID msgid,
		const UUID &uuid, bool reconnect) {
		world *w = super::find(wid);
		if (!w) { return NBR_ENOTFOUND; }
		return w->re_request(msgid, uuid, reconnect);
	}
};
class world_loader {
	class world_factory &m_wf;
	int m_max_node, m_max_replica;
public:
	world_loader(class world_factory &wf,
		int max_node, int max_replica) :
		m_wf(wf), m_max_node(max_node),
		m_max_replica(max_replica) {}
	int operator () (dbm_impl *dbm, const char *k, int ksz) {
		world *w = m_wf.create(k, m_max_node, m_max_replica);
		if (!w) { return NBR_ENOTFOUND; }
		return NBR_OK;
	}
};

inline bool world_factory::load(int max_node, int max_replica) {
	world_loader ldr(*this, max_node, max_replica);
	return super::iterate(ldr);
}

}

#endif

