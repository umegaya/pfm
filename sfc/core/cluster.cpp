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
using namespace sfc::cluster;

/*-------------------------------------------------------------*/
/* sfc::cluster												   */
/*-------------------------------------------------------------*/
/* cluster_finder */
int cluster_finder::on_inquiry(const char *sym, char *opt, int olen)
{
#if defined(_DEBUG)
	address a;
	a.from(m_sk);
	TRACE("on_inquery: from <%s> sym=<%s>\n", (const char *)a, sym);
#endif
	cluster_finder_factory *nf = (cluster_finder_factory *)m_f;
	factory *msh = (factory *)nf->find_factory(sym);
	int r = NBR_ENOTFOUND;
	if (!msh) {
		TRACE( "on_inquiry: no factory for <%s>\n", sym);
		close(); return NBR_OK;
	}
	cluster_property &p = (cluster_property &)msh->cfg();
	/* initialize phase */
	if (config::cmp(p.m_sym_master_init, sym)) {
		r = msh->event(cluster_protocol::nev_finder_query_master_init,
				(char *)&m_sk, sizeof(m_sk));
	}
	if (config::cmp(p.m_sym_servant_init, sym)) {
		r = msh->event(cluster_protocol::nev_finder_query_servant_init,
				(char *)&m_sk, sizeof(m_sk));
	}
	/* polling phase */
	if (config::cmp(p.m_sym_poll, sym)) {
		r = msh->event(cluster_protocol::nev_finder_query_poll,
				(char *)&m_sk, sizeof(m_sk));
	}
	if (r < 0) {
		log(ERROR, "cannot send event for <%s>\n", sym);
	}
	return NBR_OK;
}

int cluster_finder::on_reply(const char *sym, const char *opt, int olen)
{
#if defined(_DEBUG) && 0
	address a;
	a.from(m_sk);
	TRACE("on_reply: from <%s> sym=<%s>\n", (const char *)a, sym);
#endif
	cluster_finder_factory *nf = (cluster_finder_factory *)m_f;
	factory *msh = (factory *)nf->find_factory(sym);
	int r = NBR_ENOTFOUND;
	if (!msh) {
		TRACE( "on_reply: no factory for <%s>\n", sym);
		close(); return NBR_OK;
	}
	cluster_property &p = (cluster_property &)msh->cfg();
	if (config::cmp(p.m_sym_master_init, sym) || config::cmp(p.m_sym_servant_init, sym)) {
		r = msh->event(cluster_protocol::nev_finder_reply_init, opt, olen);
	}
	if (config::cmp(p.m_sym_poll, sym)) {
		r = msh->event(cluster_protocol::nev_finder_reply_poll, opt, olen);
	}
	if (r < 0) {
		log(ERROR, "cannot send event for <%s>\n", sym);
	}
	return NBR_OK;
}
/* cluster_finder_factory_container */
cluster_finder_factory cluster_finder_factory_container::m_finder;
int cluster_finder_factory_container::init(const cluster_property &cfg)
{
	int r;
	const cluster_property &p = (const cluster_property &)cfg;
	if (!p.m_master) {	/* servant needs packet backup for failover */
		int size = p.m_packet_backup_size > 0 ? p.m_packet_backup_size : p.m_wbuf;
		if (!m_pkt_bkup.init(size)) {
			TRACE("finder_mixin: packet backuper init fail\n");
			return r;
		}
	}
	if (!m_finder.is_initialized()) {
		TRACE("initialize finder\n");
		if ((r = m_finder.init(cfg.m_mcast,
				finder_factory::on_recv<cluster_finder>)) < 0) {
			TRACE("container: cannot init finder (%d)\n", r);
			return r;
		}
	}
	return m_finder.is_initialized() ? NBR_OK : NBR_EMALLOC;
}

void cluster_finder_factory_container::fin()
{
	m_pkt_bkup.fin();
	if (m_finder.is_initialized()) {
		m_finder.fin();
	}
}

int cluster_finder_factory_container::writable() const
{
	return primary() ? primary()->writable() : 0;
}

int cluster_finder_factory_container::cluster_unicast(session *sender, bool reply,
		const address &addr, U32 msgid, const char *p, int l)
{
	char buf[l + 1 + sizeof(addr) * 2 + sizeof(U32)];
	PUSH_START(buf, sizeof(buf));
	PUSH_8(reply ? cluster_protocol::ncmd_unicast_reply :
				cluster_protocol::ncmd_unicast);
	PUSH_ADDR(addr);
	PUSH_32(msgid);
	PUSH_MEM(p, l);
#if defined(_DEBUG)
	hexdump(p, l);
	hexdump(buf, PUSH_LEN());
#endif
	return send(*primary(), sender, msgid, buf, PUSH_LEN());
}

