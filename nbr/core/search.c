/****************************************************************
 * search.c
 * 2008/07/21 iyatomi : create
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
#include "str.h"
#include "search.h"
/* for creating memory hash, I use MurmurHash2 from
 * http://murmurhash.googlepages.com/
 * ( I modify a little to make it inline )*/
#include "murmur/MurmurHash2.cpp"



/*-------------------------------------------------------------*/
/* macro													   */
/*-------------------------------------------------------------*/
#define SEARCH_ERROUT			NBR_ERROUT
#define SEARCH_READ_LOCK(a,ret)	if (a->lock) { int r_; 							\
									if ((r_ = nbr_rwlock_rdlock(a->lock)) < 0) {\
										return ret;								\
									} 											\
								}
#define SEARCH_READ_UNLOCK(a)	if (a->lock) { nbr_rwlock_unlock(a->lock); }
#define SEARCH_WRITE_LOCK(a,ret)	if (a->lock) { int r_; 							\
									if ((r_ = nbr_rwlock_wrlock(a->lock)) < 0) {\
										return ret;								\
									}											\
								}
#define SEARCH_WRITE_UNLOCK(a)	SEARCH_READ_UNLOCK(a)



/*-------------------------------------------------------------*/
/* constant													   */
/*-------------------------------------------------------------*/
#define BIG_PRIME		(16754389)

enum	HUSH_KEY_TYPE {
	HKT_NONE = 0,
	HKT_INT,
	HKT_MINT,
	HKT_STR,
	HKT_MEM,
	HKT_MASK_TYPE  	= 0x0000FFFF,
	HKT_FLAG_EXPAND	= 0x40000000,
	HKT_FLAG_MTUSE 	= 0x80000000,
};



/*-------------------------------------------------------------*/
/* internal types											   */
/*-------------------------------------------------------------*/
typedef struct _hushelm {
	struct _hushelm	*next, *prev;
	void			*data;
	union	{
		struct { int k;		}	integer;
		struct { int k[0];	}	mint;
		struct { char k[0];	}	string;
		struct { char k[0]; } 	mem;
	}	key;
}	hushelm_t;

typedef struct _search {
	enum HUSH_KEY_TYPE	type;
	hushelm_t			**table;
	U32					size, elemsize;
	ARRAY				ad;	/* for allocating new hushelm when hush collision occured */
	RWLOCK				lock;
}	search_t;



/*-------------------------------------------------------------*/
/* static variable											   */
/*-------------------------------------------------------------*/
static 	ARRAY	*g_search = NULL;



/*-------------------------------------------------------------*/
/* internal method											   */
/*-------------------------------------------------------------*/
/* calc PJW hush */
NBR_INLINE unsigned int
_pjw_hush(int M, const unsigned char *t)
{
	unsigned int h = 0, g;
	for(;*t;++t) {
		h = (h << 4) + *t;
		if ((g = h&0xf0000000) != 0) {
			h ^= g >> 24;
			h ^= g;
		}
	}
	return h % M;
}

/* find max prime number that less than given integer */
/* 馬鹿正直なエラトステネスのふるいなので超おそい */
NBR_INLINE int
_prime(int given)
{
	int i, j;
	unsigned char *p;

	if (given <= 3) {
		return given;
	}

	/* this may cause problem if given is too huge */
	p = (unsigned char *)nbr_mem_alloc(given);
	if (!p) {
		TRACE( "_prime:cannot alloc work memory:size=%d", given );
		return -1;
	}
	nbr_mem_zero(p, given);
	for (i = 2; i <= given; i++) {
		if (i >= (int)(given/i)) {
//			TRACE("_prime:break at %u,(%u)", i, (int)(given/i));
			break;
		}
		if (p[i]) {
			continue;
		}
		else {
			for(j = i; j <= given; j += i) {
				p[j] = 1;
			}
		}
	}

	for (i = (given - 1); i > 2; i--) {
		if (p[i] == 0) {
			nbr_mem_free(p);
			TRACE("_prime:is %u\n", i);
			return i;
		}
	}

	// no prime number!? you're kidding!!
	ASSERT(FALSE);
	nbr_mem_free(p);
	return -1;
}

