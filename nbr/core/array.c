/****************************************************************
 * array.c : fast fix size memory allocation
 * 2008/06/25 iyatomi : create
 * note:
 *	now allocate algorithm is not elegant and simple in the point
 *	of:
 *		1- array element expired
 *		2- maximum array size is 1
 *	for future, ary.c should be rewrited for solving such 2
 *	problems.
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
#include "array.h"



/*-------------------------------------------------------------*/
/* macro													   */
/*-------------------------------------------------------------*/
#define ARRAY_ERROUT			NBR_ERROUT
#define ARRAY_READ_LOCK(a,ret)	if (a->lock) { int r_; 							\
									if ((r_ = nbr_rwlock_rdlock(a->lock)) < 0) {\
										return ret; 								\
									} 											\
								}
#define ARRAY_READ_UNLOCK(a)	if (a->lock) { nbr_rwlock_unlock(a->lock); }
#define ARRAY_WRITE_LOCK(a,ret)	if (a->lock) { int r_; 							\
									if ((r_ = nbr_rwlock_wrlock(a->lock)) < 0) {\
										return ret; 								\
									}											\
								}
#define ARRAY_WRITE_UNLOCK(a)	ARRAY_READ_UNLOCK(a)



/*-------------------------------------------------------------*/
/* constant													   */
/*-------------------------------------------------------------*/
typedef enum eELEMFLAG {
	ELEM_USED			= 1 << 0,
	ELEM_FROM_HEAP		= 1 << 1,
} ELEMFLAG;



/*-------------------------------------------------------------*/
/* internal types											   */
/*-------------------------------------------------------------*/
typedef struct element
{
	struct element 	*prev;
	struct element 	*next;
	U32				flag;
	U8				data[0];
} element_t;

/* global ary info */
/* array data structure */
/* first->....->last->....->end->endptr(&g_end) */
/* ALLOC elem:
	first->..->prev->last->next->...->end->endptr
	first->..->prev->(prev->next (this pointer is allocated))
	->last->next (thus last ptr value will be last->next)
*/
/* FREE elem:
	first->..->elem->...->last->..->end->endptr
=> first->..->last->..->end->elem->endptr and end=elem
(elem is removed from chain and add as a last element)
*/
typedef struct array
{
	element_t	*used;	/* first fullfilled list element */
	element_t	*free;	/* first empty list element */
	U32			max;	/* max number of allocatable element */
	U32			use;	/* number of element in use */
	U32			size;	/* size of each allocated chunk */
	U32			option;	/* behavior option */
	RWLOCK		lock;	/* for multi thread use */
} array_t;



/*-------------------------------------------------------------*/
/* internal values											   */
/*-------------------------------------------------------------*/
static array_t			*g_array = NULL;



/*-------------------------------------------------------------*/
/* internal methods											   */
/*-------------------------------------------------------------*/
#if defined(_DEBUG)
static void
array_dump(const array_t *a)
{
	element_t *e;
	int i = 0;
	ASSERT(a);
	TRACE( "arydump: %u %u/%u\n", a->size, a->use, a->max);
	TRACE( "used = %p, free = %p\n", a->used, a->free );
	TRACE( "dump inside...\n" );
	e = a->used;
	while(e) {
		TRACE( "used[%u]: %p,%s (%p<->%p)\n",
			i, e, e->flag ? "use" : "empty", e->prev, e->next );
		i++;
		e = e->next;
	}
	i = 0;
	e = a->free;
	while(e) {
		TRACE( "free[%u]: %p,%s (%p<->%p)\n",
			i, e, e->flag ? "use" : "empty", e->prev, e->next );
		i++;
		e = e->next;
	}
}

