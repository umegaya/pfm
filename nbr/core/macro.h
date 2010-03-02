/****************************************************************
 * macro.h : storing error
 * 2008/07/14 iyatomi : create
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
#if !defined(__MACRO_H__)
#define __MACRO_H__

#if defined(_DEBUG)
#include <assert.h>
#include <stdio.h>
#if !defined(ASSERT)
#define ASSERT	assert
#endif
#if !defined(VERIFY)
#define VERIFY	ASSERT
#endif
#if !defined(TRACE)
#define TRACE(...)	fprintf(stderr, __VA_ARGS__)
#endif
#else	/* _DEBUG */
#if !defined(ASSERT)
#define ASSERT(...)
#endif
#if !defined(VERIFY)
#define VERIFY(f)	(f)
#endif
#if !defined(TRACE)
#define TRACE(...)
#endif
#endif	/* _DEBUG */

#if !defined(__FUNC__)
#if defined (__GNUC__)
#define	__FUNC__	__func__
#else
/* safety */
#define	__FUNC__	""
#endif
#endif

#define TO_UTIME(sec)	((UTIME)((sec) * 1000 * 1000))
#if !defined(STATIC__JOIN)
#define STATIC__JOIN(a, b)	a ## b
#endif
#if !defined(STATIC_ASSERT)
#define STATIC_ASSERT(f)	typedef char STATIC__JOIN(ZZZZZZZZZ_static___assert_, __LINE__)[(1 / ((f) ? 1 : 0))]
#endif

#endif	/* __MACRO_H__ */
