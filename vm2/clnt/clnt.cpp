#include "clnt.h"
#include "cp.h"

using namespace pfm;

extern int custom_respond(pfm::fiber *f, bool err, pfm::serializer &sr)
{
	return ((pfm::clnt::fiber *)f)->respond(err, sr);
}

namespace pfm {
namespace clnt {
/* config */
class config : public util::config {
public:
	char m_scppath[256];
public:
	typedef util::config super;
	config(BASE_CONFIG_PLIST, const char *scppath) :
		super(BASE_CONFIG_CALL) {
		strncpy(m_scppath, scppath, 256);
	}
	virtual int set(const char *k, const char *v) {
		if (util::config::cmp("scppath", k)) {
			nbr_str_copy(m_scppath, sizeof(m_scppath), v, MAX_VALUE_STR);
			return NBR_OK;
		}
		return util::config::set(k, v);
	}
};
}


/* clnt::session impl */
void clnt::session::fin() {
	if (m_wd) { app().wl().destroy(m_wd); m_wd = NULL; }
}

/* clnt::fiber implementation */
int clnt::fiber::quorum_vote_commit(MSGID msgid,
		quorum_context *ctx, pfm::serializer &sr)
{
	return super::thrd_quorum_vote_commit(msgid, ctx, sr);
}
pfm::basic_fiber::quorum_context *clnt::fiber::init_context(
		world *w, quorum_context *qc)
{
	return super::thrd_quorum_init_context(w, qc);
}

int clnt::fiber::quorum_global_commit(world *w, quorum_context *ctx, int result)
{
	return super::thrd_quorum_global_commit(w, ctx, result);
}

int clnt::fiber::quorum_vote_callback(rpc::response &r, THREAD t, void *p)
{
	return super::thrd_quorum_vote_callback(r, t, p);
}

int clnt::fiber::respond_callback(U64 param, bool err, pfm::serializer &sr) {
	int r;
	watcher_data *wd = (watcher_data *)param;
	rpc::response res;
	sr.unpack_start(sr.p(), sr.len());
	if ((r = sr.unpack(res, sr.p(), sr.len())) <= 0) {
		return NBR_ESHORT;
	}
	r = res.success() ?
		wd->fn((watcher)wd, wd->c, NULL, sr.p(), sr.len()) :
		wd->fn((watcher)wd, wd->c, (void *)&(res.err()), sr.p(), sr.len());
	if (!res.success() && r >= 0) { r = NBR_EINVAL; }
	if (wd != ((clnt::session *)wd->c)->wd()) {
		session::app().wl().destroy(wd);
	}
	return r;
}
int clnt::fiber::call_ll_exec_client(rpc::request &req, object *o,
		bool trusted, char *p, int l)
{
	if (!(m_ctx.co = ff().co_create(this))) {
		send_error(NBR_EEXPIRE); return NBR_EEXPIRE;
	}
	return m_ctx.co->call(rpc::ll_exec_request::cast(req),
		(const char *)rpc::ll_exec_request::cast(req).method());
}
int clnt::fiber::call_login(rpc::request &rq) {
	int r;
	MSGID msgid = INVALID_MSGID;
	rpc::login_request &req = rpc::login_request::cast(rq);
	if ((msgid = new_msgid()) == INVALID_MSGID) {
		r = NBR_EEXPIRE;
		goto error;
	}
	if ((r = rpc::login_request::pack_header(ff().sr(), msgid,
		req.wid(), req.wid().len() - 1,
		req.account(), req.authdata(), req.authdata().len())) < 0) {
		goto error;
	}
	if ((r = yielding(msgid)) < 0) {
		goto error;
	}
	if ((r = get_watcher()->connection()->send(
		ff().sr().p(), ff().sr().len())) < 0) {
		goto error;
	}
	return NBR_OK;
error:
	if (msgid != INVALID_MSGID) { ff().fiber_unregister(msgid); }
	if (r < 0) {
		send_error(r);
	}
	return r;
}

int clnt::fiber::resume_login(rpc::response &res) {
	int r = NBR_OK;
	world *w = NULL;
	quorum_context *ctx = NULL;
	clnt::session *s = NULL;
	object *o = NULL;
	MSGID msgid = INVALID_MSGID;
	if (res.success()) {
		ASSERT(get_watcher());
		if (!(s = get_watcher()->connection())) {
			r = NBR_ENOTFOUND;
			goto error;
		}
		switch(m_status) {
		case start: {
			w = ff().find_world(s->wid());
			if (!w && !(w = ff().world_new(s->wid()))) {
				r = NBR_EEXPIRE;
				goto error;
			}
			s->set_player_id(res.ret().elem(2));
			s->set_node_data(s->addr(), world::vnode_replicate_num);
			ASSERT(s->valid());
			if (!w->add_node(*s)) {
				r = NBR_EEXPIRE;
				goto error;
			}
			if (!(o = ff().of().find(res.ret().elem(2)))) {
				if (!(o = ff().of().create(res.ret().elem(2), NULL,
					w, ff().vm(), res.ret().elem(1)))) {
					r = NBR_EEXPIRE;
					goto error;
				}
			}
			o->set_flag(object::flag_has_localdata, true);
			if (!(m_qc = new quorum_context)) {
				r = NBR_EMALLOC;
				goto error;
			}
			o->set_flag((object::flag_has_localdata|object::flag_loaded), true);
			if (!(ctx = init_context(w, m_qc))) {
				r = NBR_EMALLOC;
				goto error;
			}
			ASSERT(m_qc == ctx);
			ASSERT(!o->local());
			msgid = new_msgid();
			if (msgid == INVALID_MSGID) {
				r = NBR_EEXPIRE;
				ff().world_destroy(w);
				goto error;
			}
			if ((r = node_ctrl_cmd::vm_init::pack_header(ff().sr(), msgid,
				s->wid(), nbr_str_length(s->wid(), max_wid), "", 
				((const clnt::config &)s->cfg()).m_scppath)) < 0) {
				ff().world_destroy(w);
				goto error;
			}
			if ((r = quorum_vote_commit(msgid, ctx, ff().sr())) < 0) {
				goto error;
			}
			m_status = login_wait_init_vm;
		} break;
		case login_wait_init_vm: {
			if (!(w = ff().find_world(s->wid()))) {
				r = NBR_EEXPIRE;
				goto error;
			}
			if (!(ctx = yld()->p<quorum_context>())) {
				r = NBR_EINVAL;
				goto error;
			}
			ASSERT(m_qc == ctx);
			quorum_global_commit(w, ctx, NBR_OK);
			free_quorum_context();
			rpc::response::pack_header(ff().sr(), m_msgid);
			ff().sr() << NBR_OK;
			ff().sr().pushnil();
			if ((r = respond(false, ff().sr())) < 0) {
				goto error;
			}
		} break;
		}
		return NBR_OK;
	}
	else if (res.err().type() == datatype::INTEGER){
		r = res.err();
		goto error;
	}
	else if (res.err().type() == datatype::BLOB){
		rpc::response::pack_header(ff().sr(), m_msgid);
		ff().sr().pushnil();
		ff().sr().push_string(res.err(), res.err().len());
		if ((r = respond(true, ff().sr())) < 0) {
		}
		goto error;
	}
	else {
		ASSERT(false);
		r = NBR_EINVAL;
	}
error:
	if (r < 0) {
		send_error(r);
	}
	ASSERT(!m_qc || (ctx == m_qc));
	free_quorum_context();
	if (w) { ff().world_destroy(w); }
	if (o) { ff().of().destroy(o->uuid()); }
	if (s) { ff().of().destroy(s->player_id()); }
	return NBR_OK;
}

int clnt::fiber::node_ctrl_vm_init(class world *w,
		rpc::node_ctrl_cmd::vm_init &req, pfm::serializer &sr)
{
	int r;
	MSGID msgid = INVALID_MSGID;
	if (!w) { return NBR_ENOTFOUND; }
	if ((r = ff().vm()->init_world(req.wid(), req.from(), req.srcfile())) < 0) {
		goto error;
	}
	msgid = new_msgid();
	if (msgid == INVALID_MSGID) {
		goto error;
	}
	if ((r = yielding(msgid)) < 0) {
		goto error;
	}
	rpc::response::pack_header(sr, m_msgid);
	sr << msgid;
	sr.pushnil();
	if ((r = respond(false, sr)) < 0) {
		goto error;
	}
	return NBR_OK;
error:
	if (r < 0) {
		send_error(r);
	}
	ff().vm()->fin_world(req.wid());
	if (msgid != INVALID_MSGID) { ff().fiber_unregister(msgid); }
	return r;
}

int clnt::fiber::node_ctrl_vm_init_resume(
		class world *w, rpc::response &rq, pfm::serializer &sr)
{
	if (rq.success()) {
		return NBR_OK;
	}
	else {
		ff().vm()->fin_world(w->id());
		ff().world_destroy(w);
		return rq.err();
	}
}


pfmc *clnt::session::m_daemon = NULL;
}

