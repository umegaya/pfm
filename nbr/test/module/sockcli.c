/****************************************************************
 * sockcli.c : stub client for testing sock.c
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

static int alive = 1;
static int emerg = 0;
static int numhandle = 0;
static time_t tm_start = 0;
static int verbose = 0;
static int N_CLIENT = 800, N_CLIENT_GROUP_SIZE = 1000;
static int N_CLIENT_GROUP = 1, GROUP_INDEX = 0;
static ARRAY g_array = NULL;
static SEARCH g_search = NULL;
static int msgid_seed = 0;
static int g_accept_num = 0;
SOCK *ska;
int *sent;

#define VTRACE(...) if (verbose) { TRACE(__VA_ARGS__); }

/* callback functions */
typedef struct workbuf
{
	U8 sent;
	U8 type;
	U32 number;
	U32 index;
	char str[256];
} workbuf_t;
static int acceptwatcher(SOCK sk)
{
	char addr[256]; int i;
	workbuf_t *wkb;
	nbr_sock_get_addr(sk, addr, sizeof(addr));
	TRACE("cl: connect to %s\n", addr);
	for (i = 0; i < N_CLIENT; i++) {
		if (nbr_sock_is_same(sk, ska[i])) {
			TRACE("this sock(%p,%u) is for index %u\n", sk.p, sk.s, i);
			wkb = (workbuf_t *)nbr_sock_get_data(sk, NULL);
			ASSERT(wkb);
			wkb->index = i;
		}
	}
	g_accept_num++;
	return 0;
}

static int entry_query(int id, const char *b)
{
	workbuf_t *wkb = nbr_array_alloc(g_array);
	if (wkb == NULL) { ASSERT(FALSE); return -1; }
	nbr_str_copy(wkb->str, sizeof(wkb->str), b, sizeof(wkb->str) - 1);
	wkb->str[sizeof(wkb->str) - 1] = '\0';
	return nbr_search_int_regist(g_search, id, wkb);
}

static int unentry_and_check_query(int id, const char *b)
{
	workbuf_t *wkb = nbr_search_int_get(g_search, id);
	if (wkb == NULL) { ASSERT(FALSE);return -1; }
	if (nbr_str_cmp(wkb->str, sizeof(wkb->str), b, 256) != 0) {
		ASSERT(FALSE);
		nbr_search_int_unregist(g_search, id);
		nbr_array_free(g_array, wkb);
		return -1;
	}
	nbr_search_int_unregist(g_search, id);
	nbr_array_free(g_array, wkb);
	return 0;
}

static int get_msgid()
{
	if (msgid_seed > 2000000000) {
		msgid_seed = 0;
	}
	msgid_seed++;
	return msgid_seed;
}

static int closewatcher(SOCK sk, int reason)
{
	char addr[256];
	int len;
	workbuf_t *wkb;
	nbr_sock_get_addr(sk, addr, sizeof(addr));
	wkb = nbr_sock_get_data(sk, &len);
	ASSERT(addr[0] != 0 && sizeof(*wkb) == len);
	if (reason == CLOSED_BY_REMOTE) {
		TRACE("cl: disconnect by remote node %s(%d)\n",addr, reason);
	}
	else {
		TRACE("cl: close connection to %s(%d)\n",addr, reason);
	}
	sent[wkb->index] = 0;
	return 0;
}

static int sendpacket1(SOCK sk, int number)
{
	char buffer[32];
	U8 type = 0;
	PUSH_START(buffer, sizeof(buffer));
	PUSH_8(type);
	PUSH_32(number);
	TRACE("cl: sendpacket1: number = %u\n", number);
	return nbr_sock_send_bin32(sk, buffer, PUSH_LEN());
}

static int sendpacket2(SOCK sk, U32 msgid, const char *b)
{
	char buffer[1024];
	U8 type = 1;
	int r;
	PUSH_START(buffer, sizeof(buffer));
	PUSH_8(type);
	PUSH_32(msgid);
	PUSH_STR(b);
	if ((r = entry_query(msgid, b)) < 0) {
		TRACE("cl: sendpacket2: entry_query fail(%d/%d)\n", msgid, r);
		ASSERT(FALSE);
		return -1;
	}
	VTRACE("cl: sendpacket2:(%d)\n", msgid);
	return nbr_sock_send_bin32(sk, buffer, PUSH_LEN());
}

