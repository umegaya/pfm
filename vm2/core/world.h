#if !defined(__WORLD_H__)
#define __WORLD_H__

#include "sfc.hpp"
#include "uuid.h"
#include "proto.h"

namespace pfm {
using namespace sfc;
using namespace sfc::util;
class world  {
protected:
	CONHASH m_ch;
	world_id m_wid;
	UUID m_world_object;
	map<CHNODE, address> m_nodes;
	class connector_factory *m_cf;
public:
	typedef map<CHNODE, char*>::iterator iterator;
	static const U32 vnode_replicate_num = 30;
	static const U32 object_multiplexity = 3;
	static const U32 max_world = 1000;
	world(class connector_factory *cf = NULL) :
		m_ch(NULL), m_wid(NULL), m_world_object(), m_nodes(), m_cf(cf) {}
	~world() { fin(); }
	int init(int max_node, int max_replica);
	void fin();
	world_id set_id(world_id wid) { return m_wid = strndup(wid, max_wid); }
	world_id id() const { return m_wid; }
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
	static inline const char *node_addr(CHNODE &n) { return n.iden; }
	map<CHNODE, address> &nodes() { return m_nodes; }
	class connector_factory &cf() { return *m_cf; }
	void set_cf(class connector_factory *cf) { m_cf = cf; }
	CHNODE *add_node(const address &addr);
	int del_node(const address &addr);
	int add_node(const CHNODE &n) {
		return nbr_conhash_add_node(m_ch, (CHNODE *)&n); }
	int del_node(const CHNODE &n) {
		return nbr_conhash_del_node(m_ch, (CHNODE *)&n); }
	int request(MSGID msgid, const UUID &uuid, serializer &sr);
#if defined(_TEST)
	static int (*m_test_request)(world *, MSGID, const UUID &, serializer &);
	void *_connect_assigned_node(const UUID &uuid) {
		return connect_assigned_node(uuid);
	}
#endif
	int save(char *&p, int &l) {
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
	int load(const char *p, int l) {
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
	world_factory(class connector_factory *cf, const char *dbmopt) :
		pmap<world, world_id>(), m_cf(cf) {
		bool b = super::init(64, 64, opt_expandable | opt_threadsafe, dbmopt);
		assert(b);	/* if down here, your machine is not suitable to use it */
	}
	world_factory() : pmap<world, world_id>() {}
	~world_factory() { super::fin(); }
	class connector_factory *cf() { return m_cf; }
	void set_cf(class connector_factory *cf) { m_cf = cf; }
	void remove_node(address &a) {
		super::iterator it = begin();
		for (; it != super::end(); it = super::next(it)) {
			it->del_node(a);
		}
	}
	world *create(world_id wid, int max_node, int max_replica) {
		world *w = super::load(wid);
		if (w) { 
			w->set_cf(m_cf); 
			return w; 
		}
		if (!(w = super::create(wid))) { return NULL; }
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
};
}

#endif

