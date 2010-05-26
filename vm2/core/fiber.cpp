#include "fiber.h"

using namespace pfm;

NBR_TLS ll *fiber_factory::m_vm = NULL;
NBR_TLS serializer *fiber_factory::m_sr = NULL;

bool fiber_factory::init(int max_rpc)
{
	m_max_rpc = max_rpc;
	return super::init(m_max_rpc, m_max_rpc, -1, opt_threadsafe | opt_expandable);
}

bool fiber_factory::init_tls()
{
	if (m_vm) { return true; }
	if (!(m_sr = new serializer)) {
		fin_tls();
		ASSERT(false);
		return false;
	}
	if (!(m_vm = new ll(m_of, m_wf, *m_sr, m_seed))) {
		fin_tls();
		ASSERT(false);
		return false;
	}
	if (m_vm->init(m_max_rpc) < 0) {
		fin_tls();
		ASSERT(false);
		return false;
	}
	return true;
}

void fiber_factory::fin_tls()
{
	if (m_vm) {
		delete m_vm;
		m_vm = NULL;
	}
	if (m_sr) {
		delete m_sr;
		m_sr = NULL;
	}
}
