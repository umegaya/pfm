#if !defined(__CP_H__)
#define __CP_H__

#include "mstr/mstr.h"
#include "svnt/svnt.h"
#include "clnt/clnt.h"

namespace pfm {
class conn_pool {
public:
	enum type {
		invalid,
		master,
		servant,
		client,
		client_robot,
	};
	type m_type;
	typedef factory_impl<mstr::msession, mappool<mstr::msession> > mstr_cp;
	typedef factory_impl<svnt::session, mappool<svnt::session> > svnt_cp;
	typedef factory_impl<clnt::session, mappool<clnt::session> > clnt_cp;
	typedef factory_impl<clnt::session, arraypool<clnt::session> > rclnt_cp;
protected:
	union {
		mstr_cp *m_mstr;
		svnt_cp *m_svnt;
		clnt_cp *m_clnt;
		rclnt_cp *m_rclnt;
	};
	type cp_type(mstr_cp *cp) { m_mstr = cp; return master; }
	type cp_type(svnt_cp *cp) { m_svnt = cp; return servant; }
	type cp_type(clnt_cp *cp) { m_clnt = cp; return client; }
	type cp_type(rclnt_cp *cp) { m_rclnt = cp; return client_robot; }
public:
	conn_pool() : m_type(invalid) {}
	template <class CP> conn_pool(CP *cp) { m_type = cp_type(cp); }
	template <class CP> void set_pool(CP *cp) { m_type = cp_type(cp); }
#if defined(_TEST)
	static int (*m_test_connect)(class conn_pool *,
		conn *, const address &, void *);
#endif
	int connect(conn *c, const address &a, void *p = NULL) {
#if defined(_TEST)
	if (m_test_connect) { return m_test_connect(this, c, a, p); }
#endif
		switch(m_type) {
		case master: return m_mstr->connect(c, a, p);
		case servant: return m_svnt->connect(c, a, p);
		case client: return m_clnt->connect(c, a, p);
		case client_robot: return m_rclnt->connect(c, a, p);
		default: ASSERT(false); return NBR_EINVAL;
		}
	}
	conn *create(const address &a) {
		switch(m_type) {
		case master: return m_mstr->create(a);
		case servant: return m_svnt->create(a);
		case client: return m_clnt->create(a);
		case client_robot: return m_rclnt->create(a);
		default: ASSERT(false); return NULL;
		}
	}
	conn *find(const address &a) {
		switch(m_type) {
		case master: return m_mstr->pool().find(a);
		case servant: return m_svnt->pool().find(a);
		case client: return m_clnt->pool().find(a);
//		case client_robot: return NULL;
		default: ASSERT(false); return NULL;
		}
	}
	const address &bind_addr(const address &a) {
		switch(m_type) {
		case master: return m_mstr->ifaddr();
		case servant: return m_svnt->ifaddr();
		case client: return m_clnt->ifaddr();
		case client_robot: return m_rclnt->ifaddr();
		default: ASSERT(false); return a;
		}
	}
};

inline conn *connector_factory::create(const address &a)
{
	return pool().create(a);
}

inline conn *connector_factory::find(const address &a)
{
	return pool().find(a);
}

inline int connector_factory::connect(conn *c, const address &a, void *p)
{
	return pool().connect(c, a, p);
}
}

#endif
