/****************************************************************
 * sock.c : for testing core/sock.c
 * 2009/10/24 iyatomi : create
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
#include "common.h"
#include "tutil.h"
#include "nbr_pkt.h"
#include "nbr_plg.h"
#include <sys/resource.h>
#if defined(USE_TT_TEST)
#include "tcrdb.h"
#include "tculog.h"
static TCADB *g_adb = NULL;
#endif

static int alive = 1;
static int error_result = 0;
static int verbose = 0;
static int N_CLIENT = 800;
static int N_CLIENT_GROUP = 1;
#define VTRACE(...) if (verbose) { TRACE(__VA_ARGS__); }


/* callback functions */
typedef struct workbuf
{
	char addr[256];
} workbuf_t;
static int acceptwatcher(SOCK sk)
{
	int len;
	char addr[256];
	workbuf_t *wkb;
	nbr_sock_get_addr(sk, addr, sizeof(addr));
	TRACE("sv: access from %s\n", addr);
	wkb = nbr_sock_get_data(sk, &len);
	if (nbr_sock_get_addr(sk, addr, sizeof(addr)) >= 0) {
		strcpy(wkb->addr, addr);
	}
	return 0;
}

static int closewatcher(SOCK sk, int reason)
{
	int len;
	char addr[256];
	workbuf_t *wkb;
	nbr_sock_get_addr(sk, addr, sizeof(addr));
	ASSERT(addr[0] != 0);
	if (reason == CLOSED_BY_REMOTE) {
		VTRACE("sv: disconnect by remote node %s(%d)\n", addr, reason);
	}
	else {
		VTRACE("sv: close connection to %s(%d)\n", addr, reason);
	}
	wkb = nbr_sock_get_data(sk, &len);
	if (strcmp(wkb->addr, addr) != 0) {
		TRACE("sv: error: databuf wrong: <%s><%s>\n", wkb->addr, addr);
		alive = 0;
		error_result = NBR_EINVAL;
	}
	return 0;
}

static int sendresult1(SOCK sk, int number)
{
	char buffer[32];
	U8 type = 0;
	int prm = _prime(number);
	PUSH_START(buffer, sizeof(buffer));
	PUSH_8(type);
	PUSH_32(number);
	PUSH_32(prm);
	VTRACE("sv: sendresult1: number = %u, prime = %u\n", number, prm);
	return nbr_sock_send_bin32(sk, buffer, PUSH_LEN());
}

static int sendresult2(SOCK sk, U32 msgid, const char *b)
{
	char buffer[1024];
	U8 type = 1;
	PUSH_START(buffer, sizeof(buffer));
	PUSH_8(type);
	PUSH_32(msgid);
	PUSH_STR(b);
	VTRACE("sv: sendresult2:(%d)\n", msgid);
	return nbr_sock_send_bin32(sk, buffer, PUSH_LEN());
}

static int sendresult3(SOCK sk, U64 ut)
{
	char buffer[2048], data[256];
	U8 type = 2;
	PUSH_START(buffer, sizeof(buffer));
	PUSH_8(type);
	PUSH_64(ut);
	PUSH_8A(data, sizeof(data));
	VTRACE("sv: sendresult2:(%llu)\n", ut);
	return nbr_sock_send_bin32(sk, buffer, PUSH_LEN());
}

static int parser(SOCK sk, char *p, int l)
{
	U32 number;
	char strbuf[256];
	U8 type;
	U16 val;
	U64 ut;
	int len;
	nbr_sock_get_addr(sk, strbuf, sizeof(strbuf));
	VTRACE("%p: recv %u byte from %s\n", sk.p, l, strbuf);
	POP_START(p, l);
	POP_8(type);
	switch(type) {
	case 0:
		POP_32(number);
		VTRACE("sv: recv packet len:%u, number:%u\n", l, number);
		return sendresult1(sk, number);
	case 1:
		POP_32(number);
		POP_STR(strbuf, sizeof(strbuf));
		return sendresult2(sk, number, strbuf);
	case 2:
		alive = 0;
		POP_32(error_result);
		POP_16(val);
		ASSERT(val == 100);
		TRACE("###### final packet from child process res(%d)\n", error_result);
		return 0;
	case 3:
		POP_64(ut);
		len = sizeof(strbuf);
		POP_8A(strbuf, len);
		return sendresult3(sk, ut);
	}
	alive = 0;
	error_result = NBR_ERANGE;
	return 0;
}

