/***************************************************************
 * defs.h : sfc macro definition
 * 2010/01/23 iyatomi : create
 *                             Copyright (C) 2008-2009 Takehiro Iyatomi
 * This file is part of libnbr.
 * libnbr is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either
 * version 2.1 of the License or any later version.
 * libnbr is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU Lesser General Public License for more details.
 * You should have received a copy of
 * the GNU Lesser General Public License along with libnbr;
 * if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 ****************************************************************/
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
	if (nbr_str_atobn(p, &tmp, 256)) { return NBR_EFORMAT; }	\
	r = (type)tmp;					\
}

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
#define POP_TEXT_STR(s,l)	if (!(__buf = (char *)nbr_str_divide(" ",__buf, s, l))) { \
								TRACE("pop err@%s(%u)\n", __FILE__, __LINE__);	\
								return NBR_ESHORT; }
#define POP_TEXT_NUM(n,t)	{ char __tmp[256]; POP_TEXT_STR(__tmp, sizeof(__tmp)); \
								SAFETY_ATOI(__tmp, n, t); }
#define POP_TEXT_BIGNUM(n,t)	{ char __tmp[256]; POP_TEXT_STR(__tmp, sizeof(__tmp)); \
								SAFETY_ATOBN(__tmp, n, t); }
#define POP_TEXT_CURPOS()	(__buf)
#endif//__SFC_MACRO_H__