static int
array_count_usenum(const array_t *a)
{
	/* count number of element inuse with 2 way */
	int c1 = 0, c2 = 0;
	element_t *e = a->used;
	while(e) {
		c1++;
		e = e->next;
	}
	e = a->free;
	while(e) {
		c2++;
		e = e->next;
	}
	/* if count differ, something strange must occur */
	if (!((a->max == (c1 + c2)) && (a->use == c1))) {
		TRACE( "illegal count: %u %u %u %u\n", a->max, a->use, c1, c2);
		//array_dump(a);
		ASSERT(FALSE);
	}
	return (a->max == (c1 + c2)) && (a->use == c1);
}
#else
#define array_dump(a)
#define array_count_usenum(a)
#endif
/* element_t related */
NBR_INLINE int
element_is_inuse(const element_t *e)
{
	return (e->flag & ELEM_USED);
}

NBR_INLINE void *
element_get_data(element_t *e)
{
	return (e->data);
}

NBR_INLINE void
element_set_flag(element_t *e, int on, U32 flag)
{
	if (on) { e->flag |= flag; }
	else 	{ e->flag &= ~(flag); }
}

NBR_INLINE int
element_get_flag(element_t *e, U32 flag)
{
	return (e->flag & flag);
}

NBR_INLINE int
element_get_size(size_t s)
{
	return (sizeof(element_t) + s);
}

/* array_t related */
NBR_INLINE int
array_get_alloc_size(int max, size_t s)
{
	return (sizeof(array_t) + (element_get_size(s) * max));
}

NBR_INLINE void *
array_get_top(const array_t *a)
{
	return (void *)(a + 1);
}

NBR_INLINE BOOL
array_check_align(const array_t *a, const element_t *e)
{
	ASSERT(a->size > 0);
	return ((
		(((U8*)e) - ((U8*)array_get_top(a)))
		%
		element_get_size(a->size)
		) == 0);
}

NBR_INLINE int
array_get_index(const array_t *a, const element_t *e)
{
	if (array_check_align(a, e)) {
		return (
			(((U8*)e) - ((U8*)array_get_top(a)))
			/
			element_get_size(a->size)
			);
	}
	return -1;
}

NBR_INLINE element_t *
array_get_from_index(const array_t *a, int index)
{
	if (a->max > index) {
		return ((element_t *)(array_get_top(a) + index * element_get_size(a->size)));
	}
	return NULL;
}

NBR_INLINE int
array_get_data_ofs()
{
	element_t e;
	return (int)(sizeof(e.prev) + sizeof(e.next) + sizeof(e.flag));
}

NBR_INLINE element_t *
array_get_top_address(const void *p)
{
	return (element_t *)(((U8*)p) - ((size_t)array_get_data_ofs()));
}

NBR_INLINE BOOL
array_check_address(const array_t *a, const element_t *e)
{
	int	idx = array_get_index(a, e);
	return idx >= 0 ? (idx < a->max) : FALSE;
}

NBR_INLINE void
array_set_data(const array_t *a, element_t *e, const void *data, int size)
{
	nbr_mem_copy(e->data, data, a->size > size ? size : a->size);
}

NBR_INLINE int
array_init_header(array_t *a, int max, size_t s, U32 option)
{
	int i;
	element_t *e, *ep;

	nbr_mem_zero(a, array_get_alloc_size(max, s));
	a->size = s;
	a->use = 0;
	a->max = max;
	a->option = option;
	a->used = NULL;
	a->free = (element_t *)array_get_top(a);
	ep = e = a->free;
	ASSERT(ep);
	ASSERT(!ep->prev);
	for (i = 1; i < max; i++) {
		e = array_get_from_index(a, i);
		ep->next = e;
		ep = e;
	}
	e->next = NULL;
	if (option & NBR_PRIM_THREADSAFE) {
		if (!(a->lock = nbr_rwlock_create())) {
			return LASTERR;
		}
	}
	else { a->lock = NULL; }
	array_count_usenum(a);
	return NBR_OK;
}

