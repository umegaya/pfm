#if !defined(__SFC_MACRO_H__)
#define __SFC_MACRO_H__

/* trace,assert */
#if defined(_DEBUG)
#include <assert.h>
#define ASSERT(c) 	assert(c)
#define TRACE(...)	fprintf(stderr, __VA_ARGS__)
#else
#define ASSERT(c)
#define TRACE(...)
#endif

/* packetmacro */
#include "nbr_pkt.h"

/* memory related */
#include <stdlib.h>
#include <memory.h>
#define nbr_mem_alloc	malloc
#define nbr_mem_calloc	calloc
#define nbr_mem_free	free
#define nbr_mem_zero	bzero
#define nbr_mem_copy	memcpy
#define nbr_mem_cmp		memcmp
#define nbr_mem_move	memmove

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
	if (nbr_str_atoi(p, &tmp, 256)) { return NBR_EFORMAT; }	\
	r = (type)tmp;					\
}

#define POP_ADDR(a)					\
{									\
	int __r;						\
	const char *__p = a;			\
	POP_STR2(p, address::SIZE, __r);\
	a.setlen(__r);					\
}
#define PUSH_ADDR(a) PUSH_STR((const char *)a)
#endif//__SFC_MACRO_H__
