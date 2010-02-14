/***************************************************************
 * shell.cpp : implementation of shell.h
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
#include "shell.h"
#include "typedef.h"
#include "macro.h"
#include "str.h"
#if defined(_DEBUG)
#include "array.h"
#include "search.h"
#endif

using namespace sfc;
using namespace sfc::app;
using namespace sfc::cluster;

/*-------------------------------------------------------------*/
/* shelld::protocol											   */
/*-------------------------------------------------------------*/
const char shelld::protocol::cmd_list[] = "list";
const char shelld::protocol::cmd_copyinit[] = "copyinit";
const char shelld::protocol::cmd_copychunk[] = "copychunk";
const char shelld::protocol::cmd_exec[] = "exec";
const char shelld::protocol::code_list[] = "list_r";
const char shelld::protocol::code_copyinit[] = "copyinit_r";
const char shelld::protocol::code_copychunk[] = "copychunk_r";
const char shelld::protocol::code_exec_start[] = "exec_r";
const char shelld::protocol::code_exec_result[] = "exec_result";
const char shelld::protocol::code_exec_end[] = "exec_end";
char *shelld::protocol::debug_big_data = NULL;

int shelld::protocol::sendping(class session &s, UTIME ut)
{
	if (!debug_big_data) {
		debug_big_data = (char *)nbr_mem_alloc(debug_big_datasize + 1);
		memset(debug_big_data, 'a', debug_big_datasize);
		debug_big_data[debug_big_datasize] = '\0';
	}
	char work[64 + debug_big_datasize + 1];
	PUSH_TEXT_START(work, cmd_ping);
	if (s.cfg().client()) {
		ut = nbr_time();
	}
	PUSH_TEXT_BIGNUM(ut);
	PUSH_TEXT_STR(debug_big_data);
#if defined(_DEBUG)
	//s.log(kernel::INFO, "sendping: at %llu\n", ut);
#endif
	return ((shell_node &)s).senddata(0, work, PUSH_TEXT_LEN());
}

int shelld::protocol::recvping(class session &s, char *p, int l)
{
	TRACE("recv ping from (%s)\n", (const char *)s.addr());
	if (*p != '0') {
		return no_ping; /* no ping */
	}
	/* disabled ping? */
	char cmd[16 + 1];
	char dbd[debug_big_datasize + 1];
	U64 ut;
	POP_TEXT_START(p, l);
	POP_TEXT_STR(cmd, sizeof(cmd));
	POP_TEXT_BIGNUM(ut, U64);
	if ((size_t )(l - (__buf - p)) >= debug_big_datasize) {
		POP_TEXT_STR(dbd, sizeof(dbd));
	}
	else {
		TRACE("debug bigdata not available\n");
	}
	if (s.cfg().client()) {
		U64 now = nbr_time();
		s.update_latency((U32)(now - ut));
#if defined(_DEBUG)
		s.log(kernel::INFO, "recvping(shd): lacency=%u(%llu,%llu)\n", s.latency(),
				now, ut);
#endif
	}
	else {
		sendping(s, ut);
	}
#if defined(_DEBUG) && 0
	if ((nbr_rand32() % 3) == 0) { return NBR_EINVAL; }
#endif
	return NBR_OK;
}


/*-------------------------------------------------------------*/
/* shelld::protocol_impl									   */
/*-------------------------------------------------------------*/
template <class S>
int shelld::protocol_impl<S>::send_cmd_list(S &s, U32 msgid)
{
	char buf[2048];
	PUSH_TEXT_START(buf, cmd_list);
	PUSH_TEXT_NUM(msgid);
	return s.senddata(msgid, buf, PUSH_TEXT_LEN());
}

template <class S>
int shelld::protocol_impl<S>::send_cmd_copyinit(
		S &s, U32 msgid, const char *dst, int n_chunk, int chunksz)
{
	char buf[2048];
	PUSH_TEXT_START(buf, cmd_copyinit);
	PUSH_TEXT_NUM(msgid);
	PUSH_TEXT_STR(dst);
	PUSH_TEXT_NUM(n_chunk);
	PUSH_TEXT_NUM(chunksz);
	return s.senddata(msgid, buf, PUSH_TEXT_LEN());
}

