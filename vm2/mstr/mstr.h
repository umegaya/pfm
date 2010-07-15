#if !defined(__MSTR_H__)
#define __MSTR_H__

#include "common.h"
#include "fiber.h"
#include "dbm.h"
#include "proto.h"
#include "connector.h"
#include "world.h"

namespace pfm {
using namespace sfc;
namespace mstr {
class fiber : public rpc::basic_fiber {
public:
	struct account_info {
		UUID		m_uuid;
		world_id	m_login_wid;
		account_info() : m_uuid(), m_login_wid(NULL) {}
		bool is_login() const { return m_login_wid != NULL; }
		bool login(world_id wid) {
			return __sync_bool_compare_and_swap(&m_login_wid, NULL, wid);
		}
		const UUID &uuid() const { return m_uuid; }
		int save(char *&p, int &l, void *) {
			int thissz = (int)sizeof(*this);
			if (l <= thissz) {
				ASSERT(false);
				if (!(p = (char *)nbr_malloc(thissz))) {
					return NBR_EMALLOC;
				}
				l = thissz;
			}
			memcpy(p, (void *)&m_uuid, sizeof(UUID));
			return sizeof(UUID);
		}
		int load(const char *p, int l, void *) {
			m_uuid = *(UUID *)p;
			return NBR_OK;
		}
	};
	typedef pmap<account_info, const char*> account_list;
	typedef rpc::basic_fiber super;
	static account_list m_al;
public:	/* master quorum base replication */
	typedef super::quorum_context quorum_context;
	int quorum_vote_commit(world *w, MSGID msgid, quorum_context *ctx, serializer &sr);
	int quorum_global_commit(world *w, quorum_context *ctx, int result);
	quorum_context *init_context(world *w);
	static int quorum_vote_callback(rpc::response &r, class conn *c, void *ctx);
public:
	static int init_global(int max_account, const char *dbpath);
	static void fin_global() { m_al.fin(); }
public:
	int respond(bool err, serializer &sr) {
		return basic_fiber::respond<mstr::fiber>(err, sr);
	}
	int send_error(int r) { return basic_fiber::send_error<mstr::fiber>(r); }
	int call_login(rpc::request &req);
	int resume_login(rpc::response &res);
	int call_logout(rpc::request &req);
	int resume_logout(rpc::response &res);
	int call_replicate(rpc::request &req, char *, int) 
		{ ASSERT(false); return NBR_ENOTSUPPORT; }
	int resume_replicate(rpc::response &res) {
		ASSERT(false); return NBR_ENOTSUPPORT; }
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
	int call_node_regist(rpc::request &);
};
}

class pfmm : public app::daemon {
protected:
	fiber_factory<mstr::fiber> &m_ff;
	class connector_factory &m_cf;
	dbm m_db;
	serializer m_sr;
public:
	pfmm(fiber_factory<mstr::fiber> &ff, 
		class connector_factory &cf) : 
		m_ff(ff), m_cf(cf), m_db(), m_sr() {}
	fiber_factory<mstr::fiber> &ff() { return m_ff; }
	serializer &sr() { return m_sr; }
	base::factory *create_factory(const char *sname);
	int	create_config(config* cl[], int size);
	int	boot(int argc, char *argv[]);
	void shutdown();
	int	initlib(CONFIG &c) { return NBR_OK; }
};


namespace mstr {
/* session */
class session : public conn {
public:
	static class pfmm *m_daemon;
	static bool m_test_mode;
public:
	typedef conn super;
	session() : super() {}
	~session() {}
	static class pfmm &app() { return *m_daemon; }
	static int node_delete_cb(serializer &) { return NBR_OK; }
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
	int on_close(int reason) {
		if (has_node_data()) {
			session::app().ff().wf().remove_node(addr());
			world_factory::iterator wit =
				session::app().ff().wf().begin(), tmp;
			for (; wit != session::app().ff().wf().end(); ) {
				if (wit->nodes().use() == 0) {
					/* FIXME : need to notice other master nodes? */
					TRACE("World destroy (%s)\n", wit->id());
					tmp = wit;
					wit = session::app().ff().wf().next(wit);
					session::app().ff().wf().unload(tmp->id(),
						session::app().ff().vm());
					continue;
				}
				int r;
				serializer &sr = app().ff().sr();
				PREPARE_PACK(sr);
				if ((r = rpc::node_ctrl_cmd::del::pack_header(
					sr, app().ff().new_msgid(),
					wit->id(), nbr_str_length(wit->id(), max_wid),
					node_data()->iden,
					nbr_str_length(node_data()->iden, address::SIZE))) < 0) {
					ASSERT(false);
					continue;
				}
				if ((r = app().ff().run_fiber(node_delete_cb, sr.p(), sr.len())) < 0) {
					ASSERT(false);
					continue;
				}
				wit = session::app().ff().wf().next(wit);
			}
		}
		return super::on_close(reason);
	}
	int on_recv(char *p, int l) {
		app().ff().recv((class conn *)this, p, l, true);
		return NBR_OK;
	}
	int on_event(char *p, int l) {
		return app().ff().recv((class conn *)this, p, l, true);
	}
};

class msession : public session {
public:
	static int node_regist_cb(serializer &sr) {
		rpc::response res;
		PREPARE_UNPACK(sr);
		if (sr.unpack(res, sr.p(), sr.len()) > 0 && res.success()) {
			TRACE("node regist success\n");
		}
		return NBR_OK;
	}
	int on_open(const config &cfg) {
		if (!app().ff().ffutil::initialized() && !app().ff().init_tls()) {
			ASSERT(false);
			return NBR_EINVAL;
		}
		int r;
		serializer &sr = app().ff().sr();
		PREPARE_PACK(sr);
		factory *f = app().find_factory<factory>("mstr");
		if (!f) { return NBR_ENOTFOUND; }
		if ((r = rpc::node_ctrl_cmd::regist::pack_header(
			sr, app().ff().new_msgid(),
			f->ifaddr(), f->ifaddr().len(), master_node)) < 0) {
			ASSERT(false);
			return r;
		}
		if ((r = app().ff().run_fiber(node_regist_cb, sr.p(), sr.len())) < 0) {
			ASSERT(false);
			return r;
		}
		return super::on_open(cfg);
	}
	int on_close(int reason) { return super::on_close(reason); }
};
}
}

#endif
