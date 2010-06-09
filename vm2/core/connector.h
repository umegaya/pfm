#if !defined(__CONNECTOR_H__)
#define __CONNECTOR_H__

#include "common.h"
#include "proto.h"
#include "uuid.h"

namespace pfm {
using namespace sfc;
using namespace rpc;
using namespace sfc::util;
using namespace sfc::base;

typedef session conn_impl;
class conn : public conn_impl, public binprotocol {
#if defined(_TEST)
public:
	static int (*m_test_send)(class conn *, char *, int);
	void set_addr(const address &a) { conn_impl::setaddr(a); }
#endif
public:
	int writable() const { return conn_impl::writable(); }
	int send(char *p, int l) {
#if defined(_TEST)
		if (m_test_send) { return m_test_send(this, p, l); }
#endif
		return conn_impl::send(p, l);
	}
	bool valid() const { return conn_impl::valid(); }
	address &get_addr(address &a) { return conn_impl::addr(); }
};

typedef factory_impl<conn, mappool<conn> > conn_pool_impl;
class conn_pool : public conn_pool_impl {
public:
#if defined(_TEST)
	static int (*m_test_connect)(class conn_pool *, 
		conn *, const address &, void *);
#endif
	int connect(conn *c, const address &a, void *p = NULL) {
#if defined(_TEST)
	if (m_test_connect) { return m_test_connect(this, c, a, p); }
#endif
		return conn_pool_impl::connect(c, a, p); }
	conn *create(const address &a) { return conn_pool_impl::pool().create(a); }
	conn *find(const address &a) { return conn_pool_impl::pool().find(a); }
};

class connector_impl {
public:
	struct failover_chain {
		struct failover_chain *m_vnext, *m_vprev;
		struct failover_chain *m_prev, *m_next;
		conn *m_s;
	public:
		failover_chain() : m_vnext(NULL), m_vprev(NULL),
			m_prev(NULL), m_next(NULL), m_s(NULL) {}
		void insert(struct failover_chain *c) {
			if (m_next) { m_next->m_prev = c; }
			c->m_prev = this;
			c->m_next = m_next;
			m_next = c;
		}
		void unlink() {
			if (m_next) { m_next->m_prev = m_prev; }
			if (m_prev) { m_prev->m_next = m_next; }
			if (m_vnext) { m_vnext->m_vprev = m_vprev; }
			if (m_vprev) { m_vprev->m_vnext = m_vnext; }
		}
	};
	struct connector;
	struct querydata {
		enum {
			flag_multicast = 0x01,
		};
		struct querydata *m_next_q, *m_prev_q;
		char *m_p;
		size_t m_l;
		struct connector *m_c;
		MSGID m_sent_msgid;
		U8 m_flag, padd[3];
	};
	struct connector_resource;
	struct connector : protected failover_chain {
		struct querylist {
			struct querydata *m_top, *m_last;
		} m_sent, m_fail;
		struct connector *m_next_ct, *m_prev_ct;
		struct connector_resource *m_cr;
		RWLOCK m_lock;
	public:
		connector() : super(), m_next_ct(NULL), m_prev_ct(NULL) {
			m_sent.m_top = m_sent.m_last = NULL;
			m_fail.m_top = m_fail.m_last = NULL;
			if (!m_lock) { assert((m_lock = nbr_rwlock_create())); }
		}
		~connector() {}
		void set(connector_resource *cr) { m_cr = cr; }
		connector_resource &cf() { return *m_cr; }
	public:
		typedef failover_chain super;
		failover_chain *chain() { return super::m_next; }
		const failover_chain *chain() const { return super::m_next; }
		querydata *sent_list() { return m_sent.m_top; }
		const querydata *sent_list() const { return m_sent.m_top; }
		querydata *fail_list() { return m_fail.m_top; }
		const querydata *fail_list() const { return m_fail.m_top; }
		bool check_chain_validity() {
			try_resend();
			return true;
		}
		void insert(querylist &list, querydata *q) {
			lock lk(m_lock, false);
			q->m_c = this;
			q->m_next_q = NULL;
			q->m_prev_q = list.m_last;
			list.m_last = q;
			if (!list.m_top) { list.m_top = q; }
		}
		void unlink(querylist &list, querydata *q) {
			lock lk(connector::m_lock, false);
			if (list.m_top == q) {
				list.m_top = list.m_top->m_next_q;
			}
			if (list.m_last == q) {
				list.m_last = list.m_last->m_prev_q;
			}
			if (q->m_next_q) { q->m_next_q->m_prev_q = q->m_prev_q; }
			if (q->m_prev_q) { q->m_prev_q->m_next_q = q->m_next_q; }
			q->m_c = NULL;
		}
		void sent_insert(querydata *q) { insert(m_sent, q); }
		void sent_unlink(querydata *q) { unlink(m_sent, q); }
		void fail_insert(querydata *q) { insert(m_fail, q); }
		void fail_unlink(querydata *q) { unlink(m_fail, q); }
		inline int send(MSGID msgid, char*p, size_t l, querydata *fq = NULL);
		inline int replicate(MSGID msgid, char *p, size_t l);
		inline int try_resend();
		inline void remove_processed_packet(MSGID last_processed);
		bool has_session(conn *s) {
			failover_chain *c = chain();
			while (c) {
				if (c->m_s == s) { return true; }
				c = c->m_next;
			}
			return false;
		}
		int writable() const {
			const failover_chain *c = chain();
			if (!c) { return NBR_ESEND; }
			return c->m_s->writable();
		}
		conn *primary() { return chain() ? chain()->m_s : NULL; }
	};
	struct connector_resource {
		struct connector *m_failure_connector;
		MSGID			m_last_respond_msgid;
		RWLOCK			m_lock;
		/* FIXME: use memory block allocation for querydata assignment */
		//char *m_querybuf;
		map<querydata, MSGID> m_querylist;
	public:
		connector_resource() {}
		int init(int querybuf) {
			m_failure_connector = NULL;
			m_last_respond_msgid = 0;
			if (!(m_lock = nbr_rwlock_create())) { fin(); }
			return m_querylist.init(querybuf, querybuf, -1, opt_expandable);
			//return (m_querybuf = malloc(querybuf)) ? NBR_OK : NBR_EMALLOC;
		}
		void fin() { 
			if (m_lock) {
				nbr_rwlock_destroy(m_lock); 
				m_lock = NULL; 
			}		
			m_querylist.fin(); 
		}
		querydata *insert_query(MSGID msgid) {
			return m_querylist.create(msgid);
		}
		querydata *find_query(MSGID msgid) {
			return m_querylist.find(msgid);
		}
		/* TODO : call this when every packet receiving */
		void remove_query(MSGID msgid) {
			remove_query_low(msgid);
			if (msgid_generator::compare_msgid(msgid, m_last_respond_msgid) > 0) {
				m_last_respond_msgid = msgid;
			}
		}
		void remove_query_low(MSGID msgid) {
			querydata *q = find_query(msgid);
			if (q) {
				if (q->m_p) {
					nbr_free(q->m_p);
					q->m_p = NULL;
				}
				if (q->m_c) {
					q->m_c->sent_unlink(q);
					q->m_c = NULL;
				}
				remove_query(msgid);
			}
		}
		void insert_failure_connector(connector *c) {
			TRACE("connector %p fail\n", c);
			if (is_failure_connector(c)) { return; }
			lock lk(m_lock, false);
			ASSERT(c->m_next_ct == NULL && m_failure_connector->m_prev_ct == NULL);
			c->m_next_ct = m_failure_connector;
			if (c->m_next_ct) { c->m_next_ct->m_prev_ct = c; }
			c->m_prev_ct = NULL;
			m_failure_connector = c;
		}
		void unlink_failure_connector(connector *c) {
			TRACE("connector %p recover from failure\n", c);
			lock lk(m_lock, false);
			if (c->m_next_ct) { c->m_next_ct->m_prev_ct = c->m_prev_ct; }
			if (c->m_prev_ct) { c->m_prev_ct->m_next_ct = c->m_next_ct; }
			c->m_next_ct = NULL;
			c->m_prev_ct = NULL;
		}
		bool is_failure_connector(connector *c) {
			return c->m_next_ct || c == m_failure_connector;
		}
	};
};

class connector_factory : public map<connector_impl::connector, UUID>
{
public:
	typedef map<connector_impl::connector, UUID> super;
	typedef connector_impl::connector connector;
	typedef connector_impl::connector_resource connector_resource;
	typedef connector_impl::querydata querydata;
	typedef connector_impl::failover_chain failover_chain;
	connector_factory() : super(), m_pool(NULL) {}
	~connector_factory() { fin(); }
protected:
	conn_pool				*m_pool;
	array<failover_chain>	m_failover_chain_factory;
	map<failover_chain*, address>	m_address_chain_map;
	connector_resource		m_connector_resource;
	RWLOCK					m_lock;
public:
	int init(conn_pool *cp, int max_chain, int max_node, int querysize) {
		int r;
		m_pool = cp;
		if (!(m_lock = nbr_rwlock_create())) {
			fin();
			return NBR_EPTHREAD;
		}
		if (!m_failover_chain_factory.init(max_chain * 2, -1, opt_expandable)) {
			fin();
			return NBR_EMALLOC;
		}
		if ((r = m_connector_resource.init(querysize)) < 0) {
			fin();
			return r;
		}
		if (!m_address_chain_map.init(max_node, max_node / 3, -1, opt_expandable)) {
			fin();
			return NBR_EMALLOC;
		}
		if (!super::init(max_chain, max_chain / 3, -1, opt_expandable)) {
			fin();
			return NBR_EMALLOC;
		}
		return NBR_OK;
	}
	void fin() {
		m_failover_chain_factory.fin();
		m_address_chain_map.fin();
		if (m_lock) {
			nbr_rwlock_destroy(m_lock);
			m_lock = NULL;
		}
		m_connector_resource.fin();
		super::fin();
	}
	void poll(UTIME ut) {
		connector *c = m_connector_resource.m_failure_connector, *pc;
		MSGID last_msgid = m_connector_resource.m_last_respond_msgid;
		while ((pc = c)) {
			c = c->m_next_ct;
			pc->check_chain_validity();
			pc->remove_processed_packet(last_msgid);
		}
	}
	void set_pool(conn_pool *f) { m_pool = f; }
	conn_pool &pool() { return *m_pool; }
public:
	conn *get_by(const address &a) { return pool().find(a); }
	connector *connect(const UUID &k, const address &a, void *p = NULL) {
		return add_failover_chain(k, a, p);
	}
	connector *backend_connect(const address &a, void *p = NULL) {
		return add_failover_chain(UUID::invalid_id(), a, p);
	}
	connector *get_by(const UUID &k) {
		return super::find(k);
	}
	connector *backend_conn() {
		return super::find(UUID::invalid_id());
	}
	connector *add_failover_chain(const UUID &k, const address &a, void *p) {
		conn *s;
		connector *ct;
		failover_chain *c;
		lock lk(m_lock, false);
		if (!(ct = super::create(k))) {
			return NULL;
		}
		ct->set(&m_connector_resource);
		if (!(s = pool().create(a))) {
			return NULL;
		}
		if (ct->has_session(s)) { return ct; }
		if (!(c = m_failover_chain_factory.create())) {
			return NULL;
		}
		c->m_s = s;
		m_address_chain_map.insert(c, a);
		((failover_chain *)ct)->insert(c);
		if (!s->valid()) {
			pool().connect(s, a, p);
		}
		return ct;
	}
	void del_failover_chain_low(failover_chain &c) {
		c.unlink();
		m_failover_chain_factory.destroy(&c);
	}
	int del_failover_chain(address &a) {
		failover_chain *c;
		if (!(c = m_address_chain_map.find(a))) {
			return NBR_ENOTFOUND;
		}
		return del_failover_chain(c);
	}
	int del_failover_chain(connector &c) {
		return del_failover_chain(c.chain());
	}
	int del_failover_chain(failover_chain *c) {
		lock lk(m_lock, false);
		failover_chain *pc;
		while((pc = c)) {
			c = c->m_next;
			del_failover_chain_low(*pc);
		}
		return NBR_OK;
	}
};

inline
int connector_impl::connector::try_resend()
{
	querydata *fq;
	int r = NBR_OK;
	if (writable() > 0 && (fq = fail_list())) {
		querydata *pfq;
		while ((pfq = fq)) {
			fq = fq->m_next_q;
			fail_unlink(pfq);
			if ((r = send(pfq->m_sent_msgid, pfq->m_p, pfq->m_l, pfq)) < 0) {
				break;
			}
		}
		if (!fail_list()) {
			cf().unlink_failure_connector(this);
		}
	}
	return r;
}

inline
void connector_impl::connector::
	remove_processed_packet(MSGID last_processed)
{
	querydata *q = m_sent.m_top, *pq;
	while((pq = q)) {
		q = q->m_next_q;
		if (msgid_generator::compare_msgid(last_processed, q->m_sent_msgid) > 0) {
			cf().remove_query_low(q->m_sent_msgid);
		}
	}
}

inline
int connector_impl::connector::send(MSGID msgid, char *p, size_t l, querydata *fq)
{
	failover_chain *c = chain();
	if (!fq) {
		/* if connection is recovered, send unprocessed packet */
		int r = try_resend();
		ASSERT(cf().find_query(msgid) == NULL);
		querydata *q = cf().insert_query(msgid);
		if (!q) { goto error; }
		q->m_l = l;
		q->m_sent_msgid = msgid;
		if (!(q->m_p = (char *)malloc(l))) { goto error; }
		memcpy(q->m_p, p, l);
		fq = q;
		if (r < 0) { goto senderror; }
	}
	if (!c) { goto senderror; }
	if (c->m_s->send(p, l) < 0) { goto senderror; }
#if defined(_DEBUG)
	LOG("connector: send %u byte[%u]\n", l, *p);
#endif
	sent_insert(fq);
	return l;
senderror:
	fail_insert(fq);
	cf().insert_failure_connector(this);
	return NBR_OK;
error:
	cf().remove_query(msgid);
	return NBR_EEXPIRE;
}

inline
int connector_impl::connector::replicate(MSGID msgid, char *p, size_t l)
{
#if 0
	failover_chain *c = chain();
	if (!fq) {
		/* if connection is recovered, send unprocessed packet */
		int r = try_resend();
		querydata *q = cf()->insert_query(msgid);
		if (!q) { goto error; }
		q->m_l = l;
		q->m_sent_msgid = msgid;
		if (!(q->m_p = malloc(l))) { goto error; }
		memcpy(q->m_p, p, l);
		fq = q;
		if (r < 0) { goto senderror; }
	}
	if (!c) { goto senderror; }
	if (c->m_s->send(p, l) < 0) { goto senderror; }
#if defined(_DEBUG)
	f()->log(kernel::INFO, "connector: send %u byte[%u]\n", l, *p);
#endif
	/* TODO: make it thread safe (but slow...) */
	sent_insert(fq);
	return l;
senderror:
	/* TODO: make it thread safe (but slow...) */
	fail_insert(fq);
	cf()->insert_failure_connector(this);
	return NBR_OK;
error:
	cf()->remove_query(msgid);
	return NBR_EEXPIRE;
#endif
	return NBR_OK;
}

typedef connector_factory::connector connector;

}//namespace pfm

#endif