NBR_INLINE element_t *
array_alloc_elm(array_t *a)
{
	element_t *e;
	if (!a->free) {
		if (a->option & NBR_PRIM_EXPANDABLE) {
			if (!(e = nbr_mem_calloc(1, element_get_size(a->size)))) {
				return NULL;
			}
			element_set_flag(e, 1, ELEM_FROM_HEAP);
			TRACE("array: alloc from heap %p\n", e);
			a->max++;
		}
		else { return NULL; }
	}
	else {
		e = a->free;
		a->free = e->next;
		//TRACE("array: alloc from freepool: %p->%p\n", e, e->next);
	}
	e->prev = NULL;
	e->next = a->used;
	ASSERT(!(a->used) || !(a->used->prev));
	if (e->next) { e->next->prev = e; }
	a->used = e;
	return e;
}

NBR_INLINE void
array_free_elm(array_t *a, element_t *e)
{
	ASSERT(array_check_address(a,e) || element_get_flag(e, ELEM_FROM_HEAP));
	if (a->used == e) {		/* first used elem */
		ASSERT(!e->prev);
		ASSERT(!e->next || (e->next->prev == e));
		if (e->next) { e->next->prev = NULL; }
		a->used = e->next;
		ASSERT(!(a->used) || !(a->used->prev));
	}
	else if (!e->next) {	/* last used elem */
		ASSERT(e->prev);
		e->prev->next = NULL;
		ASSERT(!(a->used) || !(a->used->prev));
	}
	else {
		ASSERT(e->prev && e->next);
		e->prev->next = e->next;
		e->next->prev = e->prev;
		ASSERT(!(a->used) || !(a->used->prev));
	}
	if (element_get_flag(e, ELEM_FROM_HEAP)) {
		if ((a->max / 2) >= a->use) {
			nbr_mem_free(e);
			a->max--;
			return;
		}
	}
	e->prev = NULL;
	e->next = a->free;
	a->free = e;
	element_set_flag(e, 0, ELEM_USED);
	ASSERT(!(a->used) || !(a->used->prev));
}

NBR_INLINE void
array_destroy(array_t *a)
{
	element_t *e, *pe;
	if (a->lock) {
		nbr_rwlock_destroy(a->lock);
		a->lock = NULL;
	}
	a->use = 0;	/* force free heap elem in array_free_elm */
	e = a->used;
	while ((pe = e)) {
		e = e->next;
		if (element_get_flag(pe, ELEM_FROM_HEAP)) {
			nbr_mem_free(pe);
		}
	}
	e = a->free;
	while ((pe = e)) {
		ASSERT(element_get_flag(e, ELEM_FROM_HEAP) || array_check_align(a, e));
		e = e->next;
		if (element_get_flag(pe, ELEM_FROM_HEAP)) {
			nbr_mem_free(pe);
		}
	}
	nbr_mem_free(a);
}

#if defined(_DEBUG)
BOOL nbr_array_sanity_check(ARRAY ad)
{
	array_t *a = ad;
	element_t *e = a->used, *pe;
	while ((pe = e)) {
		if (!element_get_flag(e, ELEM_FROM_HEAP) && !array_check_align(a, e)) {
			return FALSE;
		}
		e = e->next;
	}
	e = a->free;
	while ((pe = e)) {
		if (!element_get_flag(e, ELEM_FROM_HEAP) && !array_check_align(a, e)) {
			return FALSE;
		}
		e = e->next;
	}
	return TRUE;
}
#endif


/*-------------------------------------------------------------*/
/* external methods											   */
/*-------------------------------------------------------------*/
int
nbr_array_init(int max)
{
	/* free every thing b4 initialize */
	nbr_array_fin();
	/* array manager itself also array */
	int n_size = array_get_alloc_size(max, sizeof(array_t*));
	if (!(g_array = (array_t *)nbr_mem_alloc(n_size))) {
		ARRAY_ERROUT(ERROR,MALLOC,"size=%u", n_size);
		return LASTERR;
	}
	return array_init_header(g_array, max, sizeof(array_t*), 0);
}

