#include "connector.h"

using namespace pfm;

#if defined(_TEST)
int (*conn::m_test_send)(conn *, char *, int) = NULL;
int (*conn_pool::m_test_connect)(conn_pool *, 
	conn *, const address &, void *p) = NULL; 
#endif

