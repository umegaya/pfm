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
public:
	typedef conn super;
	session() : super() {}
	~session() {}
	static class pfmm &app() { return *m_daemon; }
public:
	pollret poll(UTIME ut, bool from_worker) {
		/* check timeout */
		app().ff().poll(time(NULL));
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
}

base::factory *
pfmm::create_factory(const char *sname)
{
	if (strcmp(sname, "mstr") == 0) {
		return new base::factory_impl<mstr::session>;
	}
	if (strcmp(sname, "svnt") == 0) {
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
			"mstr",
			"",
			10,
			60, opt_not_set,
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
			"svnt",
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
	int r;
	conn_pool_impl *fc;
	mstr::finder_factory *fdr;
	INIT_OR_DIE((r = m_db.init("mstr/db/uuid.tch")) < 0, r,
		"uuid DB init fail (%d)\n", r);
	INIT_OR_DIE((r = UUID::init(m_db)) < 0, r,
		"UUID init fail (%d)\n", r);
	INIT_OR_DIE((r = mstr::fiber::init_global(10000, "mstr/db/al.tch")) < 0, r,
		"mstr::fiber::init fails(%d)\n", r);
	INIT_OR_DIE(!(fc = find_factory<conn_pool_impl>("svnt")), NBR_ENOTFOUND, 
		"conn_pool not found (%p)\n", fc);
	INIT_OR_DIE(!(fdr = find_factory<mstr::finder_factory>("finder")), NBR_ENOTFOUND,
		"conn_pool not found (%p)\n", fc);
	INIT_OR_DIE((r = ff().wf().cf()->init(conn_pool::cast(fc), 100, 100, 100)) < 0, r, 
		"init connector factory fails (%d)\n", r);
	ff().set_finder(fdr);
	INIT_OR_DIE((r = ff().of().init(10000, 1000, 0, "mstr/db/mof.tch")) < 0, r,
		"object factory creation fail (%d)\n", r);
	INIT_OR_DIE((r = ff().wf().init(
		256, 256, -1, opt_threadsafe | opt_expandable)) < 0, r,
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

