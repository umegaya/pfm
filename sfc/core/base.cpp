/***************************************************************
 * session.cpp : abstruct connection class
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
#include "sfc.hpp"
#include "nbr_pkt.h"
#include <stdarg.h>
#include "typedef.h"
#include "macro.h"
#include "str.h"

using namespace sfc;
using namespace sfc::base;

/*-------------------------------------------------------------*/
/* sfc::factory									   */
/*-------------------------------------------------------------*/
int factory::log(loglevel lv, const char *fmt, ...)
{
	if (lv < cfg().m_verbosity) {
		return NBR_OK;
	}
	char buff[4096];

	va_list v;
	va_start(v, fmt);
	vsnprintf(buff, sizeof(buff) - 1, fmt, v);
	va_end(v);

	fprintf(stdout, "%u[%s]%u:%s", nbr_osdep_getpid(), cfg().m_name, lv, buff);
	return NBR_OK;
}


int factory::connect(session *s,
		const char *address/*= NULL*/, void *p/* = NULL*/)
{
	const char *a = s->addr();
	if (!(*a)) { a = address ? address : m_cfg->m_host; }
	SOCK sk = nbr_sockmgr_connect(m_skm, a, p, s);
	if (nbr_sock_valid(sk)) {
		return NBR_OK;
	}
	return NBR_ECONNECT;
}

int factory::base_init()
{
	m_lk = nbr_rwlock_create();
	return m_lk ? NBR_OK : NBR_EPTHREAD;
}

void factory::base_fin()
{
	if (m_lk) {
		nbr_rwlock_destroy(m_lk);
		m_lk = NULL;
	}
}

int factory::mcast(const char *addr, char *p, int l)
{
	return nbr_sockmgr_mcast(m_skm, addr, p, l);
}

int factory::init(const config &cfg,
							int (*aw)(SOCK),
							int (*cw)(SOCK, int),
							int (*pp)(SOCK, char*, int),
							int (*eh)(SOCK, char*, int),
							void (*meh)(SOCKMGR, int, char*, int),
							int (*oc)(SOCK, void*),
							UTIME (*poll)(SOCK))
{
	m_cfg = &cfg;
	if (cfg.m_max_query > 0) {
		if (!m_ql.init(cfg.m_max_query, cfg.m_max_query, cfg.m_query_bufsz,
				opt_threadsafe | opt_expandable)) {
			log(ERROR, "factory: cannot init query buff (%d,%d)\n",
				cfg.m_max_query, cfg.m_query_bufsz);
			return NBR_EEXPIRE;
		}
	}
	if (!(m_skm = nbr_sockmgr_create(cfg.m_rbuf, cfg.m_wbuf,
							cfg.m_max_connection,
							sizeof(session*),
							cfg.m_timeout,
							cfg.client() ? NULL : cfg.m_host,
							nbr_proto_from_name(cfg.m_proto_name),
							cfg.proto_p(),
							cfg.m_option))) {
		log(ERROR, "factory: cannot sockmgr\n");
		return NBR_ESOCKET;
	}
	nbr_sockmgr_set_data(m_skm, this);
	nbr_sockmgr_set_callback(m_skm, cfg.m_fnp, aw, cw, pp, eh, poll);
	nbr_sockmgr_set_connect_cb(m_skm, oc);
	nbr_sockmgr_set_mgrevent_cb(m_skm, meh);
	if (!cfg.client()) {
		int r = nbr_sockmgr_get_ifaddr(
				m_skm, cfg.m_ifname, m_ifaddr.a(), address::SIZE);
		if (r < 0) {
			log(ERROR, "get ifaddr fail (%d)\n", r);
			return r;
		}
		m_ifaddr.setlen(r);
	}
	return NBR_OK;
}



/*-------------------------------------------------------------*/
/* sfc::binprotocol						   		   */
/*-------------------------------------------------------------*/
int binprotocol::sendping(class session &s, UTIME ut)
{
	/* disabled ping? */
	char work[64];
	PUSH_START(work, sizeof(work));
	PUSH_8(cmd_ping);
	PUSH_64(ut);
	return s.send(work, PUSH_LEN());
}

int binprotocol::recvping(class session &s, char *p, int l)
{
	/* disabled ping? */
	if (*p != 0) {
		return no_ping;	/* not ping packet */
	}
	U8 cmd;
	U64 ut;
	POP_START(p, l);
	POP_8(cmd);
	POP_64(ut);
	if (s.cfg().client()) {
		s.update_latency((U32)(nbr_clock() - ut));
#if defined(_DEBUG) & 0
		s.log(kernel::INFO, "recvping: lacency=%u(%llu,%llu)\n", s.latency(),
				nbr_clock(), ut);
#endif
	}
	else {
		sendping(s, ut);
	}
	return NBR_OK;
}



/*-------------------------------------------------------------*/
/* sfc::textprotocol						   		   */
/*-------------------------------------------------------------*/
const char textprotocol::cmd_ping[] = "0";
int textprotocol::sendping(class session &s, UTIME ut)
{
	char work[64];
	PUSH_TEXT_START(work, cmd_ping);
	if (s.cfg().client()) {
		ut = nbr_time();
	}
	PUSH_TEXT_BIGNUM(ut);
#if defined(_DEBUG)
	s.log(kernel::INFO, "sendping: at %llu\n", ut);
#endif
	return s.send(work, PUSH_TEXT_LEN());
}

int textprotocol::recvping(class session &s, char *p, int l)
{
	if (*p != '0') {
		return no_ping; /* no ping */
	}
	/* disabled ping? */
	char cmd[sizeof(cmd_ping) + 1];
	U64 ut;
	POP_TEXT_START(p, l);
	POP_TEXT_STR(cmd, sizeof(cmd));
	POP_TEXT_BIGNUM(ut, U64);
	if (s.cfg().client()) {
		U64 now = nbr_time();
		s.update_latency((U32)(now - ut));
#if defined(_DEBUG)
		s.log(kernel::INFO, "recvping: lacency=%u(%llu,%llu)\n", s.latency(),
				now, ut);
#endif
	}
	else {
		sendping(s, ut);
	}
	return NBR_OK;
}

bool textprotocol::cmp(const char *a, const char *b)
{
	return nbr_str_cmp(a, 256, b, 256) == 0;
}

int textprotocol::hexdump(char *out, int olen, const char *in, int ilen)
{
	if ((ilen * 2) > olen) { return NBR_ESHORT; }
	const char *w = in;
	char *v = out;
	const char table[] = "0123456789abcdef";
	while (*w) {
		*v++ = table[((U8)*w) >> 4];
		*v++ = table[((U8)*w) & 0x0F];
		w++;
		if ((w - in) > ilen) {
			break;
		}
	}
	return (v - out);
}



/*-------------------------------------------------------------*/
/* sfc::session										   		   */
/*-------------------------------------------------------------*/
int session::log(loglevel lv, const char *fmt, ...)
{
	if (lv < cfg().m_verbosity) {
		return NBR_OK;
	}
	char buff[4096];

	va_list v;
	va_start(v, fmt);
	vsnprintf(buff, sizeof(buff) - 1, fmt, v);
	va_end(v);

	fprintf(stdout, "%u[%s:%p]%u:%s", nbr_osdep_getpid(),
			f()->cfg().m_name, this, lv, buff);
	return NBR_OK;
}

void session::setaddr(char *a)
{
	if (a) { m_addr.from(a); }
	else { m_addr.from(m_sk); }
}



