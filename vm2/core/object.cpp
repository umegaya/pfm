#include "object.h"
#include "connector.h"
#include "world.h"
#include "ll.h"
#include "fiber.h"
#include "cp.h"

using namespace pfm;

#if defined(_TEST)
int (*object::m_test_save)(class object *, char *&, int &) = NULL;
int (*object::m_test_load)(class object *, const char *, int) = NULL;
int (*object::m_test_request)
	(class object *, MSGID, ll *, class serializer &) = NULL;
#endif

/* object */
int object::request(MSGID msgid, ll *vm, serializer &sr)
{
#if defined(_TEST)
	if (m_test_request) {
		return m_test_request(this, msgid, vm, sr);
	}
#endif
	if (local() || has_localdata()) {
		if (thread_current(vm)) {
			ASSERT(false);
			return NBR_EINVAL;
		}
		SWKFROM from = { fiber::from_thread, vm->thrd() };
		return nbr_sock_worker_event(
			&from, m_vm->thrd(), sr.p(), sr.len());
	}
	return m_wld->request(msgid, uuid(), sr);
}

int object::save(char *&p, int &l, void *ctx)
{
	if (!ctx) { return NBR_OK; }
	return ((ll::coroutine *)ctx)->save_object(*this, p, l);
}

int object::load(const char *p, int l, void *ctx)
{
	if (!ctx) { return NBR_OK; }
	return ((ll::coroutine *)ctx)->load_object(*this, p, l);
}


/* object_factory */
