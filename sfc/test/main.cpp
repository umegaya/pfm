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
#include "shell.h"
#include "echo.h"
#include "typedef.h"
#include "macro.h"
#include "str.h"
using namespace sfc;

#define DAEMON_ENTRY(name)										\
	if (nbr_str_cmp(dname, 256, #name, sizeof(#name)) == 0 ||	\
		nbr_str_cmp(dname, 256, "any", sizeof("any")) == 0) {	\
		p = new name;											\
		if (((name *)p)->init(argc, argv) < 0) {				\
			delete p;											\
			return NULL;										\
		}														\
		return p;												\
	}

daemon *
init_daemon(char *dname, int argc, char *argv[])
{
	daemon *p;
	DAEMON_ENTRY(shelld);
	DAEMON_ENTRY(testhttpd);
	DAEMON_ENTRY(echod);
	return NULL;
}

int
main(int argc, char *argv[])
{
	daemon *d;
	if (argc <= 1) {
		//argv[1] = "testhttpd";
		argv[1] = "shelld";
		argc = 2;
	}
	if (argc < 3) {
		char *srv_argv[] = { argv[0], argv[1], "./test/srv.conf", NULL };
		char *cli_argv[] = { "./test/cli.conf", NULL };
		int r;
		if ((r = daemon::fork(argv[0], srv_argv, NULL)) < 0) {
			return -2;
		}
		nbr_osdep_sleep(500 * 1000 * 1000/* 500ms */);
		daemon::log(kernel::INFO, "fork success: pid = %d\n", r);
		if (!(d = init_daemon(argv[1], 1, cli_argv))) {
			daemon::log(kernel::ERROR, "init_daemon fail (%s)\n", argv[1]);
			return -1;
		}
		daemon::log(kernel::INFO, "start client\n");
		d->run();
		delete d;
		return 0;
	}
	/* above daemon::fork comming to here */
	if (!(d = init_daemon(argv[1], argc - 2, argv + 2))) {
		daemon::log(kernel::ERROR, "init_daemon fail (%s)\n", argv[1]);
		return -1;
	}
	daemon::log(kernel::INFO, "start server\n");
	d->run();
	delete d;
	return 0;
}
