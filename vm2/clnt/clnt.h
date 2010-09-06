#if !defined(__CLNT_H__)
#define __CLNT_H__

#include "common.h"
#include "proto.h"
#include "connector.h"
#include "fiber.h"
#include "yuec.h"
#include "world.h"
#include "object.h"
#include "serializer.h"

namespace pfm {
class clnt_session;
namespace clnt {
/* watcher */
struct watcher_data {
	client c;
	watchercb fn;
	UTIME t;
	watcher_data() : c(NULL), fn(NULL) { t = nbr_time(); }
	class clnt_session *connection() { return (class clnt_session *)c; }
};

/* serializer */
class serializer : public pfm::serializer {
public:
	char *m_pk;
	int m_lpk;
	serializer() : pfm::serializer(), m_pk(NULL), m_lpk(0) {}
	~serializer() { if (m_pk) { nbr_free(m_pk); m_pk = NULL; } }
	int init(int len) { return (m_pk = (char *)nbr_malloc(m_lpk = len)) ?
		NBR_OK : NBR_EMALLOC; }
	void set() { pfm::serializer::pack_start(m_pk, m_lpk); }
};

/* fiber */
class fiber : public rpc::basic_fiber {
public:
	typedef rpc::basic_fiber super;
public:	/* master quorum base replication */
	typedef super::quorum_context quorum_context;
	quorum_context *m_qc;
	int quorum_vote_commit(MSGID msgid, quorum_context *ctx, pfm::serializer &sr);
	int quorum_global_commit(world *w, quorum_context *ctx, int result);
	quorum_context *init_context(world *w, quorum_context *qc);
	static int quorum_vote_callback(rpc::response &r, THREAD thrd, void *ctx);
	void free_quorum_context() {if (m_qc) { delete m_qc; m_qc = NULL; }}
public:
	fiber() : super(), m_qc(NULL) {}
	~fiber() { free_quorum_context(); }
	int respond(bool err, pfm::serializer &sr) {
		return basic_fiber::respond<clnt::fiber>(err, sr);
	}
	int send_error(int r) { return basic_fiber::send_error<clnt::fiber>(r); }
	static int respond_callback(U64 param, bool err, pfm::serializer &sr);
	watcher_data *get_watcher() { return (watcher_data *)m_param; }
	inline class clnt_session *get_session();
public:
	int call_ll_exec_client(rpc::request &req, object *o,
			bool trusted, char *p, int l);
	int call_login(rpc::request &req);
	int resume_login(rpc::response &res);
	int node_ctrl_vm_init(class world *,
			rpc::node_ctrl_cmd::vm_init &, pfm::serializer &);
	int node_ctrl_vm_init_resume(class world *, rpc::response &, pfm::serializer &);
};
}

/* daemon */
class pfmc : public app::daemon {
protected:
	fiber_factory<clnt::fiber> &m_ff;
	serializer m_sr;
	array<clnt::watcher_data> m_wl;
	array<clnt::serializer> m_sl;
public:
	pfmc(fiber_factory<clnt::fiber> &ff) : m_ff(ff), m_sr() {}
	fiber_factory<clnt::fiber> &ff() { return m_ff; }
	serializer &sr() { return m_sr; }
	array<clnt::watcher_data> &wl() { return m_wl; }
	base::factory *create_factory(const char *sname);
	int	create_config(config* cl[], int size);
	int	boot(int argc, char *argv[]);
	void shutdown();
	int	initlib(CONFIG &c) { return NBR_OK; }
};


namespace clnt {
/* session */
class session : public conn {
public:
	static class pfmc *m_daemon;
	char m_acc[rpc::login_request::max_account];
	char *m_authdata;
	world_id m_wid;
	int m_alen;
	watcher_data *m_wd;
	UUID m_uuid;
public:
	typedef conn super;
	static class pfmc &app() { return *m_daemon; }
public:
	session() : super(), m_authdata(NULL), m_wid(NULL), m_wd(NULL) {}
	~session() {
		if (m_wid) { free((void *)m_wid); m_wid = NULL; }
		if (m_authdata) { nbr_free(m_authdata); m_authdata = NULL; }
	}
	void set_account_info(watcher_data *wd, world_id wid,
		const char *acc, const char *auth, int alen) {
		nbr_str_copy(m_acc, sizeof(m_acc), acc, sizeof(m_acc));
		m_authdata = (char *)nbr_malloc(alen);
		memcpy(m_authdata, auth, alen);
		m_alen = alen;
		m_wd = wd;
		m_wid = strndup(wid, max_wid);
	}
	void set_player_id(const UUID &id) { m_uuid = id; }
	const UUID &player_id() const { return m_uuid; }
	world_id wid() const { return m_wid; }
	watcher_data *wd() { return m_wd; }
	int pack(pfm::serializer &sr) {
		return rpc::login_request::pack_header(sr, app().ff().new_msgid(),
			m_wid, nbr_str_length(m_wid, max_wid),
			m_acc, m_authdata, m_alen);
	}
	pollret poll(UTIME ut, bool from_worker) {
		/* check timeout */
		if (from_worker) { app().ff().poll(time(NULL)); }
		return super::poll(ut, from_worker);
	}
	void fin();
	int on_open(const config &cfg) {
		int r = super::on_open(cfg);
		if (r < 0) { return r; }
		/* send login command */
		PREPARE_PACK(app().sr());
		if ((r = pack(app().sr())) < 0) {
			return r;
		}
		r = app().ff().run_fiber(fiber::to_prm(m_wd), 
			app().sr().p(), app().sr().len());
		return r;
	}
	int on_close(int reason) { return super::on_close(reason); }
	int on_recv(char *p, int l) {
		return app().ff().recv((class conn *)this, p, l, true);
	}
	int on_event(char *p, int l) {
		return app().ff().recv((class conn *)this, p, l, true);
	}
};
}
class clnt_session : public clnt::session {
public:
	static class clnt_session *cast(conn *c) {
		return static_cast<class clnt_session *>(c); 
	}
};
inline pfm::clnt_session *clnt::fiber::get_session()
{
	if (m_type != from_socket) { ASSERT(false); return NULL; }
	return clnt_session::cast(m_socket);
}
}

#endif
