/****************************************************************
 * array.c : for testing core/array.c
 * 2009/10/24 iyatomi : create
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
#include "tutil.h"


#define MAX_ELEM 10000000
#define MAX_ITER 3
#define USE_MT NBR_PRIM_THREADSAFE

BOOL
nbr_array_test()
{
	ARRAY a_ad[MAX_ITER + 3];
	int i, j, k, l, size = 10;
	int n_assign = 0;
	void *p, *pp;
	static void *a_p[MAX_ELEM];
	static unsigned char write_data[MAX_ELEM/1000][1000];

	//ary_init(10);

	bzero(a_ad, sizeof(a_ad));

	TRACE( "ary_create test...\n" );
	for (i = 0; i < MAX_ITER; i++) {
		a_ad[i] = nbr_array_create(MAX_ELEM/size, size, USE_MT);
		TRACE( "a_ad[%u]=%p\n", i, a_ad[i] );
		if (a_ad[i] < 0) {
			TRACE( "ary_test: ary_create fail!!: %u %u\n", MAX_ELEM/size, size );
			goto bad;
		}

		size *= 10;
	}

	a_ad[MAX_ITER] = nbr_array_create(1, 100, USE_MT);
	a_ad[MAX_ITER + 1] = nbr_array_create(2, 100, USE_MT);
	a_ad[MAX_ITER + 2] = nbr_array_create(100, 100, NBR_PRIM_EXPANDABLE);


	TRACE( "ary_alloc/free test...\n" );
	for (k = 0; k < 3; k++) {
		size = 10;
		for (i = 0; i < MAX_ITER; i++) {
			for (j = 0; j < MAX_ELEM/size; j++) {
				a_p[j] = nbr_array_alloc(a_ad[i]);
				if (j == ((int)(MAX_ELEM/size) - 1)) {
					TRACE( "a_p[%u]=%p\n", j, a_p[j] );
					if (nbr_array_alloc(a_ad[i])) {
						TRACE( "over alloc test failed(%d)\n", j+1 );
						goto bad;
					}
				}
				if (a_p[j] == NULL) {
					TRACE( "ary_test: ary_alloc fail: %u %u %u %u\n", size, i, j, k );
					goto bad;
				}

				if (size == 1000) {
					write_random_data(a_p[j], size);
					l = nbr_array_get_index(a_ad[i], a_p[j]);
					if (l >= 0 && l < (MAX_ELEM / size)) {
						memcpy(&(write_data[l][0]), a_p[j], size);
					}
					else {
						TRACE( "ary_test: index invalid %d\n", l);
						goto bad;
					}
				}
			}

			for (j = 0; j < MAX_ELEM/size; j++) {
				if (size == 1000) {
					p = nbr_array_get_from_index(a_ad[i], j);
					if (p && memcmp(p, &(write_data[j][0]), size) == 0) {

					}
					else {
						TRACE( "bufmem illegal(%d)\n", j );
						goto bad;
					}
				}
				if (nbr_array_free(a_ad[i], a_p[j]) < 0) {
					TRACE( "ary_test: ary_free fail: %u %u %u\n", i, j, k);
					goto bad;
				}
			}
			if (nbr_array_use(a_ad[i]) > 0) {
				TRACE("ary_test: cannot free all elements %d\n", i);
				goto bad;
			}
			//TRACE( "aryusage: %u/%u", nbr_array_use(a_ad[i]), nbr_array_max(a_ad[i]) );
			size *= 10;
		}
	}

	if ((a_p[0] = nbr_array_alloc(a_ad[MAX_ITER])) &&
		!(a_p[1] = nbr_array_alloc(a_ad[MAX_ITER]))) {
		k = 0;
		ARRAY_SCAN(a_ad[MAX_ITER], p) {
			k++;
		}

		if (k != 1) {
			goto bad;
		}

		nbr_array_free(a_ad[MAX_ITER], a_p[0]);
	}
	else {
		goto bad;
	}

	if ((a_p[0] = nbr_array_alloc(a_ad[MAX_ITER + 1])) &&
		(a_p[1] = nbr_array_alloc(a_ad[MAX_ITER + 1])) &&
		!(a_p[2] = nbr_array_alloc(a_ad[MAX_ITER + 1]))) {
		k = 0;
		ARRAY_SCAN(a_ad[MAX_ITER + 1], p) {
			k++;
		}

		if (k != 2) {
			goto bad;
		}

		nbr_array_free(a_ad[MAX_ITER + 1], a_p[0]);
		nbr_array_free(a_ad[MAX_ITER + 1], a_p[1]);
	}
	else {
		goto bad;
	}

	TRACE( "ary_alloc: expand test\n");
	for (i = 0; i < 200; i++) {
		if (!(a_p[i] = nbr_array_alloc(a_ad[MAX_ITER + 2]))) {
			TRACE( "ary_alloc: expand test cannot expand no more %d\n", i);
			goto bad;
		}
	}
	for (i = 0; i < 200; i++) {
		if (nbr_array_free(a_ad[MAX_ITER + 2], a_p[i]) < 0) {
			TRACE( "ary_alloc: expand test cannot free object %d\n", i);
			goto bad;
		}
	}
	for (i = 0; i < 200; i++) {
		if (!(a_p[i] = nbr_array_alloc(a_ad[MAX_ITER + 2]))) {
			TRACE( "ary_alloc: expand test cannot expand no more %d\n", i);
			goto bad;
		}
	}
	nbr_array_destroy(a_ad[MAX_ITER + 2]);

	TRACE( "ary_get_first/next test...\n" );

	for (i = 0; i < MAX_ITER; i++) {
		n_assign = nbr_rand32() % 900 + 100;
		for (j = 0; j < n_assign; j++) {
			p = nbr_array_alloc(a_ad[i]);
			k = nbr_array_get_index(a_ad[i], p);
			a_p[k] = p;
			//TRACE( "alloc %u, 0x%08x", j, a_p[j] );
		}
		j = 0;
		for (p = nbr_array_get_first(a_ad[i]); p;
			p = nbr_array_get_next(a_ad[i], p)) {
			//if (a_p[j] != p) {
				//TRACE( "data unmatch!! %u, %u, 0x%08x, 0x%08x, 0x%08x", i, j, p, a_p[j], nbr_array_get_last(a_ad[i]) );
			//}
			k = nbr_array_get_index(a_ad[i], p);
			if (a_p[k] != p) {
				TRACE("ary_get_first/next: pointer value differ %p %p\n", a_p[k], p);
				ASSERT(FALSE);
				goto bad;
			}
			j++;
		}
		if (j != n_assign) {
			TRACE( "ary_get_fist/next: invalid iteration. actual num differ %d %d\n",
					j, n_assign);
			ASSERT(FALSE);
			goto bad;
		}
		p = nbr_array_get_first(a_ad[i]);
		j = 0;
		while((pp = p)) {
			p = nbr_array_get_next(a_ad[i], p);
			if (nbr_array_free(a_ad[i], pp) < 0) {
				ASSERT(FALSE);
				goto bad;
			}
			j++;
		}
		if (j != n_assign) {
			TRACE( "ary_get_fist/next: invalid iteration with deletion. "
					"actual num differ %d %d\n",
					j, n_assign);
			ASSERT(FALSE);
			goto bad;
		}
		if (nbr_array_use(a_ad[i]) > 0) {
			TRACE("ary_test: cannot free all elements %d\n", i);
			goto bad;
		}
	}

	//ary_end();
	for (i = 0; i < MAX_ITER; i++) {
		nbr_array_destroy(a_ad[i]);
	}
	nbr_array_destroy(a_ad[MAX_ITER]);
	nbr_array_destroy(a_ad[MAX_ITER + 1]);

	TRACE("ary_unittest: success\n");
	return TRUE;

bad:
	//ary_end();
	nbr_err_out_stack();
	ASSERT(FALSE);
	return FALSE;
}
