/****************************************************************
 * search.c : for testing core/search.c
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
#include "tcutil.h"

#define nbr_search_base(name,init,ksz,reg,find,unreg) {				\
	int ret, i;														\
	SEARCH sd;														\
	int MAX_ENTRY = 5000000 + (1 + (nbr_rand32() % 5)) * 1000000;	\
	int hush_TBL_RATIO = nbr_rand32() % 4 + 1;						\
	int bucket_num = (int)(MAX_ENTRY / hush_TBL_RATIO);				\
	unsigned int *buffer;											\
	unsigned int *s;												\
	unsigned int tot_us = 0;										\
	TESTOUT(name" test start: entry=%u,bucket=%u\n", MAX_ENTRY, bucket_num);		\
	if (!(buffer = (unsigned int*)malloc(MAX_ENTRY * sizeof(unsigned int)))) {		\
		TESTOUT("cannot allocate memory size=%u", MAX_ENTRY * sizeof(unsigned int));\
	}																				\
	if ((sd = init(MAX_ENTRY, bucket_num, ksz)) < 0) {				\
		TESTOUT( #init" fail(%p)\n", sd );							\
		goto error;													\
	}																\
	START_CLOCK(tv_reg_before, tv_reg_after);						\
	for (i = 0; i < MAX_ENTRY; i++) {								\
		buffer[i] = i;												\
		if ((ret = reg(sd, i, &(buffer[i]),ksz)) < 0) {				\
			TESTOUT( #reg"(%p,%d),%d %d fail\n", sd, i, ret, ksz );	\
			goto error;												\
		}															\
	}																\
	STOP_CLOCK(tv_reg_before, tv_reg_after, tot_us);				\
	TESTOUT("int average regist time... %f usec TOTAL(%u), ENTRY(%u), TBL(%u)\n",	\
		((float)((float)tot_us/(float)MAX_ENTRY)),									\
		tot_us, MAX_ENTRY, bucket_num));											\
	if (nbr_search_int_regist(sd, i, &i) >= 0) {					\
		TESTOUT( "int over registration success\n" );				\
		goto error;													\
	}																\
	START_CLOCK(tv_before, tv_after);								\
	for (i = 0; i < MAX_ENTRY; i++) {								\
		if (!(s = (unsigned int *)find(sd, i))) {					\
			TESTOUT( #find"(%d) fail\n", i );						\
			return FALSE;											\
		}															\
		else if (*s == i && s == &(buffer[i])) { continue; }		\
		else { TESTOUT( "wrong "name" search(%d,%d)\n", i, *s );	\
			goto error;												\
		}															\
	}																\
	STOP_CLOCK(tv_before, tv_after, tot_us);						\
	TESTOUT("int average search time... %f usec TOTAL(%u), ENTRY(%u), TBL(%u)\n",	\
		((float)((float)tot_us/(float)MAX_ENTRY)), tot_us, MAX_ENTRY,				\
		(int)(MAX_ENTRY/hush_TBL_RATIO));											\
	for (i = 0; i < MAX_ENTRY; i++) {												\
		if ((ret = unreg(sd, i)) < 0) {							\
			TESTOUT( #unreg" fail(%d,%d)\n", i, ret );			\
			goto error;												\
		}															\
	}																\
	nbr_search_destroy(sd);											\
	free(buffer);													\
	return TRUE;													\
error:																\
	nbr_search_destroy(sd);											\
	free(buffer);													\
	return FALSE;													}


static int g_use_mt = 0;

static BOOL
nbr_search_int_test()
{
	int ret, i, key;
	SEARCH sd;
	int MAX_ENTRY = 5000000 + (1 + (nbr_rand32() % 5)) * 1000000;
	int hush_TBL_RATIO = nbr_rand32() % 4 + 1;
	int bucket_num = (int)(MAX_ENTRY / hush_TBL_RATIO);
	unsigned int *buffer;
	unsigned int *s;
	unsigned int tot_us = 0;

	TESTOUT("int test start: entry=%u,bucket=%u\n", MAX_ENTRY, bucket_num);
	if (!(buffer = (unsigned int*)malloc(MAX_ENTRY * sizeof(unsigned int)))) {
		TESTOUT("cannot allocate memory size=%u", (U32)MAX_ENTRY * sizeof(unsigned int));
	}
	if ((sd = nbr_search_init_int_engine(MAX_ENTRY, g_use_mt, bucket_num)) < 0) {
		TESTOUT( "nbr_search_init_int_engine fail(%p)\n", sd );
		goto error;
	}
	START_CLOCK(tv_reg_before, tv_reg_after);
	for (i = 0; i < MAX_ENTRY; i++) {
		buffer[i] = i;
		if ((ret = nbr_search_int_regist(sd, i, &(buffer[i]))) < 0) {
			TESTOUT( "nbr_search_int_regist(%p,%d),%d fail\n", sd, i, ret );
			goto error;
		}
	}
	STOP_CLOCK(tv_reg_before, tv_reg_after, tot_us);
	TESTOUT("int average regist time... %f usec TOTAL(%u), ENTRY(%u), TBL(%u)\n",
		((float)((float)tot_us/(float)MAX_ENTRY)),
		tot_us, MAX_ENTRY, bucket_num);


	if (nbr_search_int_regist(sd, i, &i) >= 0) {
		TESTOUT( "error : int over registration success (%d)\n", i );
		goto error;
	}

	START_CLOCK(tv_before, tv_after);
	for (i = 0; i < MAX_ENTRY; i++) {
		key = (i - 1);
		key++;
		s = (unsigned int *)nbr_search_int_get(sd, key);

		if (!s) {
			TESTOUT( "nbr_search_int_get(%d) fail\n", i );
			return FALSE;
		}
		else {
			if (*s == i && s == &(buffer[i])) {
				continue;
			}
			else {
				TESTOUT( "wrong int search(%d,%d)\n", i, *s );
				goto error;
			}
		}
	}
	STOP_CLOCK(tv_before, tv_after, tot_us);
	TESTOUT("int average search time... %f usec TOTAL(%u), ENTRY(%u), TBL(%u)\n",
		((float)((float)tot_us/(float)MAX_ENTRY)), tot_us, MAX_ENTRY, bucket_num);

	for (i = 0; i < MAX_ENTRY; i++) {
		if ((ret = nbr_search_int_unregist(sd, i)) < 0) {
			TESTOUT( "nbr_search_int_unregist fail(%d,%d)\n", i, ret );
			goto error;
		}
	}

	nbr_search_destroy(sd);
	free(buffer);

	return TRUE;
error:
	nbr_search_destroy(sd);
	free(buffer);
	return FALSE;
}

static BOOL
nbr_search_mint_test()
{
	int ret, i;
	SEARCH sd;
	int MAX_ENTRY = 5000000 + (1 + (nbr_rand32() % 5)) * 1000000;
	int hush_TBL_RATIO = nbr_rand32() % 4 + 1;
	unsigned int *buffer;
	unsigned int *s;
	int mintkey[4];
	unsigned int tot_us = 0;

	TESTOUT("mint test start******(%u,%u)\n", MAX_ENTRY, hush_TBL_RATIO);

	buffer = (unsigned int*)malloc(MAX_ENTRY * sizeof(unsigned int));

	if ((sd = nbr_search_init_mint_engine(MAX_ENTRY,g_use_mt,
		(int)(MAX_ENTRY / hush_TBL_RATIO), 4)) < 0) {
		TESTOUT( "nbr_search_init_mint_engine fail(%p)\n", sd );
		goto error;
	}

	START_CLOCK(tv_reg_before, tv_reg_after);
	for (i = 0; i < MAX_ENTRY; i++) {
		buffer[i] = i;
		mintkey[0] = i;
		mintkey[1] = i + 100;
		mintkey[2] = i + 200;
		mintkey[3] = i + 300;
		if ((ret = nbr_search_mint_regist(sd, mintkey, 4, &(buffer[i]))) < 0) {
			TESTOUT( "nbr_search_mint_regist(%p,%d),%d fail\n", sd, i, ret );
			ASSERT(FALSE);
			goto error;
		}
	}
	STOP_CLOCK(tv_reg_before, tv_reg_after, tot_us);
	TESTOUT("mint average regist time... %f usec TOTAL(%u), ENTRY(%u), TBL(%u)\n",
		((float)((float)tot_us/(float)MAX_ENTRY)),
		tot_us, MAX_ENTRY, (int)(MAX_ENTRY/hush_TBL_RATIO));


	mintkey[0] = i;
	mintkey[1] = i + 100;
	mintkey[2] = i + 200;
	mintkey[3] = i + 300;
	if (nbr_search_mint_regist(sd, mintkey, 4, &i) >= 0) {
		TESTOUT( "error: mint over registration success (%d)\n", i );
		goto error;
	}

	START_CLOCK(tv_before, tv_after);
	for (i = 0; i < MAX_ENTRY; i++) {
		mintkey[0] = i;
		mintkey[1] = i + 100;
		mintkey[2] = i + 200;
		mintkey[3] = i + 300;
		s = (unsigned int *)nbr_search_mint_get(sd, mintkey, 4);

		if (!s) {
			TESTOUT( "nbr_search_mint_get(%d) fail\n", i );
			goto error;
		}
		else {
			if (*s == i && s == &(buffer[i])) {
				continue;
			}
			else {
				TESTOUT( "wrong search(%d,%d)\n", i, *s );
				goto error;
			}
		}
	}
	STOP_CLOCK(tv_before, tv_after, tot_us);
	TESTOUT("mint average regist time... %f usec TOTAL(%u), ENTRY(%u), TBL(%u)\n",
		((float)((float)tot_us/(float)MAX_ENTRY)),
		tot_us, MAX_ENTRY, (int)(MAX_ENTRY/hush_TBL_RATIO));

	for (i = 0; i < MAX_ENTRY; i++) {
		mintkey[0] = i;
		mintkey[1] = i + 100;
		mintkey[2] = i + 200;
		mintkey[3] = i + 300;
		if ((ret = nbr_search_mint_unregist(sd, mintkey, 4)) < 0) {
			TESTOUT( "nbr_search_mint_unregist fail(%d,%d)\n", i, ret );
			goto error;
		}
	}

	nbr_search_destroy(sd);
	free(buffer);

	return TRUE;
error:
	nbr_search_destroy(sd);
	free(buffer);
	return FALSE;
}

static BOOL
nbr_search_str_test()
{
	int ret, i;
	char strbuf[33];
	SEARCH sd;
	int MAX_ENTRY = 5000000 + (1 + (nbr_rand32() % 5)) * 1000000;
	int hush_TBL_RATIO = nbr_rand32() % 4 + 1;
	unsigned int *buffer;
	unsigned int *s;
	unsigned int tot_us = 0;

	TESTOUT("str test start******(%u,%u)\n", MAX_ENTRY, hush_TBL_RATIO);

	buffer = (unsigned int*)malloc(MAX_ENTRY * sizeof(unsigned int));

	if ((sd = nbr_search_init_str_engine(MAX_ENTRY,g_use_mt,
		(int)(MAX_ENTRY / hush_TBL_RATIO), 32)) < 0) {
		TESTOUT( "nbr_search_init_str_engine fail(%p)\n", sd );
		goto error;
	}

	START_CLOCK(tv_reg_before, tv_reg_after);
	for (i = 0; i < MAX_ENTRY; i++) {
		buffer[i] = i;
		nbr_str_printf(strbuf, sizeof(strbuf), "sbuf%07u", i);
		if ((ret = nbr_search_str_regist(sd, strbuf, &(buffer[i]))) < 0) {
			TESTOUT( "nbr_search_str_regist(%p,%d),%d fail\n", sd, i, ret );
			ASSERT(FALSE);
			goto error;
		}
	}
	STOP_CLOCK(tv_reg_before, tv_reg_after, tot_us);
	TESTOUT("str average regist time... %f usec TOTAL(%u), ENTRY(%u), TBL(%u)\n",
		((float)((float)tot_us/(float)MAX_ENTRY)),
		tot_us, MAX_ENTRY, (int)(MAX_ENTRY/hush_TBL_RATIO));


	nbr_str_printf(strbuf, sizeof(strbuf), "sbuf%07u", i);
	if (nbr_search_str_regist(sd, strbuf, &i) >= 0) {
		TESTOUT( "error : str over registration success (%d)\n", i );
		goto error;
	}

	START_CLOCK(tv_before, tv_after);
	for (i = 0; i < MAX_ENTRY; i++) {
		nbr_str_printf(strbuf, sizeof(strbuf), "sbuf%07u", i);
		s = (unsigned int *)nbr_search_str_get(sd, strbuf);
		gettimeofday(&tv_after, NULL);

		if (!s) {
			TESTOUT( "nbr_search_str_get(%d) fail\n", i );
			goto error;
		}
		else {
			if (*s == i && s == &(buffer[i])) {
				continue;
			}
			else {
				TESTOUT( "wrong search(%d,%d)\n", i, *s );
				goto error;
			}
		}
	}
	STOP_CLOCK(tv_before, tv_after, tot_us);
	TESTOUT("str average search time... %f usec TOTAL(%u), ENTRY(%u), TBL(%u)\n",
		((float)((float)tot_us/(float)MAX_ENTRY)),
		tot_us, MAX_ENTRY, (int)(MAX_ENTRY/hush_TBL_RATIO));

	for (i = 0; i < MAX_ENTRY; i++) {
		nbr_str_printf(strbuf, sizeof(strbuf), "sbuf%07u", i);
		if ((ret = nbr_search_str_unregist(sd, strbuf)) < 0) {
			TESTOUT( "nbr_search_str_unregist fail(%d,%d)\n", i, ret );
			goto error;
		}
	}

	nbr_search_destroy(sd);
	free(buffer);

	return TRUE;
error:
	nbr_search_destroy(sd);
	free(buffer);
	return FALSE;
}

static BOOL
nbr_search_mem_test(int keylen)
{
	int ret, i, j;
	char strbuf[33], *work;
	SEARCH sd;
	int MAX_ENTRY = 5000000 + (1 + (nbr_rand32() % 5)) * 1000000;
	int hush_TBL_RATIO = 1;//nbr_rand32() % 4 + 1;
	unsigned int *buffer;
	unsigned int *s;
	unsigned int tot_us = 0;

	TESTOUT("mem test start******(%u,%u)\n", MAX_ENTRY, hush_TBL_RATIO);

	buffer = (unsigned int*)malloc(MAX_ENTRY * sizeof(unsigned int));

	if ((sd = nbr_search_init_mem_engine(MAX_ENTRY,g_use_mt,
		(int)(MAX_ENTRY / hush_TBL_RATIO), keylen)) < 0) {
		TESTOUT( "nbr_search_init_mem_engine fail(%p)\n", sd );
		goto error;
	}

	START_CLOCK(tv_reg_before, tv_reg_after);
	for (i = 0; i < MAX_ENTRY; i++) {
		buffer[i] = i;
		work = strbuf;
		for (j = 0; j < (keylen >> 2); j++) {
			memcpy(work, &i, sizeof(i));
			work += sizeof(i);
		}
		for (j = 0; j < (keylen % 4); j++) {
			*work++ = '0';
		}
		if ((ret = nbr_search_mem_regist(sd, strbuf, keylen, &(buffer[i]))) < 0) {
			TESTOUT( "nbr_search_mem_regist(%p,%d),%d fail\n", sd, i, ret );
			ASSERT(FALSE);
			goto error;
		}
	}
	STOP_CLOCK(tv_reg_before, tv_reg_after, tot_us);
	TESTOUT("mem average regist time... %f usec TOTAL(%u), ENTRY(%u), TBL(%u)\n",
		((float)((float)tot_us/(float)MAX_ENTRY)),
		tot_us, MAX_ENTRY, (int)(MAX_ENTRY/hush_TBL_RATIO));


	work = strbuf;
	for (j = 0; j < (keylen >> 2); j++) {
		memcpy(work, &i, sizeof(i));
		work += sizeof(i);
	}
	for (j = 0; j < (keylen % 4); j++) {
		*work++ = '0';
	}
	if (nbr_search_mem_regist(sd, strbuf, keylen, &i) >= 0) {
		TESTOUT( "mem over registration success (%d)\n", i );
		goto error;
	}

	START_CLOCK(tv_before, tv_after);
	for (i = 0; i < MAX_ENTRY; i++) {
		work = strbuf;
		for (j = 0; j < (keylen >> 2); j++) {
			memcpy(work, &i, sizeof(i));
			work += sizeof(i);
		}
		for (j = 0; j < (keylen % 4); j++) {
			*work++ = '0';
		}
		s = (unsigned int *)nbr_search_mem_get(sd, strbuf, keylen);

		if (!s) {
			TESTOUT( "nbr_search_mem_get(%d) fail\n", i );
			goto error;
		}
		else {
			if (*s == i && s == &(buffer[i])) {
				continue;
			}
			else {
				TESTOUT( "wrong search(%d,%d)\n", i, *s );
				goto error;
			}
		}
	}
	STOP_CLOCK(tv_before, tv_after, tot_us);
	TESTOUT("mem average search time... %f usec TOTAL(%u), ENTRY(%u), TBL(%u)\n",
		((float)((float)tot_us/(float)MAX_ENTRY)),
		tot_us, MAX_ENTRY, (int)(MAX_ENTRY/hush_TBL_RATIO));

	for (i = 0; i < MAX_ENTRY; i++) {
		work = strbuf;
		for (j = 0; j < (keylen >> 2); j++) {
			memcpy(work, &i, sizeof(i));
			work += sizeof(i);
		}
		for (j = 0; j < (keylen % 4); j++) {
			*work++ = '0';
		}
		if ((ret = nbr_search_mem_unregist(sd, strbuf, keylen)) < 0) {
			TESTOUT( "nbr_search_mem_unregist fail(%d,%d)\n", i, ret );
			goto error;
		}
	}

	nbr_search_destroy(sd);
	free(buffer);

	return TRUE;
error:
	nbr_search_destroy(sd);
	free(buffer);
	return FALSE;
}

BOOL
tctest(int keylen)
{
	int i, j;
	char strbuf[33], *work;
	TCMAP *sd;
	int MAX_ENTRY = 5000000 + (1 + (nbr_rand32() % 5)) * 1000000;
	int hush_TBL_RATIO = 1;//nbr_rand32() % 4 + 1;
	unsigned int *buffer;
	unsigned int *s, **ps;
	int vlen = 0;
	unsigned int tot_us = 0;

	TESTOUT("tcm test start (as ref)******(%u,%u)\n", MAX_ENTRY, hush_TBL_RATIO);

	buffer = (unsigned int*)malloc(MAX_ENTRY * sizeof(unsigned int));

	if (!(sd = tcmapnew2(MAX_ENTRY))) {
		TESTOUT( "tcmapnew2 fail(%p)\n", sd );
		goto error;
	}

	START_CLOCK(tv_reg_before, tv_reg_after);
	for (i = 0; i < MAX_ENTRY; i++) {
		buffer[i] = i;
		work = strbuf;
		for (j = 0; j < (keylen >> 2); j++) {
			memcpy(work, &i, sizeof(i));
			work += sizeof(i);
		}
		for (j = 0; j < (keylen % 4); j++) {
			*work++ = '0';
		}
		s = &(buffer[i]);
		tcmapput(sd, strbuf, keylen, &s, sizeof(unsigned int *));
	}
	STOP_CLOCK(tv_reg_before, tv_reg_after, tot_us);
	TESTOUT("tcm average regist time... %f usec TOTAL(%u), ENTRY(%u), TBL(%u)\n",
		((float)((float)tot_us/(float)MAX_ENTRY)),
		tot_us, MAX_ENTRY, (int)(MAX_ENTRY/hush_TBL_RATIO));

	START_CLOCK(tv_before, tv_after);
	for (i = 0; i < MAX_ENTRY; i++) {
		work = strbuf;
		for (j = 0; j < (keylen >> 2); j++) {
			memcpy(work, &i, sizeof(i));
			work += sizeof(i);
		}
		for (j = 0; j < (keylen % 4); j++) {
			*work++ = '0';
		}
		ps = (unsigned int **)tcmapget(sd, strbuf, keylen, &vlen);

		if (!ps) {
			TESTOUT( "nbr_search_mem_get(%d) fail\n", i );
			goto error;
		}
		else {
			s = *ps;
			if (*s == i && s == &(buffer[i])) {
				continue;
			}
			else {
				TESTOUT( "wrong search(%d,%d)\n", i, *s );
				goto error;
			}
		}
	}
	STOP_CLOCK(tv_before, tv_after, tot_us);
	TESTOUT("tcm average search time... %f usec TOTAL(%u), ENTRY(%u), TBL(%u)\n",
		((float)((float)tot_us/(float)MAX_ENTRY)),
		tot_us, MAX_ENTRY, (int)(MAX_ENTRY/hush_TBL_RATIO));

	for (i = 0; i < MAX_ENTRY; i++) {
		work = strbuf;
		for (j = 0; j < (keylen >> 2); j++) {
			memcpy(work, &i, sizeof(i));
			work += sizeof(i);
		}
		for (j = 0; j < (keylen % 4); j++) {
			*work++ = '0';
		}
		if (!tcmapout(sd, strbuf, keylen)) {
			TESTOUT( "tcmapout fail(%d)\n", i );
			goto error;
		}
	}

	tcmapdel(sd);
	free(buffer);

	return TRUE;
error:
	tcmapdel(sd);
	free(buffer);
	return FALSE;
}

BOOL
nbr_search_test()
{
	if (FALSE == nbr_search_mem_test(32)) {
		TESTOUT("nbr_search_mem_test fail(%d)\n", nbr_err_get());
		return FALSE;
	}
	if (FALSE == nbr_search_int_test()) {
		TESTOUT("nbr_search_int_test fail(%d)\n", nbr_err_get());
		return FALSE;
	}
	if (FALSE == nbr_search_mint_test()) {
		TESTOUT("nbr_search_mint_test fail(%d)\n", nbr_err_get());
		return FALSE;
	}
	if (FALSE == nbr_search_str_test()) {
		TESTOUT("nbr_search_str_test fail(%d)\n", nbr_err_get());
		return FALSE;
	}
	if (FALSE == tctest(32)) {
		TESTOUT("tctest fail (%d)\n", nbr_err_get());
		return FALSE;
	}
	return TRUE;
}

