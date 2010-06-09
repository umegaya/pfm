#include "svnt.h"
#include "world.h"
#include "object.h"
#include "connector.h"

using namespace pfm;

namespace pfm {
namespace svnt {
/* session */
class session : public conn {
public:
	static class pfms *m_daemon;
public:
	typedef conn super;
	session() : super() {}
	~session() {}
	static class pfms &app() { return *m_daemon; }
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

/* config */
class config : public util::config {
public:
	typedef util::config super;
	config(BASE_CONFIG_PLIST);
};

config::config(BASE_CONFIG_PLIST) : super(BASE_CONFIG_CALL) {}
}
pfms *svnt::session::m_daemon = NULL;
}

base::factory *
pfms::create_factory(const char *sname)
{
	if (strcmp(sname, "clnt") == 0) {
		return new base::factory_impl<svnt::session>;
	}
	if (strcmp(sname, "svnt") == 0) {
		base::factory_impl<svnt::session> *fc =
				new base::factory_impl<svnt::session>;
		conn_pool_impl *cpi = (conn_pool_impl *)fc;
		ff().wf().cf()->set_pool(conn_pool::cast(cpi));
		return fc;
	}
	ASSERT(false);
	return NULL;
}

int
pfms::create_config(config* cl[], int size)
{
	CONF_START(cl);
	CONF_ADD(base::config, (
			"clnt",
			"0.0.0.0:9000",
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
			util::config::cfg_flag_server));
	CONF_ADD(svnt::config, (
			"svnt",
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
	CONF_END();
}

int
pfms::boot(int argc, char *argv[])
{
	svnt::session::m_daemon = this;
	int r;
	conn_pool_impl *fc;
	INIT_OR_DIE((r = m_db.init("db/uuid.tch")) < 0, r,
		"uuid DB init fail (%d)\n", r);
	INIT_OR_DIE((r = UUID::init(m_db)) < 0, r,
		"UUID init fail (%d)\n", r);
	INIT_OR_DIE((r = svnt::fiber::init_global(10000)) < 0, r,
		"svnt::fiber::init fails(%d)\n", r);
	INIT_OR_DIE(!(fc = find_config<conn_pool_impl>("svnt")), NBR_ENOTFOUND,
		"conn_pool not found (%p)\n", fc);
	INIT_OR_DIE((r = ff().wf().cf()->init(conn_pool::cast(fc), 100, 100, 100)) < 0, r,
		"init connector factory fails (%d)\n", r);
	INIT_OR_DIE((r = ff().of().init(10000, 1000, 0, "db/mof.tch")) < 0, r,
		"object factory creation fail (%d)\n", r);
	INIT_OR_DIE((r = ff().wf().init(
		256, 256, -1, opt_threadsafe | opt_expandable)) < 0, r,
		"object factory creation fail (%d)\n", r);
	INIT_OR_DIE((r = ff().init(10000, 100, 10)) < 0, r,
		"fiber_factory init fails(%d)\n", r);
	return NBR_OK;
}

void
pfms::shutdown()
{
	UUID::fin(m_db);
	m_db.fin();
	svnt::fiber::fin_global();
	ff().wf().cf()->fin();
	ff().of().fin();
	ff().wf().fin();
	ff().fin();
}

