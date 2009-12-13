#include "sfc.hpp"

config::config()
{
	m_name = "";
	m_host = "";
	m_port = 0;
	m_max_connection = 0;
	m_timeout = 0;
	m_rbuf = m_wbuf = 0;
	m_proto = NULL;
	m_taskspan = m_ld_wait = 0;
	m_ping_timeo = m_ping_intv = 0;
}

config::~config()
{
}

int config::load(const char *line)
{
	char *val, key[256];
	if (!(val = nbr_str_divide_tag_and_val('=', line, key, sizeof(key)))) {
		return NBR_EFORMAT;
	}
	return set(key, val);
}

int config::cmp(const char *a, const char *b)
{
	return nbr_str_cmp_nocase(a, b, MAX_VALUE_STR) == 0;
}

int config::str(const char *k, char *&v) const
{
	if (cmp("name", k)) { v = m_name; }
	else if (cmp("host", k)) { v = m_host; }
	else {
		if (m_proto == nbr_proto_tcp()) {
			v = "TCP";
			return NBR_OK;
		}
		else if (m_proto == nbr_proto_udp()) {
			v = "UDP";
			return NBR_OK;
		}
	}
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
	int tmp;
	if (cmp("name", k)) {
		nbr_str_copy(m_name, v, MAX_VALUE_STR);
		return NBR_OK;
	}
	else if (cmp("host", k)) {
		nbr_str_copy(m_host, v, MAX_VALUE_STR);
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
		return nbr_str_atoi(v, &m_ping_timeo, MAX_VALUE_STR);
	}
	else if (cmp("ping_intv", k)) {
		return nbr_str_atoi(v, &m_ping_intv, MAX_VALUE_STR);
	}
	else if (cmp("taskspan", k)) {
		return nbr_str_atobn(v, &m_taskspan, MAX_VALUE_STR);
	}
	else if (cmp("ld_wait", k))  {
		return nbr_str_atobn(v, &m_ld_wait, MAX_VALUE_STR);
	}
	else if (cmp("proto", k)) {
		if (cmp("TCP", v)) {
			m_proto = nbr_proto_tcp();
			return NBR_OK;
		}
		else if (cmp("UDP", v)) {
			m_proto = nbr_proto_udp();
			return NBR_OK;
		}
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

parser
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

sender
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

