/****************************************************************
 * mem.h : low level memory routine
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
#if !defined(__MEM_H__)
#define __MEM_H__

#if defined(_DEBUG)
#ifdef __cplusplus    /* When the user is Using C++,use C-linkage by this */
extern "C" {
#endif
extern int g_mem_log;
#define nbr_mem_alloc(s) _nbr_mem_alloc(s, __FILE__, __LINE__)
void	*_nbr_mem_alloc(size_t s, const char *file, int line);
#define nbr_mem_calloc(n, s) _nbr_mem_calloc(n, s, __FILE__, __LINE__)
void	*_nbr_mem_calloc(size_t n, size_t s, const char *file, int line);
void	nbr_mem_free(void *p);
void	nbr_mem_zero(void *p, size_t s);
void	nbr_mem_copy(void *dst, const void *src, size_t s);
int		nbr_mem_cmp(const void *dst, const void *src, size_t s);
void	*nbr_mem_move(void *dst, const void *src, size_t s);
#ifdef __cplusplus    /* When the user is Using C++,use C-linkage by this */
}
#endif
#else
#include <stdlib.h>
#include <memory.h>
#define nbr_mem_alloc	malloc
#define nbr_mem_calloc	calloc
#define nbr_mem_free	free
#define nbr_mem_zero	bzero
#define nbr_mem_copy	memcpy
#define nbr_mem_cmp		memcmp
#define nbr_mem_move	memmove
#endif

#endif
