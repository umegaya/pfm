#include "object.h"

using namespace pfm;

object_factory pfm::g_kvs;
#if defined(_TEST)
int (*object::m_test_save)(class object *, char *&, int &) = NULL;
int (*object::m_test_load)(class object *, const char *, int) = NULL;
int (*object::m_test_request)(class object *, class serializer &) = NULL;
#endif

int
pfm::init_object_factory(int max, const char *opt)
{
	return g_kvs.init(max, max, opt_threadsafe | opt_expandable, opt) ? 
		NBR_OK : NBR_ESYSCALL;
}

void
pfm::fin_object_factory()
{
	g_kvs.fin();
}

/* object */

