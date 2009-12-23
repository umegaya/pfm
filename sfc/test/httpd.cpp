#include "httpd.h"

/*-------------------------------------------------------------*/
/* get_request_session										   */
/*-------------------------------------------------------------*/
int
get_request_session::process(response &resp)
{
	fprintf(stdout, "response len : %u\n", resp.bodylen());
	fprintf(stdout, "%s\n", resp.body());
	return NBR_OK;
}

int
get_request_session::send_request()
{
	return get("localhost/consv.xml", NULL, NULL, 0);
}

/*-------------------------------------------------------------*/
/* testhttpd												   */
/*-------------------------------------------------------------*/
session::factory *
testhttpd::create_factory(const char *sname)
{
	return new httpfactory<get_request_session>;
}
