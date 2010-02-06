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
#include "typedef.h"
#include "macro.h"
#include "str.h"
#include <sys/stat.h>

using namespace sfc;
using namespace sfc::app;
using namespace sfc::http;

/*-------------------------------------------------------------*/
/* get_request_session										   */
/*-------------------------------------------------------------*/
int get_request_session::m_done = 0;
UTIME get_request_session::m_start = 0LL, get_request_session::m_end = 0LL;

int
get_request_session::process(response &resp)
{
	if (resp.error()) {
		log(ERROR, "internal error\n");
		return NBR_OK;
	}
//	log(INFO, "http result : %d\n", resp.rc());
//	log(INFO, "response len : %u\n", resp.bodylen());
	char mime[256];
	if (!resp.hdrstr("Content-Type", mime, sizeof(mime))) {
		log(INFO, "content-type unknown\n");
		ASSERT(FALSE);
		return NBR_OK;
	}
//	log(INFO, "content-type: %s\n", mime);
	char path[256], file[256];
	const char *ext;
	if ((ext = nbr_str_divide_tag_and_val('/', m_url, path, sizeof(path)))) {
		snprintf(file, sizeof(file) - 1, "./%s", ext);
		FILE *fp = fopen(file, "r");
		if (fp) {
#if 1
			struct stat st;
			fstat(fileno(fp), &st);
			ASSERT(st.st_size == resp.bodylen());

			char buffer[st.st_size];
			fread(buffer,1,st.st_size,fp);
			ASSERT(nbr_mem_cmp(buffer, resp.body(), resp.bodylen()) == 0);
#endif
			log(INFO, "%u:<%s> get test ok!\n", m_rid, file);
			m_done++;
			if (m_done >= 1000) {
				if (m_end == 0LL) {
					m_end = nbr_clock();
					log(FATAL, "### take %u us to handle 1000 request (%f qps) ###\n",
							(U32)(m_end - m_start),
							(float)((float)m_done * 1000 * 1000 / (float)(m_end - m_start)));
					daemon::stop();
				}
			}
			fclose(fp);
		}
		else {
			log(ERROR, "file %s not found\n", file);
			ASSERT(false);
		}
	}
	return NBR_OK;
}

int
get_request_session::on_close(int r)
{
//	address a;
//	localaddr(a);
	log(INFO, "on_close (%d:%d)\n", r, m_rid);
	ASSERT(m_end != 0LL || fsm().get_state() == fsm::state_recv_finish);
	return NBR_OK;
}

int
get_request_session::send_request()
{
//	address a;
//	localaddr(a);
	m_rid = f()->msgid();
	int r = get(m_url, NULL, NULL, 0, true);
	log(INFO, "send_request(%u) get(%s) result %d\n", m_rid, m_url, r);
	return r;
}


#define PORT "12345"
void
get_request_session::set_random_url()
{
	static const char *urls[] = {
		"localhost:"PORT"/test/files/eyes0164.jpg",
		"localhost:"PORT"/test/files/eyes0168.jpg",
		"localhost:"PORT"/test/files/eyes0172.jpg",
		"localhost:"PORT"/test/files/eyes0176.jpg",
		"localhost:"PORT"/test/files/eyes0182.jpg",
	};
	m_url = urls[nbr_rand32() % 5];
}

/*-------------------------------------------------------------*/
/* get_request_session										   */
/*-------------------------------------------------------------*/
map<get_response_session::fmem, char[256]>	get_response_session::m_res;

int
get_response_session::process(request &r)
{
	char buf[256], path[256];
	if (r.url(buf, sizeof(buf)) < 0) {
		log(ERROR, "cannot get url\n");
		return NBR_OK;
	}
	log(INFO, "url=<%s>\n", buf);
	snprintf(path, sizeof(path) - 1, "./%s", buf);
	fmem *fm = get_fmem(path);
	if (!fm) {
		log(INFO, "<%s> not found\n", path);
		send_result_code(HRC_NOT_FOUND, 1);
		return NBR_OK;
	}
	int len = send_result_and_body(HRC_OK, (const char *)fm->p, fm->l, "image/jpg");
//	log(INFO, "send %u byte\n", len);
	return len;
}