base::factory *
pfmc::create_factory(const char *sname)
{
	if (strcmp(sname, "be") == 0) {
		return new base::factory_impl<clnt::session, arraypool<clnt::session> >;
	}
	ASSERT(false);
	return NULL;
}

int
pfmc::create_config(config* cl[], int size)
{
	CONF_START(cl);
	CONF_ADD(clnt::config, (
			"be",
			"127.0.0.1:8100",
			100,
			60, opt_expandable,
			64 * 1024, 64 * 1024,
			100 * 1000 * 1000, 5 * 1000 * 1000,
			-1,	0,
			"TCP", "eth0",
			1 * 100 * 1000/* 100msec task span */,
			1 * 1000 * 1000/* 1sec reconnection */,
			kernel::INFO,
			nbr_sock_rparser_bin16,
			nbr_sock_send_bin16,
			util::config::cfg_flag_not_set,
			"clnt/ll/test/main.lua"));
	CONF_END();
}

int
pfmc::boot(int argc, char *argv[])
{
	clnt::session::m_daemon = this;
	int r;
	conn_pool::rclnt_cp *fc;
	INIT_OR_DIE((r = ff().init(200, 100, 10)) < 0, r,
		"fiber_factory init fails(%d)\n", r);
	INIT_OR_DIE(!(ff().of().init(10000, 1000, 0, "clnt/db/of.tch")),
		r = NBR_EMALLOC, "object factory creation fail (%d)\n", r);
	INIT_OR_DIE(!(ff().wf().init(
		256, 256, opt_threadsafe | opt_expandable, "clnt/db/wf.tch")),
		r = NBR_EMALLOC, "world factory creation fail (%d)\n", r);
	INIT_OR_DIE(!(fc = find_factory<conn_pool::rclnt_cp>("be")), NBR_ENOTFOUND,
		"conn_pool not found (%p)\n", fc);
	ff().cp()->set_pool(fc);
	INIT_OR_DIE((r = ff().wf().cf()->init(ff().cp(), fc->ifaddr(), 100, 100, 100)) < 0, r,
		"init connector factory fails (%d)\n", r);
	INIT_OR_DIE(!m_wl.init(10000, -1, opt_threadsafe | opt_expandable),
		r = NBR_EMALLOC, "watcher initialize fails (%d)\n", r);
	INIT_OR_DIE(!m_sl.init(3, -1, opt_threadsafe | opt_expandable),
		r = NBR_EMALLOC, "serializer initialize fails (%d)\n", r);
	return NBR_OK;
}