#if defined(USE_TT_TEST)
static char * ttrparser(char *p, int *l, int *r)
{
	U8 magic, cmd;
	int reqlen;
	if (*l < 2) {
		return NULL;
	}
	magic = p[0];
	cmd = p[1];
	if(magic == TTMAGICNUM){
	    switch(cmd){
	    case TTCMDRNUM:
	    case TTCMDSIZE:
	    case TTCMDVANISH:
	      *l = *r = 2;
	      return p;
	    case TTCMDGET:
	      if (*l <= 6) {
	    	  return NULL;
	      }
	      reqlen = (6 + ntohl(GET_32(p + 2)));
	      if (*l < reqlen) {
	    	  return NULL;
	      }
	      *l = *r = reqlen;
	      return p;
	    case TTCMDPUT:
	      if (*l <= 10) {
	    	  return NULL;
	      }
	      int ksz = ntohl(GET_32(p + 2));
	      int vsz = ntohl(GET_32(p + 6));
	      reqlen = 10 + ksz + vsz;
	      if (*l < reqlen) {
	    	  return NULL;
	      }
	      *l = *r = reqlen;
	      return p;

	    case TTCMDPUTKEEP:
	    case TTCMDPUTCAT:
	    case TTCMDPUTSHL:
	    case TTCMDPUTNR:
	    case TTCMDOUT:
	    case TTCMDMGET:
	    case TTCMDVSIZ:
	    case TTCMDITERINIT:
	    case TTCMDITERNEXT:
	    case TTCMDFWMKEYS:
	    case TTCMDADDINT:
	    case TTCMDADDDOUBLE:
	    case TTCMDEXT:
	    case TTCMDSYNC:
	    case TTCMDOPTIMIZE:
	    case TTCMDCOPY:
	    case TTCMDRESTORE:
	    case TTCMDSETMST:
	    case TTCMDSTAT:
	    case TTCMDMISC:
	    case TTCMDREPL:
	      break;
	    }
	}
	return NULL;
}

static int ttsendresult_rnum(SOCK sk, U64 rnum)
{
	char buffer[256];
	U8 res = 0;
	PUSH_START(buffer, sizeof(buffer));
	PUSH_8(res);
	PUSH_64(rnum);
	return nbr_sock_send_raw(sk, buffer, PUSH_LEN());
}

static int ttsendresult_vanish(SOCK sk)
{
	char buffer[256];
	U8 res = tcadbvanish(g_adb) ? 0 : 1;
	PUSH_START(buffer, sizeof(buffer));
	PUSH_8(res);
	return nbr_sock_send_raw(sk, buffer, PUSH_LEN());
}

static int ttsendresult_size(SOCK sk, U64 rnum)
{
	char buffer[256];
	U8 res = 0;
	PUSH_START(buffer, sizeof(buffer));
	PUSH_8(res);
	PUSH_64(rnum);
	return nbr_sock_send_raw(sk, buffer, PUSH_LEN());
}

static int ttsendresult_get(SOCK sk, U8 *v, int vsz)
{
	char buffer[256];
	U8 res = v ? 0 : 1;
	PUSH_START(buffer, sizeof(buffer));
	PUSH_8(res);
	PUSH_32(vsz);
	if (v) {
		PUSH_MEM(v, vsz);
	}
	else {
		VTRACE("v == NULL\n");
	}
	return nbr_sock_send_raw(sk, buffer, PUSH_LEN());
}

static int ttsendresult_put(SOCK sk, U8 *key, int ksz, U8 *val, int vsz)
{
	char buffer[256];
	U8 res = 0;
	if (false == tcadbput(g_adb, key, ksz, val, vsz)) {
		res = 1;
	}
	PUSH_START(buffer, sizeof(buffer));
	PUSH_8(res);
	return nbr_sock_send_raw(sk, buffer, PUSH_LEN());
}

static int ttparser(SOCK sk, char *p, int l)
{
	U32 ksz, vsz;
	U8 key[256], val[256], *pv;
	U8 magic, cmd;
	POP_START(p, l);
	POP_8(magic);
	POP_8(cmd);
	switch(cmd) {
	case TTCMDRNUM:
		return ttsendresult_rnum(sk, tcadbrnum(g_adb));
	case TTCMDSIZE:
		alive = 0;
		error_result = NBR_OK;
		return ttsendresult_size(sk, tcadbsize(g_adb));
	case TTCMDVANISH:
		return ttsendresult_vanish(sk);
	case TTCMDGET:
		POP_32(ksz);
		POP_MEM(key, ksz);
		pv = tcadbget(g_adb, key, ksz, &vsz);
		return ttsendresult_get(sk, pv, vsz);
	case TTCMDPUT:
		POP_32(ksz);
		POP_32(vsz);
		POP_MEM(key, ksz);
		POP_MEM(val, vsz);
		return ttsendresult_put(sk, key, ksz, val, vsz);
	}
	alive = 0;
	error_result = NBR_ERANGE;
	return 0;
}

