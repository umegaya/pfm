#include "object.h"
#include "connector.h"
#include "world.h"
#include "ll.h"
#include "fiber.h"

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
	if (local()) {
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

/* object_factory */