void
nbr_array_fin()
{
	array_t **a, **pa;
	/* not initialized */
	if (!g_array) {
		return;
	}
	/* free all allocated ary mem */
	a = nbr_array_get_first(g_array);
	while((pa = a)) {
		a = nbr_array_get_next(g_array, a);
		array_free_elm(g_array, array_get_top_address(pa));
		/* because this timing, all locks are already deleted */
		(*pa)->lock = NULL;
		array_destroy(*pa);
	}
	/* free arymgr */
	nbr_mem_free(g_array);
	g_array = NULL;
	return;
}

NBR_API ARRAY
nbr_array_create(int max, int size, int option)
{
	int n_size;
	array_t *a, **pa = nbr_array_alloc(g_array);
	if (pa == NULL) {
		ARRAY_ERROUT(ERROR,EXPIRE,"ROOT array:%p(%u/%u)",
			g_array, g_array->max, g_array->use);
		return NULL;
	}
	n_size = array_get_alloc_size(max, size);
	a = (array_t *)nbr_mem_alloc(n_size);
	if (a == NULL) {
		ARRAY_ERROUT(ERROR,MALLOC,"size:%u", n_size);
		nbr_array_free(g_array, pa);
		return NULL;
	}
	nbr_mem_zero(a, n_size);
	if (array_init_header(a, max, size, option) != NBR_OK) {
		ARRAY_ERROUT(ERROR,PTHREAD,"option:%08x", option);
		nbr_array_free(g_array, pa);
		return NULL;
	}
	*pa = a;
	return a;
}

NBR_API int
nbr_array_destroy(ARRAY ad)
{
	ASSERT(ad);
	element_t *e = g_array->used;
	array_t **a;
	/* free all allocated ary mem */
	while(e) {
		ASSERT(element_is_inuse(e));
		a = (array_t **)element_get_data(e);
		if (*a == ad) { break; }
		e = e->next;
	}
	if (!e) {
		ARRAY_ERROUT(ERROR,NOTFOUND,"not in array list: %p", ad);
		return LASTERR;
	}
	array_free_elm(g_array, e);
	array_destroy(*a);
	return NBR_OK;
}

NBR_API void *
nbr_array_alloc(ARRAY ad)
{
	ASSERT(ad);
	array_t *a = ad;
	element_t *e;
	ARRAY_WRITE_LOCK(a,NULL);
	e = array_alloc_elm(a);
	if (e) {
//		TRACE( "alloc: data=0x%08x\n", array_get_data(e) );
		ASSERT(element_get_data(e));
		element_set_flag(e, 1, ELEM_USED);
		a->use++;
		ASSERT(a->max >= a->use || array_count_usenum(a));
		ARRAY_WRITE_UNLOCK(a);
		return element_get_data(e);
	}
	ARRAY_WRITE_UNLOCK(a);
	return NULL;
}

NBR_API int
nbr_array_free(ARRAY ad, void *p)
{
	ASSERT(ad);
	array_t *a = ad;
	element_t *e = array_get_top_address(p);
	//TRACE( "free: p=0x%08x, 0x%08x, %s, top=0x%08x, elm=0x%08x\n", p, e,
	//		element_is_inuse(e) ? "use" : "empty", array_get_top(a), array_get_top_address(p));
	//, array_get_elm_size(a->size), sizeof(element_t), sizeof(a->first->data) );
	if (!element_is_inuse(e)) {
		ARRAY_ERROUT(ERROR,INVPTR,"not used:%p/%p", ad, p);
		//array_dump(a);
		ASSERT(FALSE);
		return LASTERR;
	}
	if (element_get_flag(e, ELEM_FROM_HEAP) || array_check_address(a, e)) {
		ARRAY_WRITE_LOCK(a,LASTERR);
		array_free_elm(a, e);
		a->use--;
		ASSERT((a->use >= 0 && a->max >= a->use) || array_count_usenum(a));
		ARRAY_WRITE_UNLOCK(a);
		return NBR_OK;
	}
	ARRAY_ERROUT(ERROR,INVPTR,"align:%p/%p", ad, p);
	array_dump(a);
	ASSERT(FALSE);
	return LASTERR;
}


