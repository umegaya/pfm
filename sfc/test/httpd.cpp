/***************************************************************
 * httpd.cpp : testing suite of http.h/cpp
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
	return get("/repos/nbr/", NULL, NULL, 0);
}

/*-------------------------------------------------------------*/
/* testhttpd												   */
/*-------------------------------------------------------------*/
session::factory *
testhttpd::create_factory(const char *sname)
{
	return new httpfactory<get_request_session>;
}

int
testhttpd::boot(int argc, char *argv[])
{
	session::factory *f = m_sl.find("get");
	config *c = m_cl.find("get");
	if (!c || !f) {
		log(LOG_ERROR, "conf or factory not found for 'get'\n");
		return NBR_ENOTFOUND;
	}
	for (int i = 0; i < c->m_max_connection; i++) {
		if (f->connect("localhost:80") < 0) {
			log(LOG_ERROR, "connect fail\n");
			return NBR_EINVAL;
		}
	}
	return NBR_OK;
}
