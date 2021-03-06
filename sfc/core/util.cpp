/***************************************************************
 * util.cpp : implemantion of sfc normal classes
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
#include "typedef.h"
#include "macro.h"
#include "str.h"

using namespace sfc;
using namespace sfc::util;

/*-------------------------------------------------------------*/
/* sfc::util::address										   */
/*-------------------------------------------------------------*/
int address::from(const char *a){
	m_len = nbr_str_copy(m_a, SIZE, a, SIZE);
	return m_len;
}

/*-------------------------------------------------------------*/
/* sfc::util::ringbuffer									   */
/*-------------------------------------------------------------*/
void
ringbuffer::fin()
{
	if (m_p) {
		nbr_mem_free(m_p);
		m_p = NULL;
	}
	if (m_lk) {
		nbr_rwlock_destroy(m_lk);
		m_lk = NULL;
	}
}

bool
ringbuffer::init(U32 size)
{
	if (!(m_lk = nbr_rwlock_create())) {
		fin();
		return false;
	}
	m_l = size;
	if (!(m_p = (U8 *)nbr_mem_alloc(m_l))) {
		fin();
		return false;
	}
	m_wp = m_rp = m_p;
	return true;
}



/*-------------------------------------------------------------*/
/* sfc::util::config										   */
/*-------------------------------------------------------------*/
config::config()
{
	m_name[0] = '\0';
	m_host[0] = '\0';
	m_max_connection = 0;
	m_timeout = 0;
	m_rbuf = m_wbuf = 0;
	m_proto_name = "";
	m_taskspan = m_ld_wait = 0;
	m_option = opt_not_set;
	m_ping_timeo = m_ping_intv = 0;
	m_flag = config::cfg_flag_not_set;
	m_ifname[0] = '\0';
}

config::config(BASE_CONFIG_PLIST) :
		BASE_CONFIG_SETPARAM
{
	nbr_str_copy(m_name, sizeof(m_name), name, sizeof(m_name));
	nbr_str_copy(m_host, sizeof(m_host), host, sizeof(m_host));
	nbr_str_copy(m_ifname, sizeof(m_ifname), ifname, sizeof(m_ifname));
}

config::~config()
{
}

const char *config::makepath(char *b, size_t bl, const char *base, const char *name)
{
	size_t sz = nbr_str_length(base, MAX_VALUE_STR);
	if (base[sz - 1] == '/') {
		snprintf(b, bl, "%s%s", base, name);
		return b;
	}
	snprintf(b, bl, "%s/%s", base, name);
	return b;
}

int config::load(const char *line)
{
	char key[256];
	const char *val;
	if (!(val = nbr_str_divide_tag_and_val('=', line, key, sizeof(key)))) {
		return NBR_EFORMAT;
	}
	return set(key, val);
}

int config::cmp(const char *a, const char *b)
{
	return nbr_str_cmp_nocase(a, b, MAX_VALUE_STR) == 0;
}

int config::str(const char *k, const char *&v) const
{
	if (cmp("name", k)) { v = m_name; }
	else if (cmp("host", k)) { v = m_host; }
	else if (cmp("proto", k)) { v = m_proto_name; }
	else if (cmp("ifname", k)) { v = m_ifname; }
	return NBR_ENOTFOUND;
}

int	config::num(const char *k, int &v) const
{
	if (cmp("max_connection", k)) { v = m_max_connection; }
	else if (cmp("timeout", k)) { v = m_timeout; }
	else if (cmp("rbuf", k)) { v = m_rbuf; }
	else if (cmp("wbuf", k)) { v = m_wbuf; }
	else if (cmp("ping_timeo", k)) { v = m_ping_timeo; }
	else if (cmp("ping_intv", k)) { v = m_ping_intv; }
	return NBR_ENOTFOUND;
}

int	config::bignum(const char *k, U64 &v) const
{
	if (cmp("taskspan", k)) { v = m_taskspan; }
	else if (cmp("ld_wait", k)) { v = m_ld_wait; }
	return NBR_ENOTFOUND;
}

int	config::set(const char *k, const char *v)
{
	int f;
	if (cmp("disabled", k)) {
		if (nbr_str_atoi(v, &f, MAX_VALUE_STR) >= 0 && v != 0) {
			m_flag |= cfg_flag_disabled;
		}
		return NBR_OK;
	}
	if (cmp("server", k)) {
		if (nbr_str_atoi(v, &f, MAX_VALUE_STR) >= 0 && v != 0) {
			m_flag |= cfg_flag_server;
		}
		return NBR_OK;
	}
	else if (cmp("host", k)) {
		nbr_str_copy(m_host, sizeof(m_host), v, MAX_VALUE_STR);
		return NBR_OK;
	}
	else if (cmp("max_connection", k)) {
		return nbr_str_atoi(v, &m_max_connection, MAX_VALUE_STR);
	}
	else if (cmp("timeout", k)) {
		return nbr_str_atoi(v, &m_timeout, MAX_VALUE_STR);
	}
	else if (cmp("rbuf", k)) {
		return nbr_str_atoi(v, &m_rbuf, MAX_VALUE_STR);
	}
	else if (cmp("wbuf", k)) {
		return nbr_str_atoi(v, &m_wbuf, MAX_VALUE_STR);
	}
	else if (cmp("option", k)) {
		return nbr_str_atoi(v, &m_option, MAX_VALUE_STR);
	}
	else if (cmp("ping_timeo", k)) {
		return nbr_str_atobn(v, (S64 *)&m_ping_timeo, MAX_VALUE_STR);
	}
	else if (cmp("ping_intv", k)) {
		return nbr_str_atobn(v, (S64 *)&m_ping_intv, MAX_VALUE_STR);
	}
	else if (cmp("taskspan", k)) {
		return nbr_str_atobn(v, (S64 *)&m_taskspan, MAX_VALUE_STR);
	}
	else if (cmp("ld_wait", k))  {
		return nbr_str_atobn(v, (S64 *)&m_ld_wait, MAX_VALUE_STR);
	}
	else if (cmp("proto", k)) {
		PROTOCOL *pr;
		if ((pr = nbr_proto_from_name(v))) {
			m_proto_name = v;
		}
		return pr ? NBR_OK : NBR_ENOTFOUND;
	}
	else if (cmp("ifname", k)) {
		nbr_str_copy(m_ifname, sizeof(m_ifname), v, MAX_VALUE_STR);
		return NBR_OK;
	}
	else if (cmp("sender", k)) {
		m_fns = sender_from(v);
		return NBR_OK;
	}
	else if (cmp("parser", k)) {
		m_fnp = rparser_from(v);
		return NBR_OK;
	}
	return NBR_ENOTFOUND;
}

config::parser
config::rparser_from(const char *str)
{
	if (cmp(str, "text")) {
		return nbr_sock_rparser_text;
	}
	else if (cmp(str, "bin16")) {
		return nbr_sock_rparser_bin16;
	}
	else if (cmp(str, "bin32")) {
		return nbr_sock_rparser_bin32;
	}
	else if (cmp(str, "raw")){
		return nbr_sock_rparser_raw;
	}
	ASSERT(FALSE);
	return nbr_sock_rparser_raw;
}

config::sender
config::sender_from(const char *str)
{
	if (cmp(str, "text")) {
		return nbr_sock_send_text;
	}
	else if (cmp(str, "bin16")) {
		return nbr_sock_send_bin16;
	}
	else if (cmp(str, "bin32")) {
		return nbr_sock_send_bin32;
	}
	else if (cmp(str, "raw")) {
		return nbr_sock_send_raw;
	}
	ASSERT(FALSE);
	return nbr_sock_send_raw;
}