NBR_INLINE int
search_get_elem_size(enum HUSH_KEY_TYPE type, int size)
{
	int base = (sizeof(hushelm_t*) * 2) + sizeof(void *);
	switch(type) {
	case HKT_INT:
		return base + sizeof(int);
	case HKT_MINT:
		return base + sizeof(int) * size;
	case HKT_STR:
	case HKT_MEM:
		return base + (((0x03 + size) >> 2) << 2);	/* string aligns by 4 byte */
	default:
		ASSERT(FALSE);
		return 0;
	}
}


NBR_INLINE int
search_init(search_t *m, enum HUSH_KEY_TYPE type, int size, int param)
{
	int prime = _prime(size);
	if (prime < 0) {
		return -1;
	}
	m->size = prime;
	m->type = type;
	m->elemsize = search_get_elem_size(type, param);
	m->table = (hushelm_t **)nbr_mem_alloc(m->size * sizeof(hushelm_t*));
	if (!m->table) {
		return -2;
	}
	nbr_mem_zero(m->table, m->size * sizeof(hushelm_t*));
	return 0;
}

NBR_INLINE int
search_fin(search_t *m)
{
	if (m->table) {
		nbr_mem_free(m->table);
	}
	if (m->ad) {
		nbr_array_destroy(m->ad);
	}
	if (m->lock) {
		nbr_rwlock_destroy(m->lock);
	}
	nbr_mem_zero(m, sizeof(*m));
	return 0;
}

NBR_INLINE hushelm_t **
search_get_hushelm(search_t *m, int index)
{
	ASSERT(m && index < m->size);
	return (hushelm_t **)&(m->table[index]);
}

NBR_INLINE int
search_get_int_hush(search_t *m, int key)
{
	ASSERT(m && m->size > 0 && m->type == HKT_INT);
	return (key % m->size);
}

NBR_INLINE hushelm_t *
search_int_get(search_t *m, int key)
{
	hushelm_t *e;
	ASSERT(m->type == HKT_INT);
	e = *search_get_hushelm(m, search_get_int_hush(m, key));
	if (e == NULL) {
		return NULL;
	}
	while(e) {
		if (e->key.integer.k == key) {
			return e;
		}
		e = e->next;
	}
	return NULL;
}

NBR_INLINE int
search_get_str_hush(search_t *m, const char *str)
{
	ASSERT(m && m->type == HKT_STR);
	return _pjw_hush(m->size, (U8 *)str);
}

NBR_INLINE hushelm_t *
search_str_get(search_t *m, const char *key)
{
	hushelm_t *e;
	int es = m->elemsize;
	ASSERT(m->type == HKT_STR);
	e = *search_get_hushelm(m, search_get_str_hush(m, key));
	if (e == NULL) {
		return NULL;
	}
	while(e) {
		if (nbr_str_cmp(e->key.string.k, es, key, es) == 0) {
			return e;
		}
		e = e->next;
	}
	return NULL;
}

NBR_INLINE int
search_get_mem_hush(search_t *m, const char *mem, int len)
{
	ASSERT(m && (m->type == HKT_MEM || m->type == HKT_MINT));
	return MurmurHash2(mem, len, BIG_PRIME) % m->size;
}

NBR_INLINE hushelm_t *
search_mem_get(search_t *m, const char *key, int klen)
{
	hushelm_t *e;
	ASSERT(m->type == HKT_MEM);
	e = *search_get_hushelm(m, search_get_mem_hush(m, key, klen));
	if (e == NULL) {
		return NULL;
	}
	while(e) {
		if (nbr_mem_cmp(e->key.mem.k, key, klen) == 0) {
			return e;
		}
		e = e->next;
	}
	return NULL;
}