static BOOL
tc_init()
{
	g_adb = tcadbnew();
	return g_adb && tcadbopen(g_adb, "/home/iyatomi/prj/nbr/test/unit/test.tch");
}
#endif
struct {
	SSLCONF sslc;
} g_confs = {
		{ 1000, NBR_SSL_METHOD_SERVER_V23, NBR_SSL_METHOD_CLIENT_V23,
			"plugin/public.key", "plugin/private.key" },
};

PROTOCOL *proto_regist_by_name(const char *name, void **proto_p)
{
	*proto_p = NULL;
	if (strcmp(name, "SSL") == 0) {
		return nbr_proto_ssl(&(g_confs.sslc));
	}
	else if (strcmp(name, "TCP") == 0) {
		return nbr_proto_tcp();
	}
	else if (strcmp(name, "UDP") == 0) {
		return nbr_proto_udp();
	}
	return NULL;
}

BOOL
nbr_sock_test(int max_thread, int max_client, int exec_dur, int max_query, char *proto)
{
	char *cmd = "test/module/sockcli.elf";
	char *argv[] = {
		"localhost:1978",
		"3",	/* 3 thread */
		"10",	/* execute 10 sec */
		"2",	/* calculate prime */
		proto,	/*proto*/
		"800",	/* max_client */
		"128000",/* max query */
		NULL,
	};
	time_t tm_start;
	int cpid, f_udp;
	CONFIG c;
	char buf_thnum[256], buf_exedur[256], buf_max_client[256], buf_max_query[256];
	struct rlimit rl;
	void *proto_p;
	PROTOCOL *proto_if;

	nbr_get_default(&c);
	c.max_worker = max_thread;
	c.sockbuf_size = 24 * 1024 * 1024;
	N_CLIENT = max_client;
	N_CLIENT_GROUP = (N_CLIENT / 1000) + 1;
	if (nbr_init(&c) != NBR_OK) {
		return -2;
	}
	rl.rlim_cur = RLIM_INFINITY;
	rl.rlim_max = RLIM_INFINITY;
	setrlimit(RLIMIT_CORE, &rl);

	f_udp = (strcmp(proto, "UDP") == 0 ? 1 : 0);

	proto_if = proto_regist_by_name(proto, &proto_p);
	/* server */
	SOCKMGR skm = nbr_sockmgr_create(8 * 1024, 8 * 1024,
						N_CLIENT + 1,
						sizeof(workbuf_t),
						f_udp ? 10 : 500 * 1000,
						"0.0.0.0:1978",
						proto_if, proto_p,
						0/* non-expandable */);
	if (skm == NULL) {
		TRACE("fail to create skm\n");
		return FALSE;
	}

	snprintf(buf_thnum, sizeof(buf_thnum), "%u", max_thread);
	argv[1] = buf_thnum;
	snprintf(buf_exedur, sizeof(buf_exedur), "%u", exec_dur);
	argv[2] = buf_exedur;
	snprintf(buf_max_client, sizeof(buf_exedur), "%u", max_client);
	argv[5] = buf_max_client;
	snprintf(buf_max_query, sizeof(buf_max_query), "%u", max_query);
	argv[6] = buf_max_query;


#if defined(USE_TT_TEST)
	nbr_sockmgr_set_callback(skm,
			ttrparser,
			acceptwatcher,
			closewatcher,
			ttparser,
			NULL, NULL);
	if (false == tc_init()) {
		return FALSE;
	}
#else
	nbr_sockmgr_set_callback(skm,
			nbr_sock_rparser_bin32,
			acceptwatcher,
			closewatcher,
			parser,
			NULL, NULL);
	if ((cpid = nbr_osdep_fork(cmd, argv, NULL)) < 0) {
		TRACE("fail to invoke client process\n");
		return FALSE;
	}
	TRACE("child process invoked: pid = %u\n", cpid);
#endif
	tm_start = time(NULL);
	alive = 1;
	while(alive) {
		nbr_poll();
		if ((tm_start + exec_dur + 12) < time(NULL)) {
			TRACE("nbr_sock_test: error timeout\n");
			error_result = NBR_ETIMEOUT;
			break;
		}
	}

#if defined(USE_TT_TEST)
	if (g_adb) {
		TRACE("cleanup TCADB\n");
		tcadbclose(g_adb);
		tcadbdel(g_adb);
	}
#endif
	printf("cleanup nbr (%d)\n", error_result);
	nbr_fin();
	return error_result == 0;
}
