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
	static map<class csession*, UUID> m_sm;
public:	/* master quorum base replication */
	typedef ffutil::quorum_context quorum_context;
	int quorum_vote_commit(MSGID msgid, quorum_context *ctx, serializer &sr);
	int quorum_global_commit(world *w, quorum_context *ctx, int result);
	quorum_context *init_context(world *w);
	static int quorum_vote_callback(rpc::response &r, SWKFROM *from, void *ctx);
public:
	static int init_global(int max_conn) {
		if (!m_sm.init(max_conn, max_conn, -1, opt_expandable | opt_threadsafe)) {
			return NBR_EMALLOC;
		}
		return NBR_OK;
	}
	static void fin_global() { m_sm.fin(); };
public:
	int respond(bool err, serializer &sr) {
		return basic_fiber::respond<svnt::fiber>(err, sr);
	}
	int call_login(rpc::request &req);
	int resume_login(rpc::response &res);
	int call_replicate(rpc::request &req) { ASSERT(false); return NBR_ENOTSUPPORT; }
	int resume_replicate(rpc::response &res) { ASSERT(false); return NBR_ENOTSUPPORT; }
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
	int node_ctrl_regist(class world *, rpc::node_ctrl_cmd::regist &, serializer &);
	int node_ctrl_regist_resume(class world *, rpc::response &, serializer &);
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
		return app().ff().recv((class conn *)this, p, l, true);
	}
	int on_event(char *p, int l) {
		return app().ff().recv((class conn *)this, p, l, true);
	}
};

class csession : public sfc::base::session, public binprotocol {
public:
	typedef sfc::base::session super;
	int on_recv(char *p, int l) {
		return pfm::svnt::session::app().ff().recv(
			(class conn *)this, p, l, false);
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
};
}

}

#endif
