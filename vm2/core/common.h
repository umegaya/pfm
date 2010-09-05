#if !defined(__COMMON_H__)
#define __COMMON_H__

#include "sfc.hpp"
#include "nbr_pkt.h"
#include "macro.h"

#if defined(_TEST)
#define TEST_VIRTUAL virtual
#else
#define TEST_VIRTUAL
#endif

#define LOG(...) fprintf(stderr, __VA_ARGS__)	/* kari */

#define PREPARE_PACK(scr)		PREPARE_PACK_LOW(scr, __b)
#define PREPARE_PACK_LOW(scr, bufname)	\
		char bufname[4 * 1024];	\
		sr_disposer srd##bufname((scr));	\
		(scr).pack_start(bufname, sizeof(bufname));

#define PREPARE_UNPACK(scr)		\
		(scr).unpack_start((scr).p(), (scr).len());

#define INIT_OR_DIE(cond, ret, ...)	\
	if (cond) {	\
		TRACE(__VA_ARGS__);	\
		return ret;	\
	}

/* numerical conversion */
#define SAFETY_ATOI(p, r, type)		\
{									\
	S32 tmp;						\
	if (nbr_str_atoi(p, &tmp, 256)) { return NBR_EFORMAT; }	\
	r = (type)tmp;					\
}
#define SAFETY_ATOBN(p, r, type)	\
{									\
	S64 tmp;						\
	if (nbr_str_atobn(p, &tmp, 256)) { return NBR_EFORMAT; }	\
	r = (type)tmp;					\
}

/* configuration */
#define CONF_START(cl)	{ int __nconf = 0; config **__cl = cl;
#define CONF_ADD(conftype, initializer) __cl[__nconf++] = new conftype initializer;
#define CONF_END() return __nconf; }

/* function argument */
#define UNUSED(val)

/* macro for protocol */
#define POP_ADDR(a)					\
{									\
	int __r;						\
	char *__p = a.a();				\
	POP_STR2(__p, (int)address::SIZE, __r);\
	a.setlen(__r);					\
}
#define PUSH_ADDR(a) PUSH_STR((const char *)a)
#define PUSH_SOCKADDR(sk) {			\
	int __r = (nbr_sock_get_addr(sk, PUSH_BUF(), PUSH_REMAIN()) + 1);	\
	PUSH_SKIP(__r);														\
}

#define PUSH_TEXT_START(p,s)	char *__buf = p, *__p = p; size_t __max = sizeof(p);	\
								PUSH_TEXT(s)
#define PUSH_TEXT(...)		__buf += snprintf(__buf, __max - (__buf - __p),		\
								__VA_ARGS__);									\
							if ((size_t)(__buf - __p) >= __max) { 				\
								TRACE("push err@%s(%u)\n", __FILE__, __LINE__);	\
								return NBR_ESHORT; }
#define PUSH_TEXT_STR(s)	PUSH_TEXT(" %s", s)
#define PUSH_TEXT_NUM(n)	PUSH_TEXT(" %u", n)
#define PUSH_TEXT_BIGNUM(bn)	PUSH_TEXT(" %llu", bn)
#define PUSH_TEXT_CURPOS()	(__buf)
#define PUSH_TEXT_LEN()		(__buf - __p)

#define POP_TEXT_START(p,l)	char *__buf = p;
#define POP_TEXT_STR(s,l)	{ int __l = l; POP_TEXT_STRLOW(s,__l," ") }
#define POP_TEXT_STRLOW(s,l,sep)	if (!(__buf = (char *)nbr_str_divide(sep,__buf, s, &l))) { \
								TRACE("pop err@%s(%u)\n", __FILE__, __LINE__);	\
								return NBR_ESHORT; }
#define POP_TEXT_NUM(n,t)	{ char __tmp[256]; POP_TEXT_STR(__tmp, sizeof(__tmp)); \
								SAFETY_ATOI(__tmp, n, t); }
#define POP_TEXT_BIGNUM(n,t)	{ char __tmp[256]; POP_TEXT_STR(__tmp, sizeof(__tmp)); \
								SAFETY_ATOBN(__tmp, n, t); }
#define POP_TEXT_CURPOS()	(__buf)


#endif
