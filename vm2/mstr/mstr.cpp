#include "finder.h"
#include "mstr.h"
#include "world.h"
#include "object.h"
#include "connector.h"

using namespace pfm;

namespace pfm {
namespace mstr {
/* session */
class session : public conn {
public:
	static class pfmm *m_daemon;
	static bool m_test_mode;
	static char m_test_world_id[];
public:
	typedef conn super;
	session() : super() {}
	~session() {}
	static class pfmm &app() { return *m_daemon; }
public:
	pollret poll(UTIME ut, bool from_worker) {
		/* check timeout */
		if (from_worker) {
			app().ff().poll(time(NULL));
		}
		return super::poll(ut, from_worker);
	}
	void fin()						{}
	int on_open(const config &cfg) {
		if (m_test_mode) {
			if (!app().ff().ffutil::initialized() && !app().ff().init_tls()) {
				ASSERT(false);
				return NBR_EINVAL;
			}
			int r;
			UUID uuid;
			address a;
			serializer &sr = app().ff().sr();
			PREPARE_PACK(sr);
			if ((r = rpc::node_ctrl_cmd::add::pack_header(
				sr, app().ff().new_msgid(),
				m_test_world_id, 9/* rtkonline */,
				super::node_data()->iden,
				strlen(super::node_data()->iden),
				"", uuid, "svnt/ll/rtkonline/main.lua",
				0, NULL)) < 0) {
				ASSERT(false);
				return r;
			}
			if ((r = app().ff().run_fiber(sr.p(), sr.len())) < 0) {
				ASSERT(false);
				return r;
			}
		}
		return super::on_open(cfg);
	}
	int on_close(int reason) { return super::on_close(reason); }
	int on_recv(char *p, int l) {
		return app().ff().recv((class conn *)this, p, l, true);
	}
	int on_event(char *p, int l) {
		return app().ff().recv((class conn *)this, p, l, true);
	}
};

class msession : public session {
public:
	int on_open(const config &cfg) { return super::on_open(cfg); }
};

/* finder */
class finder : public cluster::finder_session {
public:
	finder(SOCK s, base::factory *f) : cluster::finder_session(s,f) {}
	int on_recv(char *p, int l) {
		session::app().ff().recv(this, p, l, true);
		return NBR_OK;
	}
};

class finder_factory : public cluster::finder_factory {
public:
	int init(const config &cfg) {
		return cluster::finder_factory::init(cfg, on_recv<finder>);
	}
	void poll(UTIME ut) {

	}
};


/* config */
class config : public util::config {
public:
	typedef util::config super;
	config(BASE_CONFIG_PLIST);
};

config::config(BASE_CONFIG_PLIST) : super(BASE_CONFIG_CALL) {}
}
pfmm *mstr::session::m_daemon = NULL;
#if defined(_DEBUG)
bool mstr::session::m_test_mode = true;
#else
bool mstr::session::m_test_mode = false;
#endif
char mstr::session::m_test_world_id[] = "rtkonline";
}

base::factory *
pfmm::create_factory(const char *sname)
{
	if (strcmp(sname, "be") == 0) {
		return new base::factory_impl<mstr::msession>;
	}
	if (strcmp(sname, "mstr") == 0) {
		base::factory_impl<mstr::session> *fc =
				new base::factory_impl<mstr::session>;
		conn_pool_impl *cpi = (conn_pool_impl *)fc;
		ff().wf().cf()->set_pool(conn_pool::cast(cpi));
		return fc;
	}
	if (strcmp(sname, "finder") == 0) {
		return new mstr::finder_factory;
	}
	ASSERT(false);
	return NULL;
}

int
pfmm::create_config(config* cl[], int size)
{
	CONF_START(cl);
	CONF_ADD(base::config, (
			"be",
			"",
			10,
			60, opt_expandable,
			64 * 1024, 64 * 1024,
			0, 0,
			10000,	-1,
			"TCP", "eth0",
			1 * 100 * 1000/* 100msec task span */,
			1 * 1000 * 1000/* after 1s, again try to connect */,
			kernel::INFO,
			nbr_sock_rparser_bin16,
			nbr_sock_send_bin16,
			util::config::cfg_flag_not_set));
	CONF_ADD(mstr::config, (
			"mstr",
			"0.0.0.0:9000",
			10,
			60, opt_expandable,
			64 * 1024, 64 * 1024,
			0, 0,
			10000,	-1,
			"TCP", "eth0",
			1 * 100 * 1000/* 100msec task span */,
			1 * 1000 * 1000/* after 1s, again try to connect */,
			kernel::INFO,
			nbr_sock_rparser_bin16,
			nbr_sock_send_bin16,
			util::config::cfg_flag_server));
	CONF_ADD(cluster::finder_property, (
			"finder",
			"0.0.0.0:8888",
			10,
			30, opt_expandable,/* max 10 session/30sec timeout */
			256, 2048, /* send 256b,recv2kb */
			0, 0,/* no ping */
			-1,0,/* no query buffer */
			"UDP", "eth0",
			10 * 1000 * 1000/* every 10 sec, send probe command */,
			0/* never wait ld recovery */,
			kernel::INFO,
			nbr_sock_rparser_raw,
			nbr_sock_send_raw,
			config::cfg_flag_server,
			finder_property::MCAST_GROUP,
			9999, 1/* ttl = 1 */));
	CONF_END();
}

int
pfmm::boot(int argc, char *argv[])
{
	mstr::session::m_daemon = this;
	if (argc > 1 && strncmp(argv[1], "--test", sizeof("--test"))) {
		int tmode;
		SAFETY_ATOI(argv[1] + sizeof("--test") + 1, tmode, int);
		mstr::session::m_test_mode = (tmode != 0);
	}
	int r;
	conn_pool_impl *fc;
	mstr::finder_factory *fdr;
	INIT_OR_DIE((r = m_db.init("mstr/db/uuid.tch")) < 0, r,
		"uuid DB init fail (%d)\n", r);
	INIT_OR_DIE((r = UUID::init(m_db)) < 0, r,
		"UUID init fail (%d)\n", r);
	INIT_OR_DIE((r = mstr::fiber::init_global(10000, "mstr/db/al.tch")) < 0, r,
		"mstr::fiber::init fails(%d)\n", r);
	INIT_OR_DIE(!(fc = find_factory<conn_pool_impl>("mstr")), NBR_ENOTFOUND,
		"conn_pool not found (%p)\n", fc);
	INIT_OR_DIE(!(fdr = find_factory<mstr::finder_factory>("finder")), NBR_ENOTFOUND,
		"conn_pool not found (%p)\n", fc);
	INIT_OR_DIE((r = ff().wf().cf()->init(conn_pool::cast(fc), 100, 100, 100)) < 0, r, 
		"init connector factory fails (%d)\n", r);
	ff().set_finder(fdr);
	INIT_OR_DIE((r = ff().of().init(10000, 1000, 0, "mstr/db/of.tch")) < 0, r,
		"object factory creation fail (%d)\n", r);
	INIT_OR_DIE((r = ff().wf().init(
		256, 256, opt_threadsafe | opt_expandable, "mstr/db/wf.tch")) < 0, r,
		"object factory creation fail (%d)\n", r);
	INIT_OR_DIE((r = ff().init(10000, 100, 10)) < 0, r,
		"fiber_factory init fails(%d)\n", r);
	return NBR_OK;
}

void
pfmm::shutdown()
{
	UUID::fin(m_db);
	m_db.fin();
	mstr::fiber::fin_global();
	ff().wf().cf()->fin();
	ff().of().fin();
	ff().wf().fin();
	ff().fin();
}

