#include "common.h"
#include "proto.h"
#include "world.h"
#include "connector.h"
#include "cp.h"

using namespace pfm;

#if defined(_TEST)
int (*world::m_test_request)(world *, MSGID,
		const UUID &, address &, serializer &) = NULL;
#endif


/* rehasher */
int rehasher::start()
{
	if (!(m_sr = new serializer)) {
		ASSERT(false);
		return NBR_EMALLOC;
	}
	TRACE("rehasher(%p/%s) : start\n", this, m_wld->id());
	int r = m_ff->of().iterate(*this);
	serializer &sr = *m_sr;
	PREPARE_PACK(sr);
	rpc::response::pack_header(sr, m_msgid);
	if (r > 0) {
		sr << r;
		sr.pushnil();
	}
	else {
		sr.pushnil();
		sr << r;
	}
	if ((r = m_ff->run_fiber(fiber::to_thrd(curr()),
		m_invoked, sr.p(), sr.len())) < 0) {
		ASSERT(false);
	}
	delete m_sr;
	TRACE("rehasher(%p/%s) : end\n", this, m_wld->id());
	return r;
}
int rehasher::operator () (dbm_impl *db, const char *k, int ksz)
{
	int r;
	ASSERT(ksz == sizeof(UUID));
	UUID &uuid = *((UUID *)k);
	char buf[256];
	TRACE("op() : for %s\n", uuid.to_s(buf, sizeof(buf)));
	if (!m_wld->primary_node_for(uuid)) {
		static const U32 save_work_buffer_size = 16 * 1024;
		char buffer[save_work_buffer_size + 1024], *b = buffer;
		serializer &sr = *m_sr;
		sr.pack_start(buffer, sizeof(buffer));
		MSGID msgid = m_ff->new_msgid();
		TRACE("rehash : do %s:%u\n", uuid.to_s(buf, sizeof(buf)), msgid);
		b += rpc::replicate_request::pack_header(
			sr, rpc::start_replicate, msgid,
			m_wld->id(), nbr_str_length(m_wld->id(), max_wid),
			uuid, replicate_move_to);
		ASSERT((size_t)(b - buffer) == sr.len());
		if ((r = db->select(k, ksz, b, sizeof(buffer) - (b - buffer))) < 0) {
			ASSERT(false);
			return NBR_ENOTFOUND;
		}
//		TRACE("record for %s : %u byte\n", uuid.to_s(buf, sizeof(buf)), r);
		sr.skip(r);
		rpc::replicate_request req_;
		ASSERT(sr.unpack(req_, sr.p(), sr.len()) > 0);
		object *o = m_ff->of().find(uuid);
		if (o) {
			if ((r = m_ff->run_fiber(fiber::to_thrd(curr()),
				o->vm()->attached_thrd(), sr.p(), sr.len())) < 0) {
				ASSERT(false); return r;
			}
		}
		else if ((r = m_ff->run_fiber(fiber::to_thrd(curr()),
			sr.p(), sr.len())) < 0) {
			ASSERT(false);
			return r;
		}
		if ((r = yield(10)) < 0) {
			TRACE("rehasher : timeout\n");
			return r;
		}
		TRACE("rehasher : do next\n");
	}
	return NBR_OK;
}

void *rehasher::proc(THREAD p)
{
	rehasher *rh = (rehasher *)nbr_thread_get_data(p);
	rh->set_thrd(p);
	rh->start();
	return NULL;/* stop job */
}



/* world */
int world::init(int max_node, int max_replica)
{
	if (!(m_ch = nbr_conhash_init(NULL, max_node, max_replica))) {
		return NBR_EMALLOC;
	}
	if (!(m_nch = nbr_conhash_init(NULL, max_node, max_replica))) {
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


bool world::primary_node_for(const UUID &uuid)
{
	const CHNODE *n[object_multiplexity];
	if (lookup_node(uuid, n, object_multiplexity) < 0) {
		return false;
	}
	TRACE("node_addr: %s, n[0] = %s\n",
		(const char *)m_cf->node_addr(), n[0]->iden);
	return m_cf->node_addr() == n[0]->iden;
}

bool world::node_for(const UUID &uuid)
{
	int n_node;
	const CHNODE *n[object_multiplexity];
	if ((n_node = lookup_node(uuid, n, object_multiplexity)) < 0) {
		return false;
	}
	for (int i = 0; i < n_node; i++) {
		if (m_cf->node_addr() == n[0]->iden) { return true; }
	}
	return false;
}

int world::request(MSGID msgid, const UUID &uuid, serializer &sr, bool recon)
{
#if defined(_TEST)
	if (m_test_request) {
		int n_node;
		address a;
		const CHNODE *n[object_multiplexity];
		if ((n_node = lookup_node(uuid, n, object_multiplexity)) <= 0) {
			a.from("www.deadbeef.com:8000");
		}
		else {
			a.from(n[0]->iden);
			ASSERT(connect_assigned_node(uuid));
			char b[256];
			TRACE("connector: add %s for %s\n",
				(const char *)a, uuid.to_s(b, sizeof(b)));
		}
		return m_test_request(this, msgid, uuid, a, sr);
	}
#endif
	connector *c = m_cf->get_by(uuid);
	if (recon && c) {
		m_cf->del_failover_chain(*c);
		c = NULL;
	}
	if (!c) { c = (connector *)connect_assigned_node(uuid); }
	if (!c) { ASSERT(false); return NBR_EEXPIRE; }
	return c->send(msgid, sr.p(), sr.len());
}

int world::re_request(MSGID msgid, const UUID &uuid, bool reconnect)
{
	int r;
	connector_factory::querydata *q = NULL;
	if (msgid != INVALID_MSGID) {
		if (!(q = m_cf->find_query(msgid))) {
			ASSERT(false);
			return NBR_ENOTFOUND;
		}
	}
	/* re-create connector */
	connector *c = m_cf->get_by(uuid);
	if (reconnect && c) {
		m_cf->del_failover_chain(*c);
		c = NULL;
	}
	if (!c) { c = (connector *)connect_assigned_node(uuid); }
	if (!c) { ASSERT(false); return NBR_EEXPIRE; }
	if (q) {
		if ((r = c->send(msgid, q->m_p, q->m_l)) > 0) {
			m_cf->remove_query_low(msgid);
		}
		return r;
	}
	else {
		return c->try_resend(c->sent_list());
	}
}


