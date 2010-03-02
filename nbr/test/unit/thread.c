/****************************************************************
 * thread.c : for testing core/thread.c
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
#include "osdep.h"
#include "tutil.h"
#include <assert.h>

static MUTEX g_mtx = NULL;

typedef struct {
	int idx;
	int param;
}	thread_param_t;

void *
prime_calc(void *p)
{
	thread_param_t *thp = (thread_param_t *)p;
	int	us;
	START_CLOCK(c1, c2);
	int prime = _prime( thp->param );
	STOP_CLOCK(c1, c2, us);
	nbr_mutex_lock(g_mtx);
	assert(thp->param >= 1000000);
	printf("rand = %u\n", nbr_rand32());
	printf("job %d: prime is %d for %d (takes %d us)\n", thp->idx, prime, thp->param, us);
	thp->param = 0;
	nbr_mutex_unlock(g_mtx);
	return NULL;
}

#define MAX_JOB 320

BOOL
nbr_thread_test()
{
	thread_param_t	base[MAX_JOB];
	int i = 0, j = 0, add = 0;
	THPOOL	tp = nbr_thpool_create(32);
	if (!tp) {
		TRACE("nbr_thread_test: create thread pool fail (%d)\n", nbr_err_get());
		return FALSE;
	}
	if (nbr_thpool_init_jobqueue(tp, 32000) < 0) {
		TRACE("nbr_thread_test: init job queue fail (%d)\n", nbr_err_get());
		return FALSE;
	}
	g_mtx = nbr_mutex_create();
	if (!g_mtx) {
		TRACE("nbr_thread_test: create mutex fail (%d)\n", nbr_err_get());
		return FALSE;
	}
	bzero(base, sizeof(base));
	while (1) {
		nbr_mutex_lock(g_mtx);
		base[i].idx = i + 1;
		if (base[i].param == 0) {
			base[i].param = (nbr_rand32() % 1000000) + 1000000;
			printf("job %d: will be start with %d\n", base[i].idx, base[i].param);
			add = 1;
		}
		else {
		//	printf("job %d: is still running\n", base[i].idx);
			add = 0;
		}
		nbr_mutex_unlock(g_mtx);
		if (add) {
			nbr_thpool_addjob(tp, &(base[i]), prime_calc);
		}
		i++;
		if (i >= MAX_JOB) {
			i = 0;
			j++;
			if (j > 20) {
				break;
			}
		}
	}
	TRACE("destroy all thread!\n");
	nbr_thpool_destroy(tp);
	return TRUE;
}
