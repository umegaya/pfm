#if !defined(__MSTR_H__)
#define __MSTR_H__

#include "common.h"
#include "fiber.h"
#include "dbm.h"
#include "proto.h"

namespace pfm {
using namespace sfc;

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
}

#endif
