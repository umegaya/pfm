/***************************************************************
 * session.h : template implementation part of sfc::session
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
using namespace sfc::finder;
using namespace sfc::app;

/*-------------------------------------------------------------*/
/* sfc::finder												   */
/*-------------------------------------------------------------*/
/* protocol */
int finder_protocol::on_inquiry(const char *sym, char *, int)
{
	daemon::log(INFO, "default: not respond to <%s>\n", sym);
	ASSERT(false);
	return NBR_ENOTFOUND;
}

/* factory */
int finder_factory::init(const config &cfg)
{
	return init(cfg, NULL);
}

int finder_factory::init(const config &cfg, int (*pp)(SOCK, char*, int))
{
	finder_property c = (const finder_property &)cfg;
	nbr_str_printf(c.m_host, sizeof(c.m_host) - 1, "0.0.0.0:%hu",
		c.m_mcastport);
	c.m_proto_name = "UDP";
	c.m_flag |= config::cfg_flag_server;
	if (!m_sl.init(DEFAULT_REGISTER, DEFAULT_HASHSZ, -1, opt_threadsafe)) {
		return NBR_EEXPIRE;
	}
	return super::init(c, NULL, NULL,
			pp, NULL, NULL,	NULL, NULL);
}

int finder_factory::inquiry(const char *sym)
{
	char work[1024], addr[1024];
	PUSH_START(work, sizeof(work));
	PUSH_8(0);
	PUSH_STR(sym);
	finder_property &p = (finder_property &)cfg();
	nbr_str_printf(addr, sizeof(addr) - 1, "%s:%hu",
		p.m_mcastaddr, p.m_mcastport);
	return mcast(addr, work, PUSH_LEN());
}

/* property */
const char 	finder_property::MCAST_GROUP[] = "239.192.1.2";
int
finder_property::set(const char *k, const char *v)
{
	if (cmp("mcastaddr", k)) {
		nbr_str_copy(m_mcastaddr, sizeof(m_mcastaddr), v, MAX_VALUE_STR);
		return NBR_OK;
	}
	else if (cmp("mcastport", k)) {
		SAFETY_ATOI(v, m_mcastport, U16);
		return NBR_OK;
	}
	return config::set(k, v);
}

void*
finder_property::proto_p() const
{
	return (void *)&m_mcastconf;
}

/* finder */
int finder_session::log(loglevel lv, const char *fmt, ...)
{
	if (lv < m_f->cfg().m_verbosity) {
		return NBR_OK;
	}
	char buff[4096];

	va_list v;
	va_start(v, fmt);
	vsnprintf(buff, sizeof(buff) - 1, fmt, v);
	va_end(v);

	fprintf(stdout, "%u[finder]%u:%s", nbr_osdep_getpid(), lv, buff);
	return NBR_OK;
}
