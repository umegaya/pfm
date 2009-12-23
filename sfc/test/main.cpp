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

config	*cnf[] =
{
	new config (
		"get",
		"",
		60,
		60, 0,
		4096, 1024,
		0, 0,
		"TCP",
		1 * 1000 * 1000/* 1sec task span */,
		0/* never wait ld recovery */,
		nbr_sock_rparser_raw,
		nbr_sock_send_raw
	),
};

int
main(int argc, char *argv[])
{
	testhttpd daemon;
	if (daemon.init(argc, argv, cnf, 1) < 0) {
		return -1;
	}
	return daemon.run();
}