NBR_API void *
nbr_array_get_first(ARRAY ad)
{
	ASSERT(ad);
	array_t *a = ad;
	void *p;
	ARRAY_READ_LOCK(a,NULL);
	ASSERT(!a->used || (a->used && !a->used->prev));
	p = a->used ? element_get_data(a->used) : NULL;
	ARRAY_READ_UNLOCK(a);
	return p;
}

NBR_API void *
nbr_array_get_next(ARRAY ad, void *p)
{
	ASSERT(ad);
	array_t *a = ad;
	void *ptr;
	element_t *e;
	ARRAY_READ_LOCK(a,NULL);
	e = array_get_top_address(p);
	ptr = e->next ? element_get_data(e->next) : NULL;
	ARRAY_READ_UNLOCK(a);
	return ptr;
}

NBR_API int
nbr_array_get_index(ARRAY ad, void *p)
{
	ASSERT(ad);
	array_t *a = ad;
	element_t *e;
	if (a->option & NBR_PRIM_EXPANDABLE) { return NBR_EINVAL; }
	e = array_get_top_address(p);
	return array_get_index(a, e);
}

NBR_API void *
nbr_array_get_from_index(ARRAY ad, int index)
{
	ASSERT(ad);
	array_t *a = ad;
	element_t *e;
	if (a->option & NBR_PRIM_EXPANDABLE) { return NULL; }
	if (!(e = array_get_from_index(a, index))) {
		ARRAY_ERROUT(ERROR,RANGE,"index over:%p/%u/%u",
			ad, index, a->max);
		return NULL;
	}
	return element_get_data(e);
}

NBR_API void *
nbr_array_get_from_index_if_used(ARRAY ad, int index)
{
	ASSERT(ad);
	array_t *a = ad;
	element_t *e;
	if (a->option & NBR_PRIM_EXPANDABLE) { return NULL; }
	if (!(e = array_get_from_index(a, index))) {
		ARRAY_ERROUT(ERROR,RANGE,"index over:%p/%u/%u",
			ad, index, a->max);
		return NULL;
	}
	if (!element_is_inuse(e)) {
		return NULL;
	}
	return element_get_data(e);
}

NBR_API int
nbr_array_is_used(ARRAY ad, void *p)
{
	ASSERT(ad);
	array_t *a = ad;
	element_t *e = array_get_top_address(p);
	if (array_check_address(a, array_get_top_address(p))) {
		return element_is_inuse(e);
	}
	ARRAY_ERROUT(ERROR,INVPTR,"align:%p/%p", ad, p);
	return 0;
}

NBR_API int
nbr_array_max(ARRAY ad)
{
	ASSERT(ad);
	array_t *a = ad;
	return a->max;
}

NBR_API int
nbr_array_use(ARRAY ad)
{
	ASSERT(ad);
	array_t *a = ad;
#if defined(_DEBUG)
	ARRAY_READ_LOCK(a,0);
	if (!array_count_usenum(a)) {
		ASSERT(0);
	}
	ARRAY_READ_UNLOCK(a);
#endif
	return a->use;
}

NBR_API int
nbr_array_full(ARRAY ad)
{
	ASSERT(ad);
	array_t *a = ad;
	return (!(a->option & NBR_PRIM_EXPANDABLE)) && (a->max <= a->use);
}

NBR_API int
nbr_array_get_size(ARRAY ad)
{
	ASSERT(ad);
	array_t *a = ad;
	return a->size;
}


