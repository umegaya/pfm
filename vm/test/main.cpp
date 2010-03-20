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

bool
test(int argc, char *argv[])
{
	for (int i = 0; i < argc; i++) {
		if (strcmp("--test", argv[i]) == 0) {
			return true;
		}
	}
	return false;
}

int
testmain(char *prog)
{
	vmd d;
	int r, sv_argc = 1;
	char *sv_argv[] = { "./test/sv.conf", NULL };
	char *cl_argv[] = { "./test/cl.conf", NULL };
	if ((r = d.init(sv_argc,sv_argv)) < 0) {
		return r;
	}
	for (int i = 0; i < 100; i++) {
		if ((r = app::daemon::fork(prog, cl_argv, NULL)) < 0) {
			return r;
		}
	}
	return d.run();
}

int
main(int argc, char *argv[])
{
	vmd d;
	int r;
	if (test(argc, argv)) {
		return testmain(argv[0]);
	}
	if ((r = d.init(argc,argv)) < 0) {
		return r;
	}
	return d.run();
}