NBR_INLINE int
search_get_mint_hush(search_t *m, int key[], int n_key)
{
	ASSERT(m && m->type == HKT_MINT);
	/* todo: need improve (need to use all key value) */
	return ((key[0] % m->size) * (BIG_PRIME % m->size) + key[1] % m->size) % m->size;
}

NBR_INLINE hushelm_t *
search_mint_get(search_t *m, int key[], int n_key)
{
	hushelm_t *e;
	int c;
	ASSERT(m->type == HKT_MINT && n_key > 0);
	e = *search_get_hushelm(m, search_get_mint_hush(m, key, n_key));
next:
	if (e == NULL) {
		return NULL;
	}
	c = 0;
	while(c < n_key) {
		if (e->key.mint.k[c] != key[c]) {
			e = e->next;
			goto next;
		}
		c++;
	}
	return e;
}

NBR_INLINE int
search_get_opt_from(int primopt)
{
	int flag = 0;
	flag |= (primopt & NBR_PRIM_THREADSAFE) ? HKT_FLAG_MTUSE : 0;
	flag |= (primopt & NBR_PRIM_EXPANDABLE) ? HKT_FLAG_EXPAND : 0;
	return flag;
}




/*-------------------------------------------------------------*/
/* external methods											   */
/*-------------------------------------------------------------*/
int
nbr_search_init(int max)
{
	nbr_search_fin();

	if (!(g_search = nbr_array_create(max, sizeof(search_t), 1))) {
		SEARCH_ERROUT(ERROR,INTERNAL,"nbr_array_create: %d", max);
		return LASTERR;
	}
	return NBR_OK;
}

void
nbr_search_fin()
{
	search_t *s, *ps;
	if (g_search) {
		s = nbr_array_get_first(g_search);
		while((ps = s)) {
			s = nbr_array_get_next(g_search, s);
			nbr_search_destroy(ps);
		}
		nbr_array_destroy(g_search);
		g_search = NULL;
	}
	return;
}

NBR_INLINE SEARCH
nbr_search_init_common(int max, int hushsize, int type, int param)
{
	int ret;
	search_t *m = (search_t *)nbr_array_alloc(g_search);
	if (!m) {
		SEARCH_ERROUT(ERROR,EXPIRE,"array_alloc: %d", nbr_array_use(g_search));
		return NULL;
	}
	if ((ret = search_init(m, type & HKT_MASK_TYPE, hushsize, param)) < 0) {
		SEARCH_ERROUT(ERROR,INTERNAL,"search_init: %d", hushsize);
		nbr_array_free(g_search, m);
		return NULL;
	}
	if (!(m->ad = nbr_array_create(max, m->elemsize,
			(type & HKT_FLAG_EXPAND) ? NBR_PRIM_EXPANDABLE : 0))) {
		SEARCH_ERROUT(ERROR,INTERNAL,"nbr_array_create: max=%d", max);
		search_fin(m);
		nbr_array_free(g_search, m);
		return NULL;
	}
	if (type & HKT_FLAG_MTUSE) {
		if (!(m->lock = nbr_rwlock_create())) {
			SEARCH_ERROUT(ERROR,PTHREAD,"nbr_rwlock_create: max=%d", max);
			search_fin(m);
			nbr_array_free(g_search, m);
			return NULL;
		}
	}
	else { m->lock = NULL; }
	TRACE("nbr_search_init_common ad:%p,%p,%d\n", m, m->ad, nbr_array_get_size(m->ad));
	ASSERT(m->ad && m->table && m->size > 0);
	return m;
}

NBR_API SEARCH
nbr_search_init_int_engine(int max, int option, int hushsize)
{
	option = (HKT_INT | search_get_opt_from(option));
	return nbr_search_init_common(max, hushsize, option, 0);
}

