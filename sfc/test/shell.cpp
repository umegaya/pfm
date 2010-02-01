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
		debug_big_data[1024] = '\0';
	}
	/* disabled ping? */
	if (s.cfg().m_ping_timeo <= 0) { return NBR_OK; }
	char work[64 + debug_big_datasize + 1];
	PUSH_TEXT_START(work, cmd_ping);
	if (s.cfg().client()) {
		ut = nbr_time();
	}
	PUSH_TEXT_BIGNUM(ut);
	PUSH_TEXT_STR(debug_big_data);
#if 1//defined(_DEBUG)
	s.log(kernel::INFO, "sendping: at %llu\n", ut);
#endif
	return s.send(work, PUSH_TEXT_LEN());
}

int shelld::protocol::recvping(class session &s, char *p, int l)
{
	if (s.cfg().m_ping_timeo <= 0) { return NBR_OK; }
	if (*p != '0') {
		return NBR_OK; /* no ping */
	}
	/* disabled ping? */
	char cmd[16 + 1];
	char dbd[debug_big_datasize + 1];
	U64 ut;
	POP_TEXT_START(p, l);
	POP_TEXT_STR(cmd, sizeof(cmd));
	POP_TEXT_BIGNUM(ut, U64);
	POP_TEXT_STR(dbd, sizeof(dbd));
	TRACE("first 16 char of dbd is :");
	for (int i = 0; i < 16; i++) {
		TRACE("%c", dbd[i]);
	}
	TRACE("\n");
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
	return s.send(buf, PUSH_TEXT_LEN());
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
		POP_TEXT_STR(path, sizeof(path));
		s.recv_cmd_exec(msgid, path);
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
	m_ctx.msgid = msgid;
	nbr_str_copy(m_ctx.cmd, sizeof(m_ctx.cmd), cmd, sizeof(m_ctx.cmd));
	m_ctx.s = this;
	m_ctx.alive = 1;
	shelld::addjob(&m_ctx);
}


/*-------------------------------------------------------------*/
/* shelld													   */
/*-------------------------------------------------------------*/
THPOOL shelld::m_job = NULL;
session::factory *
shelld::create_factory(const char *sname)
{
	TRACE("create_factory: sname=<%s>\n", sname);
	if (config::cmp(sname, "sv")) {
#if defined(_CLUSTER_)
		return new mastersession::factory_impl<shellserver>;
#else
		return new shellserver::factory;
#endif
	}
	if (config::cmp(sname, "cl")) {
#if defined(_CLUSTER_)
		return new servantsession::factory_impl<shellserver>;
#else
		return new shellclient::factory;
#endif
	}
	return NULL;
}

int
shelld::create_config(config *cl[], int size)
{
	if (size <= 1) {
		return NBR_ESHORT;
	}
	cl[0] = new shellclient::property (
			"cl",
			"localhost:12345",
			5,	/* 5 connection expandable */
			60, opt_expandable,
			1025 * 1024, 1025 * 1024,
			2, 10,	/* no ping */
			-1,0,	/* no query buffer */
			"TCP",
			1 * 1000 * 1000/* 1msec task span */,
			10/* after 10ms, again try to connect */,
			kernel::INFO,
			nbr_sock_rparser_text,
			nbr_sock_send_text,
			config::cfg_flag_not_set
			);
	cl[1] = new shellserver::property (
			"sv",
			"0.0.0.0:12345",
			10,	/* 10 connection fix */
			60, opt_not_set,
			1025 * 1024, 1025 * 1024,
			2, 10,	/* no ping */
			-1,0,	/* no query buffer */
			"TCP",
			1 * 1000 * 1000/* 10msec task span */,
			0/* never wait ld recovery */,
			kernel::INFO,
			nbr_sock_rparser_text,
			nbr_sock_send_text,
			config::cfg_flag_server
			);
	return 2;
}

int
shelld::initlib(CONFIG &c)
{
	c.ioc.job_idle_sleep_us = 10;
	return NBR_OK;
}

int
shelld::addjob(shellserver::exec_ctx *ctx)
{
	return nbr_thpool_addjob(m_job, (void *)ctx, shelld::popen_job);
}

void *
shelld::popen_job(void *p)
{
	shellserver::exec_ctx *c = (shellserver::exec_ctx *)p;
	shellserver *s = c->s;
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
	shellserver::factory *sv = find_factory<shellserver::factory>("sv");
	shellclient::factory *cl = find_factory<shellclient::factory>("cl");
	if (sv) {
		if (cl) {
			log(ERROR, "client should be disabled\n");
			return NBR_ECONFIGURE;
		}
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
		return NBR_OK;
	}
	if (!cl) {
		log(ERROR, "client should be activated\n");
		return NBR_ECONFIGURE;
	}
	shellclient *c = cl->pool().alloc();
	if (!c) {
		log(ERROR, "cannot allocate\n");
		return NBR_EEXPIRE;
	}
	int r;
	c->setcmd("ls");
	if ((r = cl->connect(c)) < 0) {
		log(ERROR, "fail to connect (%d)\n", r);
		return r;
	}
	log(INFO, "connecting... %s\n", cl->cfg().m_host);
	return NBR_OK;
}

void
shelld::shutdown()
{
	if (m_job) {
		nbr_thpool_destroy(m_job);
		m_job = NULL;
	}
}