static int sendpacket3(SOCK sk, int error_reason)
{
	char buffer[32];
	U8 type = 2;
	U16 val = 100;
	PUSH_START(buffer, sizeof(buffer));
	PUSH_8(type);
	PUSH_32(error_reason);
	PUSH_16(val);
	TRACE("sendpacket3: err=<%d>\n", error_reason);
	return nbr_sock_send_bin32(sk, buffer, PUSH_LEN());
}

static int sendpacket4(SOCK sk)
{
	char buffer[1024], str[32];
	U8 type = 3;
	U64 ut = nbr_time();
	PUSH_START(buffer, sizeof(buffer));
	PUSH_8(type);
	PUSH_64(ut);
	PUSH_8A(str, sizeof(str));
	return nbr_sock_send_bin32(sk, buffer, PUSH_LEN());
}

static U64 g_total_rtt = 0LL;
static int parser(SOCK sk, char *p, int l)
{
	workbuf_t *wkb;
	U32 prime, number;
	char strbuf[256];
	int len, msgid;
	U8 type;
	UTIME ut;
	wkb = nbr_sock_get_data(sk, &len);
	VTRACE("cl: recv: len=%u,%p\n", l, wkb);
	POP_START(p, l);
	POP_8(type);
	if (len != sizeof(*wkb)) {
		TRACE("cl: sock buffer size differ %u %u\n", len, (U32)sizeof(wkb));
		alive = 0;
		emerg = NBR_EINVAL;
		return 0;
	}
	if (type != wkb->type) {
		TRACE("cl: recv result type differ %u %u\n", type, wkb->type);
		alive = 0;
		emerg = NBR_EINVAL;
		return 0;
	}
	switch(type) {
	case 0:
		POP_32(number);
		POP_32(prime);
//		prime2 = _prime(number);
//		if (prime2 != prime) {
//			VTRACE("cl: prime different: %u %u\n", prime, prime2);
//			alive = 0;
//			emerg = NBR_EINVAL;
//			return 0;
//		}
		TRACE("cl: result prime:%u for %u\n", prime, wkb->number);
		numhandle++;
		break;
	case 1:
		POP_32(msgid);
		POP_STR(strbuf, sizeof(strbuf));
#if 1
		if (unentry_and_check_query(msgid, strbuf) < 0) {
			printf("cl: str different: <%s><%d>\n", strbuf, msgid);
#else
		if (strcmp(strbuf, wkb->str) != 0) {
			VTRACE("cl: str different: <%s><%s>\n", strbuf, wkb->str);
#endif
			alive = 0;
			emerg = NBR_EINVAL;
			return 0;
		}
		numhandle++;
		break;
	case 2:
		POP_64(ut);
//		TRACE("nbr_time = %llu (%llu)\n", nbr_time(), ut);
		g_total_rtt += (nbr_time() - ut);
		numhandle++;
		break;
	}
//	TRACE("sock reuse %p:%u:%u\n", sk.p, sk.s, wkb->index);
//	nbr_sock_close(sk);
	sent[wkb->index]++;
	return 0;
}

static int eventhandler(SOCK sk, char *p, int l)
{
	int len;
	U32 number;
	workbuf_t *wkb = nbr_sock_get_data(sk, &len);
	VTRACE("event handler %ubyte(%p)\n", l, wkb);
	POP_START(p, l);
	POP_8(wkb->type);
	switch(wkb->type) {
	case 0:
		POP_32(wkb->number);
		return sendpacket1(sk, wkb->number);
	case 1:
		POP_STR(wkb->str, sizeof(wkb->str));
		POP_32(number);
		VTRACE("wkb->str=<%s>\n", wkb->str);
		if (nbr_sock_writable(sk) <= 264) {
			TRACE("write buf almost full: remain %d\n", nbr_sock_writable(sk));
			return 1;
		}
//		len = sprintf(work, "#%u#", number);
//		nbr_mem_copy(wkb->str, work, len);
//		TRACE("str~<%s>\n", wkb->str);
		if (sendpacket2(sk, number, wkb->str) < 0) {
			printf("query expire: %d %d\n", nbr_array_use(g_array), nbr_array_max(g_array));
			alive = 0;
			emerg = NBR_EEXPIRE;
			ASSERT(FALSE);
			return -1;
		}
		return 1;
	case 2:
		if (nbr_sock_writable(sk) <= 12) {
			TRACE("write buf almost full: remain %d\n", nbr_sock_writable(sk));
			return 1;
		}
		if (sendpacket4(sk) < 0) {
			emerg = NBR_EEXPIRE;
			ASSERT(FALSE);
			return -1;
		}
		return 1;
	}
	ASSERT(FALSE);
	return 0;
}

static int sendevent(SOCK sk, U8 type)
{
	char buffer[1024], strbuf[256];
	U32 number;
	PUSH_START(buffer, sizeof(buffer));
	PUSH_8(type);
	switch(type) {
	case 0:
		number = nbr_rand32() % 1000000 + 1000000;
		PUSH_32(number);
		break;
	case 1:
		write_random_string(strbuf, sizeof(strbuf));
		PUSH_STR(strbuf);
		number = get_msgid();
		PUSH_32(number);
		break;
	case 2:
		break;
	}
	return nbr_sock_event(sk, buffer, PUSH_LEN());
}

static int create_connection(SOCKMGR skm, const char *addr, U8 type)
{
	int i = N_CLIENT_GROUP_SIZE * GROUP_INDEX,
	last = (N_CLIENT_GROUP_SIZE * (GROUP_INDEX + 1));
	if (last > N_CLIENT) {
		last = N_CLIENT;
	}
	for (; i < last; i++) {
		if (!nbr_sock_valid(ska[i])){
//			VTRACE("idx=%u is invalid: create new connection\n", i);
			ASSERT(sent[i] == 0);
			sent[i] = 0;
			ska[i] = nbr_sockmgr_connect(skm, addr, NULL, NULL);
			if (nbr_sock_valid(ska[i])) {
				continue;
			}
		}
		else {
//			if (g_accept_num < N_CLIENT) {
//				continue;
//			}
			if (nbr_sock_writable(ska[i]) > 264 /* && (sent[i] % 2) == 0 */) {
				VTRACE("idx=%u is writable: sendpacket\n", i);
				sendevent(ska[i], type);
				sent[i]++;
			}
			//VTRACE("idx=%u: writable=%u\n", i, nbr_sock_writable(ska[i]));
		}
	}
	GROUP_INDEX++;
	if (GROUP_INDEX >= N_CLIENT_GROUP) {
		GROUP_INDEX = 0;
	}
	return 0;
}

struct {
	SSLCONF sslc;
} g_confs = {
	{ 1000, NBR_SSL_METHOD_SERVER_V23, NBR_SSL_METHOD_CLIENT_V23,
			"plugin/public.key", "plugin/private.key" },
};

PROTOCOL *proto_regist_by_name(const char *name, void **proto_pp)
{
	*proto_pp = NULL;
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

int
main(int argc, char *argv[], char *envp[])
{
	int i, span, type, res_handle, f_udp = 0, max_query;
	CONFIG c;
	SOCK last;
	time_t tm_last_send_start;
	PROTOCOL *prt;
	void *prt_p;
	struct rlimit rl;
	int startidx = 0;
	if (argc < 4) {
		TRACE("argc = %u\n", argc);
		TRACE("usage: sockcli.elf {addr} {max thread} {exec duration} "
				"{query type} {proto} {max client} {max query}\n");
		return -1;
	}
	if (nbr_str_cmp_tail(argv[0], ".elf", 4, 256) == 0) {
		startidx++;	/* skip application name */
	}
	nbr_get_default(&c);
	c.sockbuf_size = 24 * 1024 * 1024;
	c.ndc.mcast_port = 9999;
	if (nbr_str_atoi(argv[startidx + 1], &(c.max_worker), 256) < 0) {
		TRACE("get args1 fail %s\n", argv[1]);
		return -3;
	}
	if (nbr_str_atoi(argv[startidx + 2], &span, 256) < 0) {
		TRACE("get args2 fail %s\n", argv[2]);
		return -4;
	}
	if (nbr_str_atoi(argv[startidx + 3], &type, 256) < 0) {
		TRACE("get args3 fail %s\n", argv[3]);
		return -5;
	}
	if (strcmp("UDP", argv[startidx + 4]) == 0) {
		f_udp = 1;
	}
	if (nbr_str_atoi(argv[startidx + 5], &N_CLIENT, 256) < 0) {
		TRACE("get args5 fail %s\n", argv[5]);
		return -6;
	}
	if (nbr_str_atoi(argv[startidx + 6], &max_query, 256) < 0) {
		TRACE("get args6 fail %s\n", argv[6]);
		return -7;
	}
	N_CLIENT_GROUP = (int)((N_CLIENT / N_CLIENT_GROUP_SIZE) + 1);
	printf("sockcli: param: %s %d %d %d %s %d %d\n",
			argv[startidx], c.max_thread, span, type, argv[startidx + 4], N_CLIENT, max_query);
	if (nbr_init(&c) != NBR_OK) {
		return -2;
	}

	if (!(ska = malloc(sizeof(SOCK) * N_CLIENT))) {
		TRACE("alloc fail: N_CLIENT=%d\n", N_CLIENT);
		return -10;
	}
	if (!(sent = malloc(sizeof(int) * N_CLIENT))) {
		TRACE("alloc fail2: N_CLIENT=%d\n", N_CLIENT);
		return -10;
	}
	prt = proto_regist_by_name(argv[startidx + 4], &prt_p);
	if (!prt) {
		TRACE("protocol registration fail %s\n", argv[startidx + 4]);
		return -10;
	}

	rl.rlim_cur = RLIM_INFINITY;
	rl.rlim_max = RLIM_INFINITY;
	setrlimit(RLIMIT_CORE, &rl);
	/* client */
	SOCKMGR skm = nbr_sockmgr_create(8 * 1024, 32 * 1024,
						N_CLIENT + 1,
						sizeof(workbuf_t),
						f_udp ? 10 : 600,
						NULL,
						prt, prt_p,
						0/* non-expandable */);
	if (skm == NULL) {
		TRACE("fail to create skm\n");
		return -3;
	}
	if (!(g_array = nbr_array_create(max_query, sizeof(workbuf_t), NBR_PRIM_THREADSAFE))) {
		return -4;
	}

	if (!(g_search = nbr_search_init_int_engine(max_query, NBR_PRIM_THREADSAFE, max_query / 10))) {
		return -5;
	}

	nbr_sockmgr_set_callback(skm,
			nbr_sock_rparser_bin32,
			acceptwatcher,
			closewatcher,
			parser,
			eventhandler,
			NULL);

	for (i = 0; i < N_CLIENT; i++) {
		nbr_sock_clear(&(ska[i]));
		sent[i] = 0;
	}

	tm_start = time(NULL);
	while(alive) {
		nbr_poll();
		if (create_connection(skm, argv[startidx], (U8)type) < 0) {
			emerg = NBR_ESHORT;
			break;
		}
		if ((tm_start + span) < time(NULL)) {
			break;
		}
	}

	printf("#######################################################################\n");
	if (emerg == 0) {
		res_handle = numhandle;
		printf("%u query handled in %u sec (%f qps)\n", res_handle, span, (float)((float)res_handle / (float)span));
		if (type == 2) {
			printf("ave latency: %f usec (spend %llu usec)\n",
					(float)((float)g_total_rtt / (float)res_handle),
					g_total_rtt);
		}
	}
	else {
		printf("error happen: e = %d\n", emerg);
	}
	printf("#######################################################################\n");
	tm_start = time(NULL);
	while(1) {
		nbr_poll();
		if ((time(NULL) - tm_start) > 3) {
			break;
		}
	}
	printf("###### sending last result...\n");
	last = nbr_sockmgr_connect(skm, argv[startidx], NULL, NULL);
	tm_last_send_start = time(NULL);
	while(1) {
		if (nbr_sock_writable(last) > 0) {
			if (sendpacket3(last, emerg) < 0) {
				printf("###### error! send\n");
				ASSERT(FALSE);
				break;
			}
			while(nbr_sock_valid(last)) {
				nbr_poll();
				if ((time(NULL) - tm_last_send_start) > 5) {
					printf("###### timeout 1\n");
					goto end;
				}
			}
			printf("###### done\n");
			break;
		}
		else if (!nbr_sock_valid(last)) {
			printf("###### error\n");
			break;
		}
		nbr_poll();
		if ((time(NULL) - tm_last_send_start) > 5) {
			printf("###### timeout 2\n");
			break;
		}
	}

end:
	nbr_fin();
	return emerg == 0;
}