void
pfmc::shutdown()
{
	ff().of().fin();
	ff().wf().fin();
	ff().wf().cf()->fin();
	ff().fin();
	m_wl.fin();
	m_sl.fin();
}

/* APIs  */
connector_factory g_cf;
object_factory g_of;
world_factory g_wf;
fiber_factory<clnt::fiber> g_ff(g_of, g_wf);
pfmc g_daemon(g_ff);
namespace pfm {
namespace clnt {
int init(const char *cfg)
{
	int argc = cfg ? 1 : 0;
	char *argv[] = { (char *)cfg, NULL };
	g_wf.set_cf(&g_cf);
	return g_daemon.init(argc, argv);
}

void poll(UTIME ut)
{
	if (g_daemon.alive()) { g_daemon.heartbeat(); }
}

void fin()
{
	pfm::watcher::show_stats();
	g_daemon.fin();
}

watcher login(const char *host, const char *wid,
		const char *acc, const char *authd, int dlen,
		watchercb cb)
{
	config *cfg = g_daemon.find_config<config>("be");
	factory_impl<clnt::session,arraypool<clnt::session> > *cp = 
		g_daemon.find_factory<factory_impl<clnt::session,
			arraypool<clnt::session> > >("be");
	if (!cp) { return NULL; }
	clnt::session *c = cp->pool().create();
	if (!c) { return NULL; }
	watcher_data *wd = g_daemon.wl().create();
	if (!wd) { 
		cp->pool().destroy(c);
		return NULL; 
	}
	if (!g_daemon.ff().wf().find(wid)) {
		if (!g_daemon.ff().wf().create(wid, 50, 50)) {
			cp->pool().destroy(c);
			return NULL;
		}
	}
	address to(host ? host : cfg->m_host);
	if (!g_daemon.ff().wf().cf()->insert(to, c)) {
		cp->pool().destroy(c);
		g_daemon.wl().destroy(wd);
		g_daemon.ff().wf().destroy(wid);
		return NULL;
	}
	wd->c = c;
	wd->fn = cb;
	c->set_account_info(wd, wid, acc, authd, dlen);
	if (cp->connect(c, to) < 0) {
		g_daemon.wl().destroy(wd);
		g_daemon.ff().wf().destroy(wid);
		cp->pool().destroy(c);
		return NULL;
	}
	return (watcher)wd;
}

watcher call(client c, args a, watchercb cb)
{
	serializer *sr = (serializer *)a;
	watcher_data *wd = g_daemon.wl().create();
	if (!wd) { return NULL; }
	wd->c = c;
	wd->fn = cb;
	if (g_daemon.ff().run_fiber(fiber::to_prm(wd), sr->p(), sr->len()) < 0) {
		g_daemon.wl().destroy(wd);
		return NULL;
	}
	return (watcher)wd;
}

const char *error_to_s(char *b, int l, errobj e)
{
	int r;
	rpc::data *d = (rpc::data *)e;
	switch(d->type()) {
	case datatype::INTEGER: {
		r = *d;
		snprintf(b, l, "system error (%d)", r);
		break;
	}
	case datatype::BLOB:
		snprintf(b, l, "%s", (const char *)*d);
		break;
	default:
		ASSERT(false);
		return "";
	}
	return b;
}

obj get_from(client c)
{
	clnt::session *s = (clnt::session *)c;
	return g_daemon.ff().of().find(s->player_id());
}

args init_args(int len)
{
	clnt::serializer *sr = new clnt::serializer();
	if (!sr) { return sr; }
	if (sr->init(len) < 0) {
		delete sr;
		return NULL;
	}
	return sr;
}

void fin_args(args a)
{
	delete (clnt::serializer*)a;
}

int setup_args(args a, obj o, const char *method, bool local_call, int n_args)
{
	clnt::serializer *sr = (clnt::serializer *)a;
	object *ob = (object *)o;
	sr->set();
	return rpc::ll_exec_request::pack_header(
		*sr, clnt::session::app().ff().new_msgid(), 
		ob->uuid(), ob->klass(),
		method, nbr_str_length(method, 65536),
		ob->belong()->id(), nbr_str_length(ob->belong()->id(), max_wid),
		local_call ? rpc::ll_exec_local : rpc::ll_exec, n_args
	);
}
int args_int(args a, int i)
{
	return *((clnt::serializer *)a) << i;
}
int args_bigint(args a, long long ll)
{
	return *((clnt::serializer *)a) << ll;
}
int args_double(args a, double d)
{
	return *((clnt::serializer *)a) << d;
}
int args_string(args a, const char *str)
{
	return ((clnt::serializer *)a)->push_string(str, nbr_str_length(str, 65536));
}
int args_array(args a, int size)
{
	return ((clnt::serializer *)a)->push_array_len(size);
}
int args_map(args a, int size)
{
	return ((clnt::serializer *)a)->push_map_len(size);
}
int args_bool(args a, bool b)
{
	return *((clnt::serializer *)a) << b;
}
int args_nil(args a)
{
	return ((clnt::serializer *)a)->pushnil();
}
int args_blob(args a, char *p, int l)
{
	return ((clnt::serializer *)a)->push_raw(p, l);
}

}
}

