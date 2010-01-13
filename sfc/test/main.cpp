/***************************************************************
 * main.cpp : entry point of sfc test suite
 * 2009/12/23 iyatomi : create
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
#include "httpd.h"
using namespace sfc;

int
main(int argc, char *argv[])
{
	testhttpd daemon;
	if (argc < 2) {
		char *srv_argv[] = { argv[0], "./test/srv.conf", NULL };
		char *cli_argv[] = { "./test/cli.conf", NULL };
		int r;
		if ((r = daemon::fork(argv[0], srv_argv, NULL)) < 0) {
			return -2;
		}
		daemon::log(kernel::INFO, "fork success: pid = %d\n", r);
		if (daemon.init(1, cli_argv) < 0) {
			return -1;
		}
		return daemon.run();
	}
	/* above daemon::fork comming to here */
	if (daemon.init(argc, argv) < 0) {
		return -1;
	}
	return daemon.run();
}
