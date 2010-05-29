#include "common.h"
#include "proto.h"
#include "world.h"

using namespace pfm;

int world::init(int max_node, int max_replica)
{
	if (!(m_ch = nbr_conhash_init(NULL, max_node, max_replica))) {
		return NBR_EMALLOC;
	}
	if (!m_nodes.init(max_node, max_node / 10, -1, opt_expandable | opt_threadsafe)) {
		nbr_conhash_fin(m_ch);
		return NBR_EEXPIRE;
	}
	return NBR_OK;
}

void world::fin()
{
	if (m_wid) { free((void *)m_wid); }
	m_nodes.fin();
	nbr_conhash_fin(m_ch);
}

void *world::connect_assigned_node(class connector_factory &cf, const UUID &uuid)
{
#if 0
	conn *c = NULL;
	bool retry_f = false;
	const CHNODE *n[vmprotocol::vmd_object_multiplexity];
	int n_node;
retry:
	if ((n_node = lookup_node(uuid, n,
		vmprotocol::vmd_object_multiplexity)) < 0) {
		ASSERT(false);
		return NULL;
	}
	for (int i = 0 ; i < n_node; i++) {
		if (!(c = cf.connect(uuid, n[i]->iden))) {
			del_node(*(n[i]));
			if (!retry_f) {
				retry_f = true;
				goto retry;
			}
			ASSERT(false);
			return NULL;
		}
		TRACE("object replicate addr <%s>\n", n[i]->iden);
	}
	return c;
#else
	return NULL;
#endif
}

CHNODE *world::add_node(const char *addr)
{
	CHNODE *n;
	if ((n = m_nodes.find(addr))) { return n; }
	if (!(n = m_nodes.create(addr))) { return NULL; }
	nbr_conhash_set_node(n, addr, vnode_replicate_num);
	int r = add_node(*n);
	if (r < 0 && r != NBR_EALREADY) {
		del_node(*n);
		m_nodes.erase(addr);
		return NULL;
	}
	return n;
}

int world::del_node(const char *addr)
{
	CHNODE *n = m_nodes.find(addr), nn;
	if (!n) {
		nbr_conhash_set_node(&nn, addr, vnode_replicate_num);
		del_node(nn);
	}
	else {
		del_node(*n);
	}
	m_nodes.erase(addr);
	return NBR_OK;
}

