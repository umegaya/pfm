/****************************************************************
 * main.c : test suite launcher
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

extern BOOL nbr_array_test();
extern BOOL nbr_str_test();
extern BOOL nbr_search_test();
extern BOOL nbr_thread_test();
extern BOOL nbr_sock_test();

#define RUNTEST(tname, routine, ret)								\
	if (!argv[1] ||													\
		nbr_str_cmp(#tname, sizeof(#tname), argv[1], 256) == 0 ||	\
		nbr_str_cmp("all", 4, argv[1], 256) == 0) {					\
		if (FALSE == routine) {										\
			TRACE("test "#tname" fail\n");							\
			return ret;												\
		}															\
	}

int main(int argc, char *argv[])
{
	RUNTEST(udp, nbr_sock_test(3, 1000, 10, 256000, "UDP"), -1);
	RUNTEST(tcp, nbr_sock_test(2, 1000, 10, 256000, "TCP"), -2);
	if (argv[2]) {
		RUNTEST(plugin, nbr_sock_test(3, 1000, 60, 128000, argv[2]), -100);
	}
	if (nbr_init(NULL) != NBR_OK) { return -3; }
	RUNTEST(thread, nbr_thread_test(), -4);
	RUNTEST(str, nbr_str_test(), -5);
	RUNTEST(array, nbr_array_test(), -6);
	RUNTEST(search, nbr_search_test(), -7);
	nbr_fin();
	return 0;
}
