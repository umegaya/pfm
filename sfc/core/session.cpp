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

int session::factory::log(int lv, const char *fmt, ...)
{
	char buff[4096];

	va_list v;
	va_start(v, fmt);
	vsnprintf(buff, sizeof(buff) - 1, fmt, v);
	va_end(v);

	fprintf(stdout, "[%s]%u:%s", cfg().m_name, lv, buff);
	return NBR_OK;
}


int session::factory::connect(session *s,
		const char *addr/*= NULL*/, void *p/* = NULL*/)
{
	SOCK sk = nbr_sockmgr_connect(m_skm, addr ? addr : m_cfg->m_host, p, s);
	return nbr_sock_valid(sk) ? NBR_OK : NBR_ECONNECT;
}

int session::factory::mcast(const char *addr, char *p, int l)
{
	return nbr_sockmgr_mcast(m_skm, addr, p, l);
}

int session::factory::init(const config &cfg,
							int (*aw)(SOCK),
							int (*cw)(SOCK, int),
							int (*pp)(SOCK, char*, int),
							int (*eh)(SOCK, char*, int),
							void (*oc)(SOCK, void*),
							void (*poll)(SOCK))
{
	m_cfg = &cfg;
	if (!(m_skm = nbr_sockmgr_create(cfg.m_rbuf, cfg.m_wbuf,
							cfg.m_max_connection,
							sizeof(session*),
							cfg.m_timeout,
							cfg.client() ? NULL : cfg.m_host,
							nbr_proto_from_name(cfg.m_proto_name),
							cfg.proto_p(),
							cfg.m_option))) {
		return NBR_ESOCKET;
	}
	nbr_sockmgr_set_data(m_skm, this);
	nbr_sockmgr_set_callback(m_skm, cfg.m_fnp, aw, cw, pp, eh, poll);
	nbr_sockmgr_set_connect_cb(m_skm, oc);
	return NBR_OK;
}

int session::pingmgr::send(class session &s)
{
	/* disabled ping? */
	if (s.cfg().m_ping_timeo <= 0) { return NBR_OK; }
	if (m_last_msgid != 0) {
		return NBR_OK;	/* last ping not replied yet */
	}
	char work[64];
	PUSH_START(work, sizeof(work));
	PUSH_8((U8)0);
	m_last_msgid = s.msgid();
	PUSH_32(m_last_msgid);
	return s.send(work, PUSH_LEN());
}

int session::pingmgr::recv(class session &s, char *p, int l)
{
	if (*p != 0) {
		return NBR_OK;	/* not ping packet */
	}
	/* disabled ping? */
	if (s.cfg().m_ping_timeo <= 0) { return NBR_OK; }
	U8 cmd;
	U32 msgid;
	POP_START(p, l);
	POP_8(cmd)
	POP_32(msgid);
	if (msgid != m_last_msgid) {
		return NBR_EINVAL;
	}
	return NBR_OK;
}

int session::log(int lv, const char *fmt, ...)
{
	char buff[4096];

	va_list v;
	va_start(v, fmt);
	vsnprintf(buff, sizeof(buff) - 1, fmt, v);
	va_end(v);

	fprintf(stdout, "[%s:%p]%u:%s", f()->cfg().m_name, this, lv, buff);
	return NBR_OK;
}