template <class S>
int shelld::protocol_impl<S>::send_cmd_copychunk(
		S &s, U32 msgid, int chunkno, const char *chunk, int clen)
{
	char buf[2048 + clen * 2];
	PUSH_TEXT_START(buf, cmd_copychunk);
	PUSH_TEXT_NUM(msgid);
	PUSH_TEXT_NUM(chunkno);
	__buf += hexdump(__buf, sizeof(buf), chunk, clen);
	return s.senddata(msgid, buf, PUSH_TEXT_LEN());
}

template <class S>
int shelld::protocol_impl<S>::send_cmd_exec(S &s, U32 msgid, const char *cmd)
{
	char buf[2048];
	PUSH_TEXT_START(buf, cmd_exec)
	PUSH_TEXT_NUM(msgid);
	PUSH_TEXT_STR(cmd);
	return s.senddata(msgid, buf, PUSH_TEXT_LEN());
}

template <class S>
int shelld::protocol_impl<S>::send_code_list(S &s,
		U32 msgid, const char *hostdata[], int n_host)
{
	char buf[2048 + n_host * 1024];
	PUSH_TEXT_START(buf, code_list);
	PUSH_TEXT_NUM(msgid);
	PUSH_TEXT_NUM(n_host);
	for (int i = 0; i < n_host; i++) {
		PUSH_TEXT_STR(hostdata[i]);
	}
	return s.senddata(msgid, buf, PUSH_TEXT_LEN());
}

template <class S>
int shelld::protocol_impl<S>::send_code_copyinit(S &s,
		U32 msgid, const char *result)
{
	char buf[2048];
	PUSH_TEXT_START(buf, code_copyinit);
	PUSH_TEXT_NUM(msgid);
	PUSH_TEXT_STR(result);
	return s.senddata(msgid, buf, PUSH_TEXT_LEN());
}

template <class S>
int shelld::protocol_impl<S>::send_code_copychunk(S &s,
		U32 msgid, const char *result)
{
	char buf[2048];
	PUSH_TEXT_START(buf, code_copychunk);
	PUSH_TEXT_NUM(msgid);
	PUSH_TEXT_STR(result);
	return s.senddata(msgid, buf, PUSH_TEXT_LEN());
}

template <class S>
int shelld::protocol_impl<S>::send_code_exec_start(S &s,
		U32 msgid, const char *result)
{
	char buf[2048];
	PUSH_TEXT_START(buf, code_exec_start);
	PUSH_TEXT_NUM(msgid);
	PUSH_TEXT_STR(result);
	return s.senddata(msgid, buf, PUSH_TEXT_LEN());
}

template <class S>
int shelld::protocol_impl<S>::send_code_exec_result(S &s,
		U32 msgid, const char *line)
{
	char buf[2048];
	PUSH_TEXT_START(buf, code_exec_result);
	PUSH_TEXT_NUM(msgid);
	PUSH_TEXT_STR(line);
	TRACE("send<%s>\n", buf);
	return s.senddata(msgid, buf, PUSH_TEXT_LEN());
}

template <class S>
int shelld::protocol_impl<S>::send_code_exec_end(S &s,
		U32 msgid, const char *result)
{
	char buf[2048];
	PUSH_TEXT_START(buf, code_exec_end);
	PUSH_TEXT_NUM(msgid);
	PUSH_TEXT_STR(result);
	return s.senddata(msgid, buf, PUSH_TEXT_LEN());
}

