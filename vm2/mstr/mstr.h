#if !defined(__MSTR_H__)
#define __MSTR_H__

#include "common.h"
#include "fiber.h"
#include "dbm.h"
#include "proto.h"

namespace pfm {
using namespace sfc;
using namespace sfc::util;

class pfmm : public app::daemon {
public:
	/* session */
	class session : public base::session {
	public:
		static class pfmm *m_daemon;
	public:
		session() : base::session() {}
		~session() {}
	};
protected:
	fiber_factory<mstr::fiber> &m_ff;
public:
	pfmm(fiber_factory<mstr::fiber> &ff) : m_ff(ff) {
		session::m_daemon = this;
	}
	/* daemon process */
	base::factory *create_factory(const char *sname) { return NULL; }
	int	create_config(config* cl[], int size) { return 0; }
	int	boot(int argc, char *argv[]) { return NBR_OK; }
	int	initlib(CONFIG &c) { return NBR_OK; }
	int on_signal(int signo) { return NBR_OK; }
};
}

#endif
