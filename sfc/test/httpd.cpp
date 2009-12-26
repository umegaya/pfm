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
	if (resp.error()) {
		log(LOG_ERROR, "internal error\n");
		return NBR_OK;
	}
	log(LOG_INFO, "http result : %d\n", resp.rc());
	log(LOG_INFO, "response len : %u\n", resp.bodylen());
	char mime[256];
	if (!resp.hdrstr("Content-Type", mime, sizeof(mime))) {
		log(LOG_INFO, "Content-Type unknown\n");
		return NBR_OK;
	}
	log(LOG_INFO, "content-type: %s\n", mime);
	char type[256], file[256];
	const char *ext;
	if ((ext = nbr_str_divide_tag_and_val('/', mime, type, sizeof(type)))) {
		snprintf(file, sizeof(file) - 1, "resp.%s", ext);
		FILE *fp = fopen(file, "w+");
		if (fp) {
			fwrite(resp.body(),1,resp.bodylen(),fp);
			fclose(fp);
		}
	}
	return NBR_OK;
}

int
get_request_session::on_close(int r)
{
	log(LOG_INFO, "connection closed (%d)\n", r);
	return NBR_OK;
}

int
get_request_session::send_request()
{
	int r = get(m_url, NULL, NULL, 0);
	TRACE("get_request_session::send_request get result %d\n", r);
	return r;
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
testhttpd::create_config(config *cl[], int size)
{
	if (size <= 0) {
		return NBR_ESHORT;
	}
	cl[0] = new config (
			"get",
			"",
			60,
			60, 0,
			1280 * 1024, 1024,
			0, 0,	/* no ping */
			"TCP",
			1 * 1000 * 1000/* 1sec task span */,
			0/* never wait ld recovery */,
			nbr_sock_rparser_raw,
			nbr_sock_send_raw
			);
	return 1;
}

int
testhttpd::boot(int argc, char *argv[])
{
	httpfactory<get_request_session> *f =
			(httpfactory<get_request_session> *)m_sl.find("get");
	config *c = m_cl.find("get");
	if (!c || !f) {
		log(LOG_ERROR, "conf or factory not found for 'get'\n");
		return NBR_ENOTFOUND;
	}
	get_request_session *s;
	for (int i = 0; i < 1 /*c->m_max_connection*/; i++) {
		if (!(s = f->pool().alloc())) {
			log(LOG_ERROR, "allocate session fail\n");
			return NBR_EINVAL;
		}
		s->seturl("sol-web.gamecity.ne.jp:8080"
				"/wpxy/1?method_id=55233&MsgID=0&StartIndex=0&Num=300");
		if (f->connect(s, "sol-web.gamecity.ne.jp:8080") < 0) {
			log(LOG_ERROR, "connect fail\n");
			return NBR_EINVAL;
		}
	}
	return NBR_OK;
}