template <class S>
int shelld::protocol_impl<S>::on_recv(S &s, char *p, int l)
{
	char cmd[256], path[256], line[1024];
	int n1, n2, n3;
	U32 msgid;
	POP_TEXT_START(p, l);
	POP_TEXT_STR(cmd, sizeof(cmd));
	POP_TEXT_NUM(msgid,U32);
	if (cmp(cmd, cmd_list)) {
		s.recv_cmd_list(msgid);
	}
	else if (cmp(cmd, cmd_copyinit)) {
		POP_TEXT_STR(path, sizeof(path));
		POP_TEXT_NUM(n1,int);
		POP_TEXT_NUM(n2,int);
		s.recv_cmd_copyinit(msgid, path, n1, n2);
	}
	else if (cmp(cmd, cmd_copychunk)) {
		POP_TEXT_NUM(n1,int);
		POP_TEXT_NUM(n2,int);
		char chunk[n2 * 2];
		POP_TEXT_STR(chunk, n3);
		s.recv_cmd_copychunk(msgid, n1, chunk, n3);
	}
	else if (cmp(cmd, cmd_exec)) {
		n1 = sizeof(line);
		POP_TEXT_STRLOW(line, n1, "");
		s.recv_cmd_exec(msgid, line);
	}
	else if (cmp(cmd, code_list)) {
		POP_TEXT_NUM(n1,int);
		char hostdata[n1][1024];
		const char *p_hostdata[n1];
		for (int i = 0; i < n1; i++) {
			POP_TEXT_STR(hostdata[i], sizeof(hostdata[i]));
			p_hostdata[i] = &(hostdata[i][0]);
		}
		s.recv_code_list(msgid, n1, p_hostdata);
	}
	else if (cmp(cmd, code_copyinit)) {
		POP_TEXT_STR(path, sizeof(path));
		s.recv_code_copyinit(msgid, path);
	}
	else if (cmp(cmd, code_copychunk)) {
		POP_TEXT_STR(path, sizeof(path));
		s.recv_code_copychunk(msgid, path);
	}
	else if (cmp(cmd, code_exec_start)) {
		POP_TEXT_STR(path, sizeof(path));
		s.recv_code_exec_start(msgid, path);
	}
	else if (cmp(cmd, code_exec_result)) {
		POP_TEXT_STR(line, sizeof(line));
		s.recv_code_exec_result(msgid, line);
	}
	else if (cmp(cmd, code_exec_end)) {
		POP_TEXT_STR(path, sizeof(path));
		s.recv_code_exec_end(msgid, path);
	}
	else if (cmp(cmd, "*")) {
		/* cluster_broadcast */
		n1 = sizeof(line);
		POP_TEXT_STRLOW(line, n1, "");
		s.recv_cmd_bcast(msgid, line, n1);	/* add last null ch */
	}
	else {
		/* unicast */
		n1 = sizeof(line);
		POP_TEXT_STRLOW(line, n1, "");
		s.recv_cmd_unicast(msgid, cmd, line, n1); /* add last null ch */
	}
	return NBR_OK;
}



/*-------------------------------------------------------------*/
/* shelld::shellclient										   */
/*-------------------------------------------------------------*/
session::pollret shelld::shellclient::poll(UTIME ut, bool thread)
{
#if defined(_DEBUG)
//	log(INFO, "shellclient: poll (%s:%s:%u)\n",
//			valid() ? "valid" : "invalid",
//			thread ? "from thread" : "from main",
//			writable());
#endif
	return servant::poll(ut, thread);
}

int shelld::shellclient::on_open(const config &cfg)
{
	return send_cmd_exec(*this, f()->msgid(), m_cmd);
}

void shelld::shellclient::setcmd(const char *cmd)
{
	nbr_str_copy(m_cmd, sizeof(m_cmd), cmd, sizeof(m_cmd));
}

void shelld::shellclient::recv_code_exec_start(U32 msgid, const char *result)
{
	log(INFO, "exec start: %u(%s)\n", msgid, result);
}

void shelld::shellclient::recv_code_exec_end(U32 msgid, const char *result)
{
	log(INFO, "exec finish: %u(%s)\n", msgid, result);
}

void shelld::shellclient::recv_code_exec_result(U32 msgid, const char *line)
{
	log(INFO, "output>%s\n", line);
}


/*-------------------------------------------------------------*/
/* shelld::shellserver										   */
/*-------------------------------------------------------------*/
session::pollret shelld::shellserver::poll(UTIME ut, bool thread)
{
#if defined(_DEBUG)
//	log(INFO, "shellserver: poll (%s:%s:%u)\n",
//			valid() ? "valid" : "invalid",
//			thread ? "from thread" : "from main",
//			writable());
#endif
	return master::poll(ut, thread);
}

void shelld::shellserver::recv_cmd_exec(U32 msgid, const char *cmd)
{
	log(INFO, "cmd_exec: %s(%u)\n", cmd, msgid);
	shelld::exec_ctx *ctx = shelld::allocator().create();
	if (!ctx) {
		send_code_exec_start(*this, msgid, "resource expire");
		return;
	}
	ctx->msgid = msgid;
	nbr_str_copy(ctx->cmd, sizeof(ctx->cmd), cmd, sizeof(ctx->cmd));
	ctx->s = this;
	ctx->alive = 1;
	shelld::addjob<shellserver>(ctx);
}

