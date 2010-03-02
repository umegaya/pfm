/***************************************************************
 * rand.h : mersenne twister
 *	implementation from
 *	http://www.math.sci.hiroshima-u.ac.jp/~m-mat/MT/SFMT/index-jp.html
 * 2008/07/23 iyatomi : create
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
#include "rand.h"
#include <time.h>
#define MEXP	19937	/* mersenne twister degree */
#include "sfmt/SFMT.c"



/*-------------------------------------------------------------*/
/* macro													   */
/*-------------------------------------------------------------*/
#define RAND_ERROUT		NBR_ERROUT



/*-------------------------------------------------------------*/
/* internal values											   */
/*-------------------------------------------------------------*/



/*-------------------------------------------------------------*/
/* external methods											   */
/*-------------------------------------------------------------*/
int
nbr_rand_init()
{
	//maybe now this routine is linux specific.
	union {
		struct {
			U8	hwaddr[6];
			U16	pid;
			U32	time;
		}	src;
		U32	data[3];
	}	seed;
	int r;

	if ((r = nbr_osdep_get_macaddr("eth0", seed.src.hwaddr)) != NBR_OK) {
		RAND_ERROUT(ERROR,INTERNAL,"get_macaddr: %d\n", r);
		//if no eth0, but continue
	}
	seed.src.pid = getpid();
	seed.src.time = time(NULL);
	init_by_array(seed.data, 3);

	return NBR_OK;
}

void
nbr_rand_fin()
{
}

U32
nbr_rand32()
{
	U32 r;
//	nbr_mutex_lock(g_rand_mtx);
	r = gen_rand32();
//	nbr_mutex_unlock(g_rand_mtx);
	return r;
}

U64
nbr_rand64()
{
	U64 r;
//	nbr_mutex_lock(g_rand_mtx);
	r = ((U64)gen_rand32() << 32) | gen_rand32();
//	nbr_mutex_unlock(g_rand_mtx);
	return r;
}

