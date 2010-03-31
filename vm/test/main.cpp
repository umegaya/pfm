/****************************************************************
 * main.cpp : entry point of sfc::vm test suite
 * 2010/02/15 iyatomi create
 *                             Copyright (C) 2008-2010 Takehiro Iyatomi
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
#include "sfc.hpp"
#include "vmd.h"

using namespace sfc;
using namespace sfc::vm;

enum {
	test_flag_client_only = 0x00000001,
};

int
test(int argc, char *argv[], U32 &flag)
{
	flag = 0;
	int n_cli = 10;
	bool found = false;
	for (int i = 0; i < argc; i++) {
		if (memcmp("--test", argv[i], 6) == 0) {
			if (*(argv[i] + 6)) {
				SAFETY_ATOI(argv[i] + 6, n_cli, int);
			}
			found = true;
			continue;
		}
		if (memcmp("--client-only", argv[i], 13) == 0) {
			flag |= test_flag_client_only;
			continue;
		}
	}
	return found ? n_cli : NBR_ENOTFOUND;
}

int
testmain(char *prog, int n_cli, U32 flag)
{
	vmd d;
	int r, sv_argc = 1;
	char *sv_argv[] = { "./test/sv.conf", NULL };
	char *cl_argv[] = { "./test/cl.conf", NULL };
	if ((r = d.init(sv_argc,sv_argv)) < 0) {
		return r;
	}
	for (int i = 0; i < n_cli; i++) {
		if ((r = app::daemon::fork(prog, cl_argv, NULL)) < 0) {
			ASSERT(false);
			return r;
		}
	}
	if (!(flag & test_flag_client_only)) {
		return d.run();
	}
	sleep(10);
	return NBR_OK;
}

int
main(int argc, char *argv[])
{
	vmd d;
	U32 f;
	int r = test(argc, argv, f);
	if (r > 0) {
		return testmain(argv[0], r, f);
	}
	if ((r = d.init(argc,argv)) < 0) {
		return r;
	}
	return d.run();
}