void shelld::shell_ucaster::recv_cmd_exec(U32 msgid, const char *cmd)
{
	daemon::log(INFO, "cmd_exec: %s(%u)\n", cmd, msgid);
	ASSERT(strlen(cmd) <= 6);
	shelld::exec_ctx *ctx = shelld::allocator().create();
	if (!ctx) {
		send_code_exec_start(*this, msgid, "resource expire");
		return;
	}
	ctx->msgid = msgid;
	nbr_str_copy(ctx->cmd, sizeof(ctx->cmd), cmd, sizeof(ctx->cmd));
	ctx->uc = *this;
	ctx->alive = 1;
	shelld::addjob<shell_ucaster>(ctx);
}

void shelld::shellserver::recv_cmd_bcast(U32 msgid, const char *cmd, int cmdl)
{
	 if (master_node()) {
		 master_shell::super *sp = master_shell::cluster_conn_from(f());
		 sp->bcaster(this).senddata(msgid, cmd, cmdl);
	 }
	 else {
		 servant_shell::super *sp = servant_shell::cluster_conn_from(f());
		 sp->bcaster(this).senddata(msgid, cmd, cmdl);
	 }
}

void shelld::shellserver::recv_cmd_unicast(
		U32 msgid, const char *addr, const char *cmd, int cmdl)
{
	shell_ucaster *shc;
	if (master_node()) {
		master_shell::super *sp = master_shell::cluster_conn_from(f());
		ucast_sender uc = sp->ucaster(addr, this);
		shc = (shell_ucaster *)(&uc);
	}
	else {
		servant_shell::super *sp = servant_shell::cluster_conn_from(f());
		ucast_sender uc = sp->ucaster(addr, this);
		shc = (shell_ucaster *)(&uc);
	}
	shc->senddata(msgid, cmd, cmdl);
}



/*-------------------------------------------------------------*/
/* shelld													   */
/*-------------------------------------------------------------*/
THPOOL shelld::m_job = NULL;
array<shelld::exec_ctx> shelld::m_el;

factory *
shelld::create_factory(const char *sname)
{
	TRACE("create_factory: sname=<%s>\n", sname);
	if (config::cmp(sname, "sv")) {
		return new master_shell;
	}
	if (config::cmp(sname, "cl")) {
		/* this shellserver means, shellserver feature + cluster node */
		return new servant_shell;
	}
	return NULL;
}

int
shelld::create_config(config *cl[], int size)
{
	if (size <= 1) {
		return NBR_ESHORT;
	}
	cl[1] = new master_shell::property (
				"sv",
				"localhost:12345",
				5,	/* 5 connection expandable */
				60, opt_expandable,
				256 * 1024, 256 * 1024,
				10 * 1000 * 1000, 2 * 1000 * 1000,	/* 10sec timeout, 2sec ping intv */
				1000, sizeof(node::querydata),	/* 1000 query buffer, size is auto generated */
				"TCP", "eth0",
				1 * 1000 * 1000/* 1msec task span */,
				10/* after 10ms, again try to connect */,
				kernel::INFO,
				nbr_sock_rparser_bin32,
				nbr_sock_send_bin32,
				config::cfg_flag_not_set,
				"shell", 2,	-1, /* finder sym is 'shell' and multiplexity = 2
								 packet backup size is auto decided */
				12345, 54321,	/* master port = 12345, servant = 54321 */
				config("for_mstr",	/* server for master:config */
						"0.0.0.0:12345",
						5,	/* 5 connection expandable */
						60, opt_expandable,
						256 * 1024, 256 * 1024,
						10 * 1000 * 1000, 0,	/* 10sec timeout, no ping sent */
						-1,0,	/* no query buffer */
						"TCP", "eth0",
						1 * 1000 * 1000/* 1msec task span */,
						10/* after 10ms, again try to connect */,
						kernel::INFO,
						nbr_sock_rparser_bin32,
						nbr_sock_send_bin32,
						config::cfg_flag_server),
				config("for_svnt",	/* server for servant:config */
						"0.0.0.0:54321",
						5,	/* 5 connection expandable */
						60, opt_expandable,
						256 * 1024, 256 * 1024,
						10 * 1000 * 1000, 0,	/* 10sec timeout, no ping sent */
						-1,0,	/* no query buffer */
						"TCP", "eth0",
						1 * 1000 * 1000/* 1msec task span */,
						10/* after 10ms, again try to connect */,
						kernel::INFO,
						nbr_sock_rparser_bin32,
						nbr_sock_send_bin32,
						config::cfg_flag_server)
			);
	cl[0] = new servant_shell::property (
				"cl",
				"localhost:12345",
				10,	/* 10 connection fix */
				60, opt_not_set,
				256 * 1024, 256 * 1024,
				10 * 1000 * 1000, 2 * 1000 * 1000,	/* 10sec timeout, 2sec ping intv */
				1000,sizeof(node::querydata),	/* 1000 query buffer,size is auto generated */
				"TCP", "eth0",
				1 * 1000 * 1000/* 10msec task span */,
				0/* never wait ld recovery */,
				kernel::INFO,
				nbr_sock_rparser_bin32,
				nbr_sock_send_bin32,
				config::cfg_flag_not_set,
				"shell", 2,	-1 /* finder sym is 'shell' and multiplexity = 2 */,
				config("for_client",	/* server for client:config */
						"0.0.0.0:23456",
						5,	/* 5 connection expandable */
						60, opt_expandable,
						256 * 1024, 256 * 1024,
						10 * 1000 * 1000, 0,	/* 10sec timeout, no ping sent */
						-1,0,	/* no query buffer */
						"TCP", "eth0",
						1 * 1000 * 1000/* 1msec task span */,
						10/* after 10ms, again try to connect */,
						kernel::INFO,
						nbr_sock_rparser_text,
						nbr_sock_send_text,
						config::cfg_flag_server)
			);
	return 2;
}

