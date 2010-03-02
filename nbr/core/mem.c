/****************************************************************
 * mem.c
 * 2008/06/26 iyatomi : create
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
#include "common.h"
#include "mem.h"
#if defined(_DEBUG)
#include <stdlib.h>
#include <memory.h>
#endif


/*-------------------------------------------------------------*/
/* external methods											   */
/*-------------------------------------------------------------*/
#if defined(_DEBUG)
int g_mem_log = 0;
void*
_nbr_mem_alloc(size_t s, const char *file, int line)
{
	void *p = malloc(s);
	if (g_mem_log) {
		TRACE("alloc %p at %s(%u)\n", p, file, line);
	}
	return p;
}

void*
_nbr_mem_calloc(size_t n, size_t s, const char *file, int line)
{
	void *p = calloc(n, s);
	if (g_mem_log) {
		TRACE("alloc %p at %s(%u)\n", p, file, line);
	}
	return p;
}


void
nbr_mem_free(void *p)
{
	if (g_mem_log) {
		TRACE("nbr_mem_free: free %p\n", p);
	}
	free(p);
}

void
nbr_mem_zero(void *p, size_t s)
{
	bzero(p, s);
}

void
nbr_mem_copy(void *dst, const void *src, size_t s)
{
	memcpy(dst, src, s);
}

int
nbr_mem_cmp(const void *dst, const void *src, size_t s)
{
	return memcmp(dst, src, s);
}

void *
nbr_mem_move(void *dst, const void *src, size_t s)
{
	return memmove(dst, src, s);
}
#endif