NBR_API SEARCH
nbr_search_init_mint_engine(int max, int option, int hushsize, int num_key)
{
	option = (HKT_MINT | search_get_opt_from(option));
	return nbr_search_init_common(max, hushsize, option, num_key);
}

NBR_API SEARCH
nbr_search_init_str_engine(int max, int option, int hushsize, int length)
{
	option = (HKT_STR | search_get_opt_from(option));
	return nbr_search_init_common(max, hushsize, option, length);
}

NBR_API SEARCH
nbr_search_init_mem_engine(int max, int option, int hushsize, int length)
{
	option = (HKT_MEM | search_get_opt_from(option));
	return nbr_search_init_common(max, hushsize, option, length);
}

NBR_API int
nbr_search_destroy(SEARCH sd)
{
	ASSERT(sd);
	search_t *m = (search_t *)sd;
	if (nbr_array_is_used(g_search, m)) {
		ASSERT(m->ad && m->table && m->size > 0);
		search_fin(m);
		nbr_array_free(g_search, m);
		return NBR_OK;
	}
	SEARCH_ERROUT(INFO,ALREADY,"%p: already destroy", sd);
	return LASTERR;
}

#define SEARCH_REGISTER(keyset, keycmp, hushfunc)			\
	ASSERT(sd);												\
	search_t *m = (search_t *)sd;							\
	hushelm_t *tmp, **e;									\
	SEARCH_WRITE_LOCK(m,LASTERR);							\
	e = search_get_hushelm(m, hushfunc);					\
	tmp = *e;												\
	while (tmp) {											\
		if (keycmp) { break; }								\
		tmp = tmp->next;									\
	}														\
	if (tmp) { tmp->data = data; }							\
	else {													\
		tmp = nbr_array_alloc(m->ad);						\
		if (tmp) {											\
			tmp->next = NULL;								\
			tmp->prev = NULL;								\
			tmp->data = data;								\
			keyset;											\
		}													\
		else {												\
			SEARCH_ERROUT(ERROR,EXPIRE,"used: %d,%d",		\
				nbr_array_use(m->ad), nbr_array_max(m->ad));\
			SEARCH_WRITE_UNLOCK(m);							\
			return LASTERR;									\
		}													\
		ASSERT(tmp != *e);									\
		tmp->next = *e;										\
		if (*e) {											\
			(*e)->prev = tmp;								\
		}													\
		*e = tmp;											\
	}														\
	SEARCH_WRITE_UNLOCK(m);									\
	return NBR_OK;


#define SEARCH_UNREGISTER(keycmp, hushfunc)					\
	ASSERT(sd);												\
	search_t *m = (search_t *)sd;							\
	hushelm_t **pe, *e;										\
	int r;													\
	SEARCH_WRITE_LOCK(m,LASTERR);							\
	pe = search_get_hushelm(m, hushfunc);					\
	e = *pe;												\
	if (!e) {												\
		SEARCH_WRITE_UNLOCK(m);								\
		return NBR_OK;										\
	}														\
	while(e) {												\
		if (keycmp) { break; }								\
		e = e->next;										\
	}														\
	ASSERT(e);												\
	/* remove e from hush-collision chain */				\
	if (e->prev) {											\
		ASSERT(e->prev->next == e);							\
		e->prev->next = e->next;							\
	}														\
	else {													\
		*pe = e->next;										\
	}														\
	if (e->next) {											\
		ASSERT(e->next->prev == e);							\
		e->next->prev = e->prev;							\
	}														\
	/* free e into array m->ad */							\
	r = nbr_array_free(m->ad, e);							\
	SEARCH_WRITE_UNLOCK(m);									\
	return r;


NBR_API int
nbr_search_int_regist(SEARCH sd, int key, void *data)
{
	SEARCH_REGISTER(tmp->key.integer.k = key,
			tmp->key.integer.k == key,
			search_get_int_hush(m, key));
}