int
shelld::initlib(CONFIG &c)
{
	c.ioc.job_idle_sleep_us = 10;
	return NBR_OK;
}

template <class S> int
shelld::addjob(exec_ctx *ctx)
{
	return nbr_thpool_addjob(m_job, (void *)ctx, popen_job<S>);
}

template <class S>
S *get_from_context(shelld::exec_ctx *ctx)
{
	return (S *)ctx->s;
}
template <>
shelld::shell_ucaster *get_from_context(shelld::exec_ctx *ctx)
{
	return &(ctx->uc);
}

template <class S> void *
shelld::popen_job(void *p)
{
	exec_ctx *c = (exec_ctx *)p;
	S *s = get_from_context<S>(c);
	FILE *fp = popen(c->cmd, "r");
	const char *r = "ok";
	bool start_sent = false;
	if (!fp) {
		r = "cannot open process";
		goto end;
	}
	if (s->send_code_exec_start(*s, c->msgid, r) < 0) {
		r = "packet send fail";
		goto end;
	}
	start_sent = true;
	char buffer[1024], *str;
	while (c->alive && fgets(buffer, sizeof(buffer), fp)) {
		int wait = 0;
		while (c->alive && wait < 100 && sizeof(buffer) > (size_t)s->writable()) {
			nbr_osdep_sleep(10 * 1000 * 1000/* 10ms */);
			wait++;
			nbr_thread_testcancel();
		}
		if (!(str = nbr_str_chop(buffer))) {
			continue;
		}
		TRACE("result = <%s>\n", str);
		if (s->send_code_exec_result(*s, c->msgid, str) < 0) {
			r = "packet send fail at popen result";
			goto end;
		}
		nbr_thread_testcancel();
	}
end:
	if (start_sent) {
		s->send_code_exec_end(*s, c->msgid, r);
	}
	else {
		s->send_code_exec_start(*s, c->msgid, r);
	}
	if (fp) {
		pclose(fp);
	}
	return NULL;
}

int
shelld::boot(int argc, char *argv[])
{
	if (!m_job) {
		if (!(m_job = nbr_thpool_create(3))) {
			log(ERROR, "cannot create thread pool\n");
			return NBR_EPTHREAD;
		}
		int r;
		if ((r = nbr_thpool_init_jobqueue(m_job, 30)) < 0) {
			log(ERROR, "cannot init jobqueue (%d)\n", r);
			return r;
		}
	}
	if (!m_el.initialized()) {
		m_el.init(100, -1, opt_threadsafe);
	}
	return NBR_OK;
}

void
shelld::shutdown()
{
	if (m_job) {
		nbr_thpool_destroy(m_job);
		m_job = NULL;
	}
	if (m_el.initialized()) {
		m_el.fin();
	}
}


