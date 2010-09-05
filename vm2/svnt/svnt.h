#if !defined(__SVNT_H__)
#define __SVNT_H__

#include "common.h"
#include "fiber.h"
#include "dbm.h"
#include "proto.h"
#include "connector.h"
#include "world.h"

namespace pfm {
using namespace sfc;
namespace svnt {
class fiber : public rpc::basic_fiber {
public:
	typedef rpc::basic_fiber super;
	struct csession_entry {
		class csession *m_s;
	};
	static map<csession_entry, UUID> m_sm;
public:	/* master quorum base replication */
	typedef super::quorum_context quorum_context;
	int quorum_vote_commit(MSGID msgid, quorum_context *ctx, serializer &sr);
	int quorum_global_commit(world *w, quorum_context *ctx, int result);
	quorum_context *init_context(world *w);
	static int quorum_vote_callback(rpc::response &r, THREAD thrd, void *ctx);
public:
	static int init_global(int max_conn) {
		if (!m_sm.init(max_conn, max_conn, -1, opt_expandable | opt_threadsafe)) {
			return NBR_EMALLOC;
		}
		return NBR_OK;
	}
	static void fin_global() { m_sm.fin(); };
	static int register_session(class csession *s, const UUID &uuid) {
		csession_entry *ce = m_sm.create(uuid);
		if (!ce) { return NBR_EEXPIRE; }
		ce->m_s = s;
		return NBR_OK;
	}
	static void unregister_session(const UUID &uuid) { m_sm.erase(uuid); }
	static class csession *find_session(const UUID &uuid) {
		csession_entry *ce = m_sm.find(uuid);
		return ce ? ce->m_s : NULL;
	}
public:
	int respond(bool err, serializer &sr) {
		return basic_fiber::respond<svnt::fiber>(err, sr);
	}
	int send_error(int r) { return basic_fiber::send_error<svnt::fiber>(r); }
	int call_ll_exec_client(rpc::request &req, object *o,
			bool trusted, char *p, int l);
	int call_login(rpc::request &req);
	int resume_login(rpc::response &res);
	int call_logout(rpc::request &req);
	int resume_logout(rpc::response &res);
	int call_replicate(rpc::request &req, char *p, int l);
	int resume_replicate(rpc::response &res);
	int call_node_inquiry(rpc::request &req);
	int resume_node_inquiry(rpc::response &res);
public:
	int node_ctrl_add(class world *, rpc::node_ctrl_cmd::add &, serializer &);
	int node_ctrl_add_resume(class world *, rpc::response &, serializer &);
	int node_ctrl_del(class world *, rpc::node_ctrl_cmd::del &, serializer &);
	int node_ctrl_del_resume(class world *, rpc::response &, serializer &);
	int node_ctrl_list(class world *, rpc::node_ctrl_cmd::list &, serializer &);
	int node_ctrl_list_resume(class world *, rpc::response &, serializer &);
	int node_ctrl_deploy(class world *, rpc::node_ctrl_cmd::deploy &, serializer &);
	int node_ctrl_deploy_resume(class world *, rpc::response &, serializer &);
	int node_ctrl_vm_init(class world *,
			rpc::node_ctrl_cmd::vm_init &, serializer &);
	int node_ctrl_vm_init_resume(class world *, rpc::response &, serializer &);
	int node_ctrl_vm_fin(class world *,
		rpc::node_ctrl_cmd::vm_fin &, serializer &);
	int node_ctrl_vm_fin_resume(class world *, rpc::response &, serializer &);
	int node_ctrl_vm_deploy(class world *,
		rpc::node_ctrl_cmd::vm_deploy &, serializer &);
	int node_ctrl_vm_deploy_resume(class world *, rpc::response &, serializer &);
	int call_node_regist(rpc::request &);
	int resume_node_regist(rpc::response &);
};
}

class pfms : public app::daemon {
protected:
	fiber_factory<svnt::fiber> &m_ff;
	class connector_factory &m_cf;
	dbm m_db;
	serializer m_sr;
public:
	pfms(fiber_factory<svnt::fiber> &ff,
		class connector_factory &cf) :
		m_ff(ff), m_cf(cf), m_db(), m_sr() {}
	fiber_factory<svnt::fiber> &ff() { return m_ff; }
	serializer &sr() { return m_sr; }
	base::factory *create_factory(const char *sname);
	int	create_config(config* cl[], int size);
	int	boot(int argc, char *argv[]);
	void shutdown();
	int	initlib(CONFIG &c) { return NBR_OK; }
};


namespace svnt {
/* session */
class session : public conn {
public:
	static class pfms *m_daemon;
	static bool m_test_mode;
public:
	typedef conn super;
	session() : super() {}
	~session() {}
	static class pfms &app() { return *m_daemon; }
public:
	pollret poll(UTIME ut, bool from_worker) {
		/* check timeout */
		if (from_worker) {
			app().ff().poll(time(NULL));
		}
		return super::poll(ut, from_worker);
	}
	void fin()						{}
	int on_open(const config &cfg) { return super::on_open(cfg); }
	int on_close(int reason) { return super::on_close(reason); }
	int on_recv(char *p, int l) {
		app().ff().recv((class conn *)this, p, l, true);
		return NBR_OK;
	}
	int on_event(char *p, int l) {
		return app().ff().recv((class conn *)this, p, l, true);
	}
};

class csession : public sfc::base::session, public binprotocol {
protected:
	static const size_t max_authdata = 1024;
	char m_authdata[max_authdata];
	char m_wid[max_wid];
	U16 m_alen, m_wlen;
	UUID m_uuid;
	char m_acc[rpc::login_request::max_account];
public:
	typedef sfc::base::session super;
	csession() : super(), m_alen(0), m_wlen(0), m_uuid() {}
	~csession() {}
	int set_account_info(world_id wid, 
		const char *acc, const char *auth, int alen) {
		nbr_str_copy(m_acc, sizeof(m_acc), acc, sizeof(m_acc));
		if ((size_t)alen > max_authdata) { return NBR_ESHORT; }
		memcpy(m_authdata, auth, alen);
		m_alen = alen;
		m_wlen = nbr_str_copy(m_wid, max_wid, wid, max_wid);
		if (m_wlen > max_wid) { return NBR_ESHORT; }
		return NBR_OK;
	}
	const char *authdata() const { return m_authdata; }
	int alen() const { return m_alen; }
	const UUID &player_id() const { return m_uuid; }
	const char *account() const { return m_acc; }
	void set_uuid(const UUID &uuid) { m_uuid = uuid; }
	inline int on_recv(char *p, int l);
	static int logout_cb(serializer &sr) {
		LOG("logout\n");
		/* TODO : remove object from factory and vm */
		return NBR_OK;
	}
	int on_close(int reason) {
		int r;
		char b[256];
		LOG("session unregister <%s>\n", m_uuid.to_s(b, sizeof(b)));
		fiber::unregister_session(m_uuid);
		serializer &sr = svnt::session::app().sr();
		PREPARE_PACK(sr);
		if ((r = rpc::logout_request::pack_header(
			sr, svnt::session::app().ff().new_msgid(), m_wid, m_wlen, m_acc)) < 0) {
			ASSERT(false);
			return r;
		}
		if ((r = svnt::session::app().ff().run_fiber(logout_cb, sr.p(), sr.len())) < 0) {
			ASSERT(false);
			return r;
		}
		return super::on_close(reason);
	}
};

class besession : public session {
public:
	typedef session super;
	void fin() {
		TRACE("be session destroyed (%s)\n", (const char *)addr());
		app().ff().wf().cf()->del_failover_chain(addr());
	}
	static int node_regist_cb(serializer &sr) {
		TRACE("node regist finish\n");
		return NBR_OK;
	}
	bool master_session() const { return !has_node_data(); }
	int regist_node() {
		if (!app().ff().ffutil::initialized() && !app().ff().init_tls()) {
			ASSERT(false);
			return NBR_EINVAL;
		}
		int r;
		serializer &sr = app().ff().sr();
		PREPARE_PACK(sr);
		factory *f = app().find_factory<factory>("svnt");
		if (!f) { return NBR_ENOTFOUND; }
		if ((r = rpc::node_ctrl_cmd::regist::pack_header(
			sr, app().ff().new_msgid(), f->ifaddr(), f->ifaddr().len(),
			m_test_mode ? test_servant_node : servant_node)) < 0) {
			ASSERT(false);
			return r;
		}
		if ((r = app().ff().run_fiber(node_regist_cb, sr.p(), sr.len())) < 0) {
			ASSERT(false);
			return r;
		}
		return NBR_OK;
	}
	int on_open(const config &cfg) {
		if (master_session()) {
			int r = regist_node();
			if (r < 0) { return r; }
		}
		return super::on_open(cfg);
	}
	/* TODO : when close remove it from connector
	 * and timeout yielded fiber which uses this connection */
};
}
class svnt_csession : public svnt::csession {
public:
	static svnt_csession *cast(svnt::csession *s) {
		return (svnt_csession *)s; }
private:
	svnt_csession() {}
};
inline int svnt::csession::on_recv(char *p, int l) {
	return pfm::svnt::session::app().ff().recv(
		svnt_csession::cast(this), p, l, false);
}


}

#endif
