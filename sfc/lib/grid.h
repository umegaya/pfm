/***************************************************************
 * grid.h : simple one master - servant - client cluster
 * 2010/02/27 iyatomi : create
 *                             Copyright (C) 2008-2009 Takehiro Iyatomi
 * This file is part of libnbr.
 * libnbr is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either
 * version 2.1 of the License or any later version.
 * libnbr is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU Lesser General Public License for more details.
 * You should have received a copy of
 * the GNU Lesser General Public License along with libnbr;
 * if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 ****************************************************************/
#if !defined(__GRID_H__)
#define __GRID_H__

#include "sfc.hpp"

namespace sfc {
using namespace base;
namespace grid {
template <class S, class K, template <class C> class CP>
	class connector_factory_impl;
template <class S, typename K, template <class C> class CP>
	class grid_servant_factory_impl;

template <class S, class K, template <class C> class CP>
class connector_impl {
public:
	struct failover_chain;
	class connector_session : public S {
		struct failover_chain *m_chain;
	public:
		typedef connector_factory_impl<S,K,CP> factory;
		connector_session() : S(), m_chain(NULL) {}
		struct failover_chain *chain() { return m_chain; }
		void insert(struct failover_chain *c);
	};
	struct failover_chain {
		struct failover_chain *m_vnext, *m_vprev;
		struct failover_chain *m_prev, *m_next;
		connector_session *m_s;
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
	struct connector : public failover_chain, public CP<connector> {
		typedef CP<connector> protocol;
		struct querydata : public S::querydata {
			struct querydata *m_next_q, *m_prev_q;
			char *m_p;
			size_t m_l;
			struct connector *m_c;
			U32 m_sent_msgid;
			U8 m_is_query, padd[3];
		};
		struct querylist {
			struct querydata *m_top, *m_last;
		} m_sent, m_fail;
		struct connector *m_next_ct, *m_prev_ct;
		grid_servant_factory_impl<S,K,CP> *m_gsf;
		static RWLOCK m_lock;
	public:
		connector() : super(), protocol(this), m_next_ct(NULL), m_prev_ct(NULL) {
			memset(&m_sent, 0, sizeof(m_sent));
			memset(&m_fail, 0, sizeof(m_fail));
		}
		connector(connector *c);
		~connector();
	public:
		typedef failover_chain super;
		const grid_servant_factory_impl<S,K,CP> *gsf() const { return m_gsf; }
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
		/* TODO: all insert and unlink need to be thread safe */
		void insert(querylist &list, querydata *q) {
			lock lk(connector::m_lock, true);
			q->m_c = this;
			q->m_next_q = NULL;
			q->m_prev_q = list.m_last;
			list.m_last = q;
			if (!list.m_top) { list.m_top = q; }
		}
		void unlink(querylist &list, querydata *q) {
			lock lk(connector::m_lock, true);
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
		factory *f() {
			failover_chain *c = chain(); 
			return c ? c->m_s->f() : NULL; 
		}
		connector_factory_impl<S,K,CP> *cf() {
			return (connector_factory_impl<S,K,CP> *) f();
		}
		querydata *sendlow(U32 msgid, char*p, size_t l,
				int *r, querydata *fq);
		int try_resend();
		void remove_processed_packet(U32 last_processed);
		int send(char *p, size_t l) {
			int r;
			sendlow(0, p, l, &r, NULL);
			return r;
		}
		querydata *senddata(S &s, U32 msgid, char *p, size_t l) {
			int r;
			return senddatalow(s, msgid, p, l, &r, NULL);
		}
		querydata *senddatalow(S &, U32 msgid, char *p, size_t l,
				int *r, querydata *fq);
		bool has_session(connector_session *s) {
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
	};

};

template <class S, class K, template <class C> class CP>
RWLOCK connector_impl<S,K,CP>::connector::m_lock = NULL;

template <class S, class K, template <class C> class CP>
connector_impl<S,K,CP>::connector::connector(connector *c) : 
super(), protocol(c)
{
	memset(&m_sent, 0, sizeof(m_sent));
	memset(&m_fail, 0, siaeof(m_fail));
}

template <class S, class K, template <class C> class CP>
void connector_impl<S,K,CP>::connector_session::insert(
	connector_impl<S,K,CP>::failover_chain *c)
{
	c->m_vprev = NULL;
	c->m_vnext = m_chain;
	m_chain = c;
}

template <class S, typename K, template <class C> class CP>
class connector_factory_impl :
public factory_impl<typename connector_impl<S,K,CP>::connector_session > {
public:
	typedef typename connector_impl<S,K,CP>::connector connector;
	typedef typename connector_impl<S,K,CP>::connector_session connector_session;
	typedef typename connector::querydata querydata;
	typedef typename connector_impl<S,K,CP>::failover_chain failover_chain;
	typedef factory_impl<connector_session> super;
	typedef connector session_type;
protected:
	grid_servant_factory_impl<S,K,CP>	*m_gsf;
	array<failover_chain>			m_failover_chain_factory;
	map<connector,K>				m_failover_chain_group;
	connector						*m_failure_connector;
	RWLOCK							m_lock;
	U32								m_last_respond_msgid;
public:
	connector_factory_impl() : m_gsf(NULL),
		m_failover_chain_factory(), m_failover_chain_group(),
		m_failure_connector(NULL), m_lock(NULL),
		m_last_respond_msgid(0) {}
	~connector_factory_impl() {}
	grid_servant_factory_impl<S,K,CP>	*gsf() { return m_gsf; }
	const grid_servant_factory_impl<S,K,CP>	*gsf() const { return m_gsf; }
	void set_gsf(grid_servant_factory_impl<S,K,CP> *gsf) { m_gsf = gsf; }
	int init(const config &cfg) {
		if (!(connector::m_lock = nbr_rwlock_create())) {
			return NBR_EPTHREAD;
		}
		if (!(m_lock = nbr_rwlock_create())) {
			return NBR_EPTHREAD;
		}
		if (!m_failover_chain_factory.init(100000 * 2, -1, opt_expandable)) {
			return NBR_EMALLOC;
		}
		if (!m_failover_chain_group.init(100000, 100000, -1, opt_expandable)) {
			return NBR_EMALLOC;
		}
		/* TODO: better way? it is too personal */
		S::set_cf(this);
		if (&(S::cf()) != this) {
			return NBR_EINVAL;
		}
		return super::init(cfg);
	}
	void fin() {
		m_failover_chain_group.fin();
		m_failover_chain_factory.fin();
		if (connector::m_lock) {
			nbr_rwlock_destroy(connector::m_lock);
		}
		if (m_lock) {
			nbr_rwlock_destroy(m_lock);
		}
		super::fin();
	}
	void poll(UTIME ut) {
		connector *c = m_failure_connector, *pc;
		U32 last_msgid = m_last_respond_msgid;
		while ((pc = c)) {
			c = c->m_next_ct;
			pc->check_chain_validity();
			pc->remove_processed_packet(last_msgid);
		}
		super::poll(ut);
	}
public:
	connector *connect(const K &k, const address &a, void *p = NULL) {
		return add_failover_chain(k, a, p);
	}
	connector *backend_connect(const address &a, void *p = NULL);
	connector *from_key(const K &k) {
		return m_failover_chain_group.find(k);
	}
	connector *backend_conn();
	connector *add_failover_chain(const K &k, const address &a, void *p) {
		connector_session *s;
		connector *ct;
		failover_chain *c;
		lock lk(m_lock, true);
		if (!(ct = m_failover_chain_group.create(k))) {
			return NULL;
		}
		ct->m_gsf = gsf();
		if (!(s = super::pool().create(a))) {
			return NULL;
		}
		if (ct->has_session(s)) { return ct; }
		if (!(c = m_failover_chain_factory.create())) {
			return NULL;
		}
		c->m_s = s;
		s->insert(c);
		((failover_chain *)ct)->insert(c);
		if (!s->valid()) {
			super::connect(s, a, p);
		}
		return ct;
	}
	void del_failover_chain(failover_chain &c) {
		c.unlink();
		m_failover_chain_factory.destroy(&c);
	}
	int del_failover_chain(address &a) {
		connector_session *ct;
		if (!(ct = super::pool().find(a))) {
			return NBR_ENOTFOUND;
		}
		return del_failover_chain(*ct);
	}
	int del_failover_chain(connector_session &s) {
		return del_failover_chain_low(s.chain());
	}
	int del_failover_chain(connector &c) {
		return del_failover_chain_low(c.chain());
	}
	int del_failover_chain_low(failover_chain *c) {
		lock lk(m_lock, true);
		failover_chain *pc;
		while((pc = c)) {
			c = c->m_next;
			del_failover_chain(*pc);
		}
		return NBR_OK;
	}
	querydata *senddata(connector &via, S &sender,
			U32 msgid, char *p, size_t l, int *r, querydata *fq) {
		querydata *q;
		if ((q = via.sendlow(msgid, p, l, r, fq))) {
			/* send success. register to sent list */
			q->s = &sender;
			q->sk = sender.sk();
			q->m_is_query = 1;
			/* if r == 0, it means, packet insert to fail list.
			so dont insert it to sent list */
			return q;
		}
		/* if failed, it should be added to failure list */
		ASSERT(*r < 0);
		return NULL;
	}
	querydata *insert_query(U32 msgid) {
		return (querydata *)super::insert_query(msgid);
	}
	querydata *find_query(U32 msgid) {
		return (querydata *)super::find_query(msgid);
	}
	void remove_query(U32 msgid) {
		remove_query_low(msgid);
		if (factory::compare_msgid(msgid, m_last_respond_msgid) > 0) {
			m_last_respond_msgid = msgid;
		}
	}
	void remove_query_low(U32 msgid) {
		querydata *q = find_query(msgid);
		if (q) {
			if (q->m_p) { free(q->m_p); }
			q->m_c->sent_unlink(q);
			super::remove_query(msgid);
		}
	}
	void insert_failure_connector(connector *c) {
		TRACE("connector %p fail\n", c);
		if (is_failure_connector(c)) { return; }
		ASSERT(c->m_next_ct == NULL && m_failure_connector->m_prev_ct == NULL);
		c->m_next_ct = m_failure_connector;
		if (c->m_next_ct) { c->m_next_ct->m_prev_ct = c; }
		c->m_prev_ct = NULL;
		m_failure_connector = c;
	}
	void unlink_failure_connector(connector *c) {
		TRACE("connector %p recover from failure\n", c);
		if (c->m_next_ct) { c->m_next_ct->m_prev_ct = c->m_prev_ct; }
		if (c->m_prev_ct) { c->m_prev_ct->m_next_ct = c->m_next_ct; }
		c->m_next_ct = NULL;
		c->m_prev_ct = NULL;
	}
	bool is_failure_connector(connector *c) {
		return c->m_next_ct || c == m_failure_connector;
	}
};

template <class S, class K, template <class C> class CP>
typename connector_factory_impl<S,K,CP>::connector *
connector_factory_impl<S,K,CP>::backend_connect(const address &a, void *p)
{
	return add_failover_chain(S::backend_key(), a, p);
}

template <class S, class K, template <class C> class CP>
typename connector_factory_impl<S,K,CP>::connector *
connector_factory_impl<S,K,CP>::backend_conn()
{
	return from_key(S::backend_key());
}

template <class S, class K, template <class C> class CP>
connector_impl<S,K,CP>::connector::~connector()
{
	cf()->del_failover_chain(*this);
}

template <class S, class K, template <class C> class CP>
typename connector_impl<S,K,CP>::connector::querydata *
connector_impl<S,K,CP>::connector::senddatalow(
		S &s, U32 msgid, char *p, size_t l, int *r, querydata *fq)
{
	return cf()->senddata(*this, s, msgid, p, l, r, fq);
}

template <class S, class K, template <class C> class CP>
int connector_impl<S,K,CP>::connector::try_resend()
{
	querydata *fq;
	int r = NBR_OK;
	if (writable() > 0 && (fq = fail_list())) {
		querydata *pfq;
		while ((pfq = fq)) {
			fq = fq->m_next_q;
			/* TODO: make it thread safe */
			fail_unlink(pfq);
			if (pfq->m_is_query) {
				senddatalow(
					*(pfq->sender()),
					pfq->m_sent_msgid, pfq->m_p, pfq->m_l, &r, pfq);
			}
			else {
				sendlow(pfq->m_sent_msgid, pfq->m_p, pfq->m_l, &r, pfq);
			}
			if (r < 0) { break; }
		}
		if (!fail_list()) {
			cf()->unlink_failure_connector(this);
		}
	}
	return r;
}

template <class S, class K, template <class C> class CP>
void connector_impl<S,K,CP>::connector::
	remove_processed_packet(U32 last_processed)
{
	querydata *q = m_sent.m_top, *pq;
	while((pq = q)) {
		q = q->m_next_q;
		if (q->m_is_query) { continue; }
		if (factory::compare_msgid(last_processed, q->m_sent_msgid) > 0) {
			cf()->remove_query_low(q->m_sent_msgid);
		}
	}
}

template <class S, class K, template <class C> class CP>
typename connector_impl<S,K,CP>::connector::querydata *
connector_impl<S,K,CP>::connector::sendlow(
		U32 msgid, char *p, size_t l, int *r, querydata *fq)
{
	failover_chain *c = chain();
	if (!fq) {
		/* if connection is recovered, send unprocessed packet */
		int r = try_resend();
		if (msgid == 0) { msgid = cf()->msgid(); }
		querydata *q = cf()->insert_query(msgid);
		if (!q) { goto error; }
		q->m_l = l;
		q->m_sent_msgid = msgid;
		q->m_is_query = 0;
		if (!(q->m_p = (char *)malloc(l))) { goto error; }
		memcpy(q->m_p, p, l);
		fq = q;
		if (r < 0) { goto senderror; }
	}
	if (!c) { goto senderror; }
	if (c->m_s->send(p, l) < 0) { goto senderror; }
#if defined(_DEBUG)
	f()->log(kernel::INFO, "connector: send %u byte[%u]\n", l, *p);
#endif
	*r = l;
	/* TODO: make it thread safe (but slow...) */
	sent_insert(fq);
	return fq;
senderror:
	/* TODO: make it thread safe (but slow...) */
	fail_insert(fq);
	cf()->insert_failure_connector(this);
	*r = 0;
	return fq;
error:
	cf()->remove_query(msgid);
	*r = NBR_EEXPIRE;
	return NULL;
}

template <class S, typename K/* failover_chain group key */,
	template <class C> class CP>
class grid_servant_factory_impl : public factory_impl<S> {
public:
		typedef factory_impl<S> super;
		typedef typename connector_factory_impl<S,K,CP>::connector connector;
protected:
		connector_factory_impl<S,K,CP>	m_connector_factory;
public:
		grid_servant_factory_impl() : super(), m_connector_factory() {}
		~grid_servant_factory_impl() {}
		connector *from_key(const K &k) {
			return m_connector_factory.from_key(k); }
		int init(const config &cfg) {
			if (super::init(cfg) < 0) {
				return NBR_EINVAL;
			}
			m_connector_factory.set_gsf(this);
			/* use same configuration, but client context */
			config *dcfg = cfg.dup();
			if (!dcfg) {
				return NBR_EMALLOC;
			}
			dcfg->m_flag &= (~(config::cfg_flag_server));
			dcfg->m_query_bufsz = sizeof(
					typename connector_impl<S,K,CP>::connector::querydata);
			return m_connector_factory.init(*dcfg);
		}
		void fin() {
			m_connector_factory.fin();
			super::fin();
		}
		void poll(UTIME ut) {
			m_connector_factory.poll(ut);
			super::poll(ut);
		}
		connector *connect(const K &k, const address &a, void *p = NULL) {
			return m_connector_factory.connect(k, a, p);
		}
		connector *backend_connect(const address &a, void *p = NULL) {
			return m_connector_factory.backend_connect(a,p);
		}
		connector *backend_conn() {
			return m_connector_factory.backend_conn();
		}

};

#include "grid.inc"
}	//namespace grid
}	//namespace sfc

#endif	//__GRID_H__