get_response_session::fmem *
get_response_session::get_fmem(const char *path)
{
	map<fmem, char[256]>::iterator it;
	fmem *p = m_res.find(path);
	if (p) {
//		TRACE("%s already loaded on memory (%p)\n", path, p);
		return p;
	}
	fmem fm = { NULL, 0 };
	FILE *fp = fopen(path, "r");
	if (!fp) {
		daemon::log(ERROR, "%s cannot open\n", path);
		goto error;
	}
	struct stat st;
	fstat(fileno(fp), &st);
	fm.p = (U8 *)nbr_mem_alloc(st.st_size);
	if (!(fm.p)) {
		daemon::log(ERROR, "memory cannot allocate %llu byte\n", (U64)st.st_size);
		goto error;
	}
	fm.l = st.st_size;
	fread(fm.p, 1, fm.l, fp);
	fclose(fp);
	if ((it = m_res.insert(fm, path)) == m_res.end()) {
		daemon::log(ERROR, "%s search engine full\n", path);
		goto error;
	}
	return &(*it);
error:
	if (fm.p) {
		nbr_mem_free(fm.p);
	}
	if (fp) {
		fclose(fp);
	}
	return NULL;
}

int
get_response_session::init_res()
{
	int r;
	if ((r = m_res.init(1000, 100, -1,
			opt_expandable | opt_threadsafe)) < 0) {
		return r;
	}
	return NBR_OK;
}

void
get_response_session::fin_res()
{
	if (m_res.initialized()) {
		map<fmem, char[256]>::iterator p = m_res.begin();
		for (; p != m_res.end(); p = m_res.next(p)) {
			if (p->p) {
				nbr_mem_free(p->p);
			}
		}
		m_res.fin();
	}
}

/*-------------------------------------------------------------*/
/* testhttpd												   */
/*-------------------------------------------------------------*/
factory *
testhttpd::create_factory(const char *sname)
{
	TRACE("create_factory: sname=<%s>\n", sname);
	if (config::cmp(sname, "sv")) {
		return new get_response_session::factory;
	}
	if (config::cmp(sname, "cl")) {
		return new get_request_session::factory;
	}
	return NULL;
}

int
testhttpd::create_config(config *cl[], int size)
{
	if (size <= 1) {
		return NBR_ESHORT;
	}
	cl[0] = new config (
			"cl",
			"localhost:"PORT,
			60,
			60, opt_not_set,
			128 * 1024, 1024,
			0, 0,	/* no ping */
			-1,0,	/* no query buffer */
			"TCP", "eth0",
			1 * 10 * 1000/* 100msec task span */,
			1/* after 10ms, again try to connect */,
			kernel::INFO,
			nbr_sock_rparser_raw,
			nbr_sock_send_raw,
			config::cfg_flag_not_set
			);
	cl[1] = new config (
			"sv",
			"0.0.0.0:"PORT,
			60,
			60, opt_not_set,
			1024, 32 * 1024,
			0, 0,	/* no ping */
			-1,0,	/* no query buffer */
			"TCP", "eth0",
			1 * 10 * 1000/* 100msec task span */,
			0/* never wait ld recovery */,
			kernel::INFO,
			nbr_sock_rparser_raw,
			nbr_sock_send_raw,
			config::cfg_flag_server
			);
	return 2;
}

int
testhttpd::boot(int argc, char *argv[])
{
	get_request_session::factory *f =
			find_factory<get_request_session::factory>("cl");
	config *c = find_config<config>("cl");
	if (!c || !f) {
		m_server = 1;
		return get_response_session::init_res();	/* maybe server session. ok */
	}
	get_request_session *s;
	get_request_session::m_start = nbr_clock();
	for (int i = 0; i < c->m_max_connection; i++) {
		if (!(s = f->pool().create())) {
			log(ERROR, "allocate session fail\n");
			return NBR_EINVAL;
		}
		s->set_random_url();
		if (f->connect(s) < 0) {
			log(ERROR, "connect fail\n");
			return NBR_EINVAL;
		}
	}
	return NBR_OK;
}

void
testhttpd::shutdown()
{
	if (m_server) {
		get_response_session::fin_res();
	}
}


