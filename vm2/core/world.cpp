#include "common.h"
#include "proto.h"
#include "world.h"
#include "connector.h"
#include "cp.h"

using namespace pfm;

#if defined(_TEST)
int (*world::m_test_request)(world *, MSGID, const UUID &, serializer &) = NULL;
#endif

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

void* world::connect_assigned_node(const UUID &uuid)
{
	connector *c = NULL;
	bool retry_f = false;
	const CHNODE *n[object_multiplexity];
	int n_node;
retry:
	if ((n_node = lookup_node(uuid, n, object_multiplexity)) < 0) {
		ASSERT(false);
		return NULL;
	}
	for (int i = 0 ; i < n_node; i++) {
		if (!(c = m_cf->connect(uuid, n[i]->iden))) {
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
	return (void *)c;
}

/* for servant : from addr string (not active connection) */
const CHNODE *world::add_node(const char *a)
{
	const CHNODE *n;
	map<const CHNODE *,const char *>::iterator i;
	connector_factory::failover_chain *ch;
	conn *c;
	if (!(ch = m_cf->insert(a, NULL))) { ASSERT(false); return NULL; }
	c = ch->m_s;
	if (!c->has_node_data()) {
		c->set_node_data(a, vnode_replicate_num);
	}
	ASSERT(strcmp(a, c->node_data()->iden) == 0);
	TRACE("addnode str: <%s:%p>\n", (const char *)c->node_data()->iden, c);
	if ((n = m_nodes.find(a))) { return n; }
	if ((i = m_nodes.insert(c->node_data(), a)) == m_nodes.end()) { return NULL; }
	n = &(*i);
	int r = add_node(*n);
	if (r < 0 && r != NBR_EALREADY) {
		del_node(*n);
		m_nodes.erase(a);
		return NULL;
	}
	return n;
}

/* for master : from connection (active) */
const CHNODE *world::add_node(const conn &c)
{
	const CHNODE *n;
	map<const CHNODE *, const char *>::iterator i;
	TRACE("addnode conn: <%s:%s:%p>\n", (const char *)c.node_data()->iden, 
		(const char *)c.addr(), &c);
	if ((n = m_nodes.find(c.addr()))) { return n; }
	if ((i = m_nodes.insert(c.node_data(), c.addr())) == m_nodes.end()) {
		return NULL; }
	n = &(*i);
	int r = add_node(*n);
	if (r < 0 && r != NBR_EALREADY) {
		del_node(*n);
		m_nodes.erase(c.addr());
		return NULL;
	}
	return n;
}

int world::del_node(const char *a)
{
	const CHNODE *n = m_nodes.find(a);
	CHNODE tn;
	if (!n) {
		nbr_conhash_set_node(&tn, a, vnode_replicate_num);
		n = &tn;
	}
	del_node(*n);
	m_nodes.erase(a);
	return NBR_OK;
}

int world::request(MSGID msgid, const UUID &uuid, serializer &sr)
{
#if defined(_TEST)
	if (m_test_request) {
		return m_test_request(this, msgid, uuid, sr);
	}
#endif
	connector *c = m_cf->get_by(uuid);
	if (!c) { c = (connector *)connect_assigned_node(uuid); }
	if (!c) { ASSERT(false); return NBR_EEXPIRE; }
	return c->send(msgid, sr.p(), sr.len());
}


