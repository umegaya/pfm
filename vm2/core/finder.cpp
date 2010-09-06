#include "finder.h"
#include "stdarg.h"

using namespace pfm;
using namespace pfm::cluster;

/*-------------------------------------------------------------*/
/* finder												   */
/*-------------------------------------------------------------*/
/* factory */
int finder_factory::init(const config &cfg, int (*pp)(SOCK, char*, int))
{
	const finder_property &c = (const finder_property &)cfg;
	nbr_str_printf(m_mcast_addr.a(), sizeof(m_mcast_addr) - 1, "%s:%hu",
		c.m_mcastaddr, c.m_mcastport);
	return super::init(c, NULL, NULL,
			pp, NULL, NULL,	NULL, NULL);
}

/* property */
const char 	finder_property::MCAST_GROUP[] = "239.192.1.2";
finder_property::finder_property(BASE_CONFIG_PLIST,
	const char *mcastgrp, U16 mcastport, int ttl) :
	config(BASE_CONFIG_CALL), m_mcastport(mcastport)
{
	if (!(*mcastgrp)) { mcastgrp = MCAST_GROUP; }
	nbr_str_copy(m_mcastaddr, sizeof(m_mcastaddr),
		mcastgrp, sizeof(m_mcastaddr));
	m_mcastconf.mcast_addr = m_mcastaddr;
	m_mcastconf.ttl = ttl;
//	nbr_str_printf(m_host, sizeof(m_host) - 1, "0.0.0.0:%hu", m_mcastport);
	m_flag |= config::cfg_flag_server;
}

int
finder_property::set(const char *k, const char *v)
{
	if (cmp("mcastaddr", k)) {
		nbr_str_copy(m_mcastaddr, sizeof(m_mcastaddr), v, MAX_VALUE_STR);
		return NBR_OK;
	}
	else if (cmp("mcastport", k)) {
		SAFETY_ATOI(v, m_mcastport, U16);
		nbr_str_printf(m_host, sizeof(m_host) - 1, "0.0.0.0:%hu", m_mcastport);
		return NBR_OK;
	}
	int r = config::set(k, v);
	if (r >= 0) {
		m_flag |= config::cfg_flag_server;
	}
	return r;
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
