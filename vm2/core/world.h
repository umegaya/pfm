#if !defined(__WORLD_H__)
#define __WORLD_H__

#include "sfc.hpp"
#include "uuid.h"

namespace pfm {
using namespace sfc;
using namespace sfc::util;
class world  {
protected:
	CONHASH m_ch;
	world_id m_wid;
	UUID m_world_object;
	map<CHNODE, char*> m_nodes;
	static const U32 vnode_replicate_num = 30;
	static const U32 max_wid = 256;
public:
	typedef map<CHNODE, char*>::iterator iterator;
	world() : m_ch(NULL), m_wid(NULL), m_world_object(), m_nodes() {}
	~world() { fin(); }
	int init(int max_node, int max_replica);
	void fin();
	world_id set_id(world_id wid) { return m_wid = strndup(wid, max_wid); }
	world_id id() const { return m_wid; }
	const UUID &world_object_uuid() const { return m_world_object; }
	void set_world_object_uuid(const UUID &uuid) { m_world_object = uuid; }
	int lookup_node(const UUID &uuid, const CHNODE *n[], int n_max) {
		*n = nbr_conhash_lookup(m_ch, (const char *)&uuid, sizeof(uuid));
		return (*n) ? 1 : NBR_ENOTFOUND;
	}
	map<CHNODE, char*> &nodes() { return m_nodes; }
	CHNODE *add_node(const char *addr);
	int del_node(const char *addr);
	void *connect_assigned_node(class connector_factory &cf, const UUID &uuid);
	int add_node(const CHNODE &n) {
		return nbr_conhash_add_node(m_ch, (CHNODE *)&n); }
	int del_node(const CHNODE &n) {
		return nbr_conhash_del_node(m_ch, (CHNODE *)&n); }
};

class world_factory : public map<world, world_id> {
public:
	typedef map<world, world_id> super;
	world_factory() : map<world, world_id>() {
		bool b = super::init(64, 64, -1, opt_expandable | opt_threadsafe);
		assert(b);	/* if down here, your machine is not suitable to use it */
	}
	void remove_node(address &a) {
		super::iterator it = begin();
		for (; it != super::end(); it = super::next(it)) {
			it->del_node(a);
		}
	}
	world *create(world_id wid, int max_node, int max_replica) {
		world *w = super::create(wid);
		if (!w) { return NULL; }
		if (w->init(max_node, max_replica) < 0) {
			super::destroy(w);
			return NULL;
		}
		if (!w->set_id(wid)) {
			super::destroy(w);
			return NULL;
		}
		return w;
	}
	void destroy(world_id wid) { super::erase(wid); }
};
}

#endif

