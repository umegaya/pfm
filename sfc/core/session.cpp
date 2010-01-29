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

/*-------------------------------------------------------------*/
/* sfc::session::factory									   */
/*-------------------------------------------------------------*/
int session::factory::log(loglevel lv, const char *fmt, ...)
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


int session::factory::connect(session *s,
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

int session::factory::base_init()
{
	m_lk = nbr_rwlock_create();
	return m_lk ? NBR_OK : NBR_EPTHREAD;
}

void session::factory::base_fin()
{
	if (m_lk) {
		nbr_rwlock_destroy(m_lk);
		m_lk = NULL;
	}
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
							void (*meh)(SOCKMGR, int, char*, int),
							int (*oc)(SOCK, void*),
							UTIME (*poll)(SOCK))
{
	m_cfg = &cfg;
	if (cfg.m_max_query > 0 && cfg.m_query_bufsz > 0) {
		if (!m_ql.init(cfg.m_max_query, cfg.m_max_query, cfg.m_query_bufsz,
				opt_threadsafe | opt_expandable)) {
			log(ERROR, "session::factory: cannot init query buff (%d,%d)\n",
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
		log(ERROR, "session::factory: cannot sockmgr\n");
		return NBR_ESOCKET;
	}
	nbr_sockmgr_set_data(m_skm, this);
	nbr_sockmgr_set_callback(m_skm, cfg.m_fnp, aw, cw, pp, eh, poll);
	nbr_sockmgr_set_connect_cb(m_skm, oc);
	nbr_sockmgr_set_mgrevent_cb(m_skm, meh);
	return NBR_OK;
}



/*-------------------------------------------------------------*/
/* sfc::session::binprotocol						   		   */
/*-------------------------------------------------------------*/
int session::binprotocol::sendping(class session &s, UTIME ut)
{
	/* disabled ping? */
	if (s.cfg().m_ping_timeo <= 0) { return NBR_OK; }
	char work[64];
	PUSH_START(work, sizeof(work));
	PUSH_8(cmd_ping);
	PUSH_64(ut);
	return s.send(work, PUSH_LEN());
}

int session::binprotocol::recvping(class session &s, char *p, int l)
{
	/* disabled ping? */
	if (s.cfg().m_ping_timeo <= 0) { return NBR_OK; }
	if (*p != 0) {
		return NBR_OK;	/* not ping packet */
	}
	U8 cmd;
	U64 ut;
	POP_START(p, l);
	POP_8(cmd);
	POP_64(ut);
	s.update_latency((U32)(nbr_clock() - ut));
	return NBR_OK;
}



/*-------------------------------------------------------------*/
/* sfc::session::textprotocol						   		   */
/*-------------------------------------------------------------*/
const char session::textprotocol::cmd_ping[] = "0";
int session::textprotocol::sendping(class session &s, UTIME ut)
{
	/* disabled ping? */
	if (s.cfg().m_ping_timeo <= 0) { return NBR_OK; }
	char work[64];
	PUSH_TEXT_START(work, cmd_ping);
	PUSH_TEXT_BIGNUM(ut);
	return s.send(work, PUSH_TEXT_LEN());
}

int session::textprotocol::recvping(class session &s, char *p, int l)
{
	if (s.cfg().m_ping_timeo <= 0) { return NBR_OK; }
	if (*p != '0') {
		return NBR_OK; /* no ping */
	}
	/* disabled ping? */
	char cmd[sizeof(cmd_ping) + 1];
	U64 ut;
	POP_TEXT_START(p, l);
	POP_TEXT_STR(cmd, sizeof(cmd));
	POP_TEXT_BIGNUM(ut, U64);
	s.update_latency((U32)(nbr_clock() - ut));
	return NBR_OK;
}

bool session::textprotocol::cmp(const char *a, const char *b)
{
	return nbr_str_cmp(a, 256, b, 256) == 0;
}

int session::textprotocol::hexdump(char *out, int olen, const char *in, int ilen)
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



/*-------------------------------------------------------------*/
/* sfc::finder												   */
/*-------------------------------------------------------------*/
const char 	finder::MCAST_GROUP[] = "239.192.1.2";

/* factory */
int finder::factory::init(const config &cfg)
{
	return init(cfg, finder::factory::on_recv<finder>);
}
int finder::factory::init(const config &cfg, int (*pp)(SOCK, char*, int))
{
	finder::property c = (const finder::property &)cfg;
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

int finder::factory::inquiry(const char *sym)
{
	char work[1024], addr[1024];
	PUSH_START(work, sizeof(work));
	PUSH_8(0);
	PUSH_STR(sym);
	finder::property &p = (finder::property &)cfg();
	nbr_str_printf(addr, sizeof(addr) - 1, "%s:%hu",
		p.m_mcastaddr, p.m_mcastport);
	return mcast(addr, work, PUSH_LEN());
}

/* property */
int
finder::property::set(const char *k, const char *v)
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
finder::property::proto_p() const
{
	return (void *)&m_mcastconf;
}

/* finder */
int finder::log(loglevel lv, const char *fmt, ...)
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

/*-------------------------------------------------------------*/
/* sfc::cluster::node										   */
/*-------------------------------------------------------------*/
/* nodefinder */
int node::nodefinder::on_inquiry(const char *sym, char *opt, int olen)
{
	finder::factory *nf = (finder::factory *)m_f;
	node::factory *msh = (node::factory *)nf->find_factory(sym);
	node::property &p = (node::property &)msh->cfg();
	int r;
	/* initialize phase */
	if (check_sym(p.m_sym_master_init, sym)) {
		r = msh->event(node::finder_mixin::convert(fev_init_master_query),
				(char *)&m_sk, sizeof(m_sk));
	}
	if (check_sym(p.m_sym_servant_init, sym)) {
		r = msh->event(node::finder_mixin::convert(fev_init_servant_query),
				(char *)&m_sk, sizeof(m_sk));
	}
	/* polling phase */
	if (check_sym(p.m_sym_poll, sym)) {
		r = msh->event(node::finder_mixin::convert(fev_poll_query),
				(char *)&m_sk, sizeof(m_sk));
	}
	if (r < 0) {
		log(ERROR, "cannot send event for <%s>\n", sym);
	}
	/* no need for this session. closed */
	close();
	return NBR_OK;
}

int node::nodefinder::on_reply(const char *sym, const char *opt, int olen)
{
	finder::factory *nf = (finder::factory *)m_f;
	node::factory *msh = (node::factory *)nf->find_factory(sym);
	node::property &p = (node::property &)msh->cfg();
	int r;
	if (check_sym(p.m_sym_master_init, sym) || check_sym(p.m_sym_servant_init, sym)) {
		r = msh->event(node::finder_mixin::convert(fev_init_reply), opt, olen);
	}
	if (check_sym(p.m_sym_poll, sym)) {
		r = msh->event(node::finder_mixin::convert(fev_poll_reply), opt, olen);
	}
	if (r < 0) {
		log(ERROR, "cannot send event for <%s>\n", sym);
	}
	/* no need for this session. closed */
	close();
	return NBR_OK;
}

/* finder_mixin */
finder::factory *node::finder_mixin::m_finder = NULL;
int node::finder_mixin::init_finder(const config &cfg)
{
	int r;
	const node::property &p = (const node::property &)cfg;
	if (!p.m_master) {	/* servant needs packet backup for failover */
		int size = p.m_packet_backup_size > 0 ? p.m_packet_backup_size : p.m_wbuf;
		if (!m_pkt_bkup.init(size)) {
			TRACE("finder_mixin: packet backuper init fail\n");
			return r;
		}
	}
	if (!m_finder) {
		TRACE("initialize finder\n");
		m_finder = new finder::factory;
		if ((r = m_finder->init(p.m_mcast,
				finder::factory::on_recv<node::nodefinder>)) < 0) {
			TRACE("finder_mixin: cannot init finder (%d)\n", r);
			return r;
		}
	}
	return m_finder ? NBR_OK : NBR_EMALLOC;
}

int node::finder_mixin::unicast(const address &addr, U32 msgid, char *p, int l)
{
	char buf[l + 1 + sizeof(addr) * 2 + sizeof(U32)];
	PUSH_START(buf, sizeof(buf));
	PUSH_8(ncmd_unicast);
	PUSH_ADDR(addr);
	PUSH_32(msgid);
	PUSH_MEM(p, l);
	return send(*primary(), msgid, buf, PUSH_LEN());
}

int node::finder_mixin::broadcast(U32 msgid, char *p, int l)
{
	char buf[l + 1 + sizeof(U32)];
	PUSH_START(buf, sizeof(buf));
	PUSH_8(ncmd_broadcast);
	PUSH_32(msgid);
	PUSH_MEM(p, l);
	return send(*primary(), msgid, buf, PUSH_LEN());
}

int node::finder_mixin::send(session &s, U32 msgid, char *p, int l)
{
	if (s.writable() >= l) {
		U32 ret = l;
		session::factory::query *q;
		if (msgid != 0) {
			char *ptr = (char *)m_pkt_bkup.write((U8 *)p, l, ret);
			q = s.f()->querymap().create(msgid);
			if (q) {
				((nodequery *)q->data)->p = ptr;
				((nodequery *)q->data)->l = ret;
			}
			else { return NBR_EEXPIRE; }
		}
		if (s.send(p, ret) < 0) {
			if (q) { s.f()->querymap().erase(msgid); }
			return NBR_ESEND;
		}
		return ret;
	}
	return NBR_ESHORT;
}

void node::finder_mixin::switch_primary(node *new_node)
{

}

/* property */
finder::property node::property::m_mcast("finder",
		"0.0.0.0:8888",
		10, 10, opt_expandable,/* max 10 session/10sec timeout */
		256, 2048, /* send 256b,recv2kb */ 0, 0,/* no ping */
		-1,0,/* no query buffer */
		"UDP", 10 * 1000 * 1000/* 10 sec task span */,
		0/* never wait ld recovery */,
		kernel::INFO,
		nbr_sock_rparser_bin16,
		nbr_sock_send_bin16,
		config::cfg_flag_server,
		finder::MCAST_GROUP, 8888, 1/* ttl = 1 */);

node::property::property(BASE_CONFIG_PLIST, const char *sym,
	int master, int multiplex, int packet_backup_size) :
	config(BASE_CONFIG_CALL), m_master(master), m_multiplex(multiplex),
	m_packet_backup_size(packet_backup_size)
{
	set("finder_symbol", sym);
}

int
node::property::set(const char *k, const char *v)
{
	if (nbr_mem_cmp(k, "finder.", sizeof("finder.") - 1)) {
		return m_mcast.set(k + sizeof("finder.") - 1, v);
	}
	else if (cmp("packet_backup_size", k)) {
		SAFETY_ATOI(v, m_packet_backup_size, int);
		return NBR_OK;
	}
	else if (cmp("multiplex", k)) {
		SAFETY_ATOI(v, m_multiplex, U8);
		return NBR_OK;
	}
	else if (cmp("finder_symbol", k)) {
		nbr_str_printf(m_sym_master_init, finder::SYM_SIZE, "%s_master_init", v);
		nbr_str_printf(m_sym_servant_init, finder::SYM_SIZE, "%s_servant_init", v);
		nbr_str_printf(m_sym_poll, finder::SYM_SIZE, "%s_poll", v);
		return NBR_OK;
	}
	return config::set(k, v);
}

