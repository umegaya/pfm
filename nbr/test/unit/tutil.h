/****************************************************************
 * tutil.h : test utility routine
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
#if !defined(__TUTIL_H__)
#define __TUTIL_H__

#include <sys/time.h>	//for gettimeofday
#include <time.h>		//for time
#include <unistd.h>		//for usleep
#include <memory.h>		//for memcpy
#include <stdlib.h>		//for malloc, free

extern int _prime(int given);
extern void write_random_data(void *p, int size);
extern void write_random_string(void *p, int size);
extern char *toupper_string(const char *p, char *buf);

#define START_CLOCK(clock, clock2)	\
	struct timeval	clock, clock2;	\
	gettimeofday(&clock, NULL);

#define STOP_CLOCK(clock, clock2, us)						\
	gettimeofday(&clock2, NULL);							\
	us = (((S64)(clock2.tv_sec - clock.tv_sec) * 1000000)	\
			+ ((S64)(clock2.tv_usec - clock.tv_usec)));

#define TESTOUT(...)	\
	fprintf(stdout, __VA_ARGS__)

#endif/* __TUTIL_H__ */