NBR_API int
nbr_search_int_unregist(SEARCH sd, int key)
{
	SEARCH_UNREGISTER(e->key.integer.k == key,
			search_get_int_hush(m, key));
}

NBR_API void *
nbr_search_int_get(SEARCH sd, int key)
{
	ASSERT(sd);
	void *p;
	search_t *m = (search_t *)sd;
	hushelm_t *e;
	SEARCH_READ_LOCK(m,NULL);
	e = search_int_get(m, key);
	p = e ? e->data : NULL;
	SEARCH_READ_UNLOCK(m);
	return p;
}

NBR_API int
nbr_search_str_regist(SEARCH sd, const char *key, void *data)
{
	int es = ((search_t *)sd)->elemsize;
	SEARCH_REGISTER(nbr_str_copy(tmp->key.string.k, es, key, es),
			nbr_str_cmp(tmp->key.string.k, es, key, es) == 0,
			search_get_str_hush(m, key));
}

NBR_API int
nbr_search_str_unregist(SEARCH sd, const char *key)
{
	int es = ((search_t *)sd)->elemsize;
	SEARCH_UNREGISTER(
			nbr_str_cmp(e->key.string.k, es, key, es) == 0,
			search_get_str_hush(m, key));
}

NBR_API void *
nbr_search_str_get(SEARCH sd, const char *key)
{
	ASSERT(sd);
	search_t *m = (search_t *)sd;
	hushelm_t *e;
	void *p;
	SEARCH_READ_LOCK(m,NULL);
	e = search_str_get(m, key);
	p = e ? e->data : NULL;
	SEARCH_READ_UNLOCK(m);
	return p;
}

NBR_API int
nbr_search_mem_regist(SEARCH sd, const char *key, int kl, void *data)
{
	SEARCH_REGISTER(nbr_mem_copy(tmp->key.mem.k, key, kl),
			nbr_mem_cmp(tmp->key.mem.k, key, kl) == 0,
			search_get_mem_hush(m, key, kl));
}

NBR_API int
nbr_search_mem_unregist(SEARCH sd, const char *key, int kl)
{
	SEARCH_UNREGISTER(
			nbr_mem_cmp(e->key.mem.k, key, kl) == 0,
			search_get_mem_hush(m, key, kl));
}

NBR_API void *
nbr_search_mem_get(SEARCH sd, const char *key, int kl)
{
	ASSERT(sd);
	search_t *m = (search_t *)sd;
	hushelm_t *e;
	void *p;
	SEARCH_READ_LOCK(m,NULL);
	e = search_mem_get(m, key, kl);
	p = e ? e->data : NULL;
	SEARCH_READ_UNLOCK(m);
	return p;
}

NBR_INLINE int
search_cmp_mint(int key1[], int key2[], int kl)
{
	while(--kl >= 0) { if (key1[kl] != key2[kl]) { return 0; } }
	return 1;
}

NBR_API int
nbr_search_mint_regist(SEARCH sd, int key[], int n_key, void *data)
{
	int i;
	SEARCH_REGISTER(i = 0; while(i < n_key) { tmp->key.mint.k[i] = key[i]; i++; },
			search_cmp_mint(tmp->key.mint.k, key, n_key),
			search_get_mint_hush(m, key, n_key));
}

NBR_API int
nbr_search_mint_unregist(SEARCH sd, int key[], int n_key)
{
	SEARCH_UNREGISTER(
			search_cmp_mint(e->key.mint.k, key, n_key),
			search_get_mint_hush(m, key, n_key));
}

NBR_API void *
nbr_search_mint_get(SEARCH sd, int key[], int n_key)
{
	ASSERT(sd);
	search_t *m = (search_t *)sd;
	hushelm_t *e;
	void *p;
	SEARCH_READ_LOCK(m,NULL);
	e = search_mint_get(m, key, n_key);
	p = e ? e->data : NULL;
	SEARCH_READ_UNLOCK(m);
	return p;
}
