#include "httpd.h"

config	*cnf[] =
{
	new config (
		"get",
		"localhost",
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