int cluster_finder_factory_container::cluster_broadcast(session *sender,
		U32 msgid, const char *p, int l)
{
	char buf[l + 1 + sizeof(U32)];
	PUSH_START(buf, sizeof(buf));
	PUSH_8(cluster_protocol::ncmd_broadcast);
	PUSH_32(msgid);
	PUSH_MEM(p, l);
	return send(*primary(), sender, msgid, buf, PUSH_LEN());
}

int cluster_finder_factory_container::send(session &s, session *sender,
		U32 msgid, const char *p, int l)
{
	if (s.writable() >= l) {
		node::querydata *q;
		if (msgid != 0 && sender) {
			//char *ptr = (char *)m_pkt_bkup.write((U8 *)p, l, ret);
			if ((q = (node::querydata *)s.f()->querymap().create(msgid))) {
				//((nodequery *)q->data)->p = ptr;
				//((nodequery *)q->data)->l = ret;
				q->s = sender;
				q->msgid = msgid;
				q->sk = sender->sk();
			}
			else { return NBR_EEXPIRE; }
		}
		if (s.send(p, l) < 0) {
			if (q) { s.f()->querymap().erase(msgid); }
			return NBR_ESEND;
		}
		return l;
	}
	return NBR_ESHORT;
}

void cluster_finder_factory_container::switch_primary(node *new_node)
{
	new_node->f()->log(kernel::INFO,
			"[%s] primary: to <%s>\n", (const char *)new_node->addr());
	m_primary = new_node;
}

int cluster_finder_factory_container::bcast_sender::senddata(
		U32 msgid, const char *p, int l) {
	return m_c.cluster_broadcast(m_s, msgid, p, l);
}

int cluster_finder_factory_container::ucast_sender::senddata(
		U32 msgid, const char *p, int l) {
	ASSERT(p[l - 1] == 0);
	return m_c->cluster_unicast(m_s, m_reply != 0, m_a, msgid, p, l);
}

/* property */
finder_property cluster_property::m_mcast("finder",
		"0.0.0.0:8888",
		10, 30, opt_expandable,/* max 10 session/30sec timeout */
		256, 2048, /* send 256b,recv2kb */ 0, 0,/* no ping */
		-1,0,/* no query buffer */
		"UDP", "eth0", 10 * 1000 * 1000/* 10 sec task span */,
		0/* never wait ld recovery */,
		kernel::INFO,
		nbr_sock_rparser_bin16,
		nbr_sock_send_bin16,
		config::cfg_flag_server,
		finder_property::MCAST_GROUP, 8888, 1/* ttl = 1 */);

cluster_property::cluster_property(BASE_CONFIG_PLIST, const char *sym,
	int master, int multiplex, int packet_backup_size) :
	config(BASE_CONFIG_CALL), m_master(master), m_multiplex(multiplex),
	m_packet_backup_size(packet_backup_size)
{
	set("finder_symbol", sym);
}

int
cluster_property::set(const char *k, const char *v)
{
	if (nbr_mem_cmp(k, "finder.", sizeof("finder.") - 1) == 0) {
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
		nbr_str_printf(m_sym_master_init,
				finder_protocol::SYM_SIZE, "%s_master_init", v);
		nbr_str_printf(m_sym_servant_init,
				finder_protocol::SYM_SIZE, "%s_servant_init", v);
		nbr_str_printf(m_sym_poll,
				finder_protocol::SYM_SIZE, "%s_poll", v);
		return NBR_OK;
	}
	return config::set(k, v);
}

/*-------------------------------------------------------------*/
/* sfc::cluster::node										   */
/*-------------------------------------------------------------*/
void
node::nodedata::setdata(factory &f)
{
	SKMSTAT st;
	f.stat(st);
	m_n_servant = st.n_connection;
	ASSERT(m_n_servant < 1000);
}

int
node::nodedata::pack(char *p, int l) const
{
	PUSH_START(p, l);
	PUSH_16(m_n_servant);
	ASSERT(m_n_servant < 1000);
	return PUSH_LEN();
}

int
node::nodedata::unpack(const char *p, int l)
{
	ASSERT(l >= 2);
	POP_START(p, l);
	POP_16(m_n_servant);
	ASSERT(m_n_servant < 1000);
	return POP_LEN();
}

