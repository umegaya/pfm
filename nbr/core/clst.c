/***************************************************************
 * node.c : control master / application cluster
 * 2009/10/27 iyatomi : create
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
#include "clst.h"
#include "proto.h"
#include "sock.h"
#include "nbr_pkt.h"



/*-------------------------------------------------------------*/
/* macro													   */
/*-------------------------------------------------------------*/
#define CLST_ERROUT			NBR_ERROUT
#define CLST_LOG(prio,...)	fprintf(stderr, __VA_ARGS__)



/*-------------------------------------------------------------*/
/* constant													   */
/*-------------------------------------------------------------*/
/* clst status */
enum {
	NST_INVALID,
	/* master clst status */
	NST_M_START,
	NST_M_MULTICAST_TO_FIND,
	NST_M_CONNECTING_MASTER,
	NST_M_CONNECTED_TO_MASTER,
	NST_M_RECEIVEDATA_FROM_MASTER,
	NST_M_CONNECTING_ALL_CLST,
	NST_M_CONNECTED_TO_ALL_CLST,
	/* servant clst status */
	NST_S_START,
	NST_S_MULTICAST_TO_FIND,
	NST_S_CONNECTING_TO_SECONDARY,
	NST_S_CONNECTED_TO_SECONDARY,
	NST_S_CONNECTING_MASTER,
	NST_S_CONNECTED_TO_MASTER,
	NST_S_DETECT_MASTER_DOWN,
	/* establish mark */
	NST_M_ESTABLISHED = NST_M_CONNECTED_TO_ALL_CLST,
	NST_S_ESTABLISHED = NST_S_CONNECTED_TO_MASTER,
};

/* bcast proto */
enum {
	NBR_CLST_BCAST_INVALID,
	NBR_CLST_BCAST_FIND,
	NBR_CLST_BCAST_FIND_ACK,
};

#define CLST_TIMEO_SEC		(10)	/* 10 sec timeout */
#define CLST_BCAST_RB		(1024)
#define CLST_BCAST_WB		(1024)
#define CLST_BCAST_SOCK		(5)
#define CLST_BCAST_GROUP	("239.192.1.2")



/*-------------------------------------------------------------*/
/* internal types											   */
/*-------------------------------------------------------------*/
typedef struct node {
	struct node	*next;
	SOCK 		conn;		/* connection to clst */
	U8			addr[32];
	U8			addrlen, padd[3];
	U32			servant_num;
	UTIME		mtime;
}	node_t;

typedef struct clst {
	SOCKMGR	servant_skm, master_skm;
	node_t	*master;
	U8		state, padd;
	U16		type_id;	/* its port number as well */
}	clst_t;



/*-------------------------------------------------------------*/
/* internal values											   */
/*-------------------------------------------------------------*/
struct {
	ARRAY	clst_a, node_a;
	SOCKMGR	bcast_skm;
	U8		multiplex, establish;
	U16		bcast_port;
}	g_clst = {
			NULL, NULL,
			NULL,
			0, 0,
			0
};



/*-------------------------------------------------------------*/
/* internal methods											   */
/*-------------------------------------------------------------*/
NBR_INLINE void
node_destroy(node_t *nd)
{
	if (nd) {
		nd->next = NULL;
		nbr_sock_close(nd->conn);
		nbr_array_free(g_clst.node_a, nd);
	}
}

NBR_INLINE void
clst_set_establish(int est)
{
	g_clst.establish = est;
}

NBR_INLINE int
clst_is_master(clst_t *c)
{
	return c->master_skm ? 1 : 0;
}

NBR_INLINE void
clst_destroy(clst_t *clst)
{
	node_t *ndd, *pndd;
	if (!clst) { return; }
	ndd = clst->master;
	while ((pndd = ndd)) {
		ndd = ndd->next;
		node_destroy(pndd);
	}
	clst->state = NST_INVALID;
	/* skm will destroy at nbr_sock_fin */
	clst->master_skm = NULL;
	clst->servant_skm = NULL;
	nbr_array_free(g_clst.clst_a, clst);
}

NBR_INLINE clst_t *
clst_search_by_id(U16 type_id)
{
	clst_t *clst;
	ARRAY_SCAN(g_clst.clst_a, clst) {
		if (clst->type_id == type_id) { return clst; }
	}
	return NULL;
}

NBR_INLINE void
clst_poll(clst_t *clst)
{
	switch(clst->state) {
	/* master clst status */
	case NST_M_START:
	case NST_M_MULTICAST_TO_FIND:
	case NST_M_CONNECTING_MASTER:
	case NST_M_CONNECTED_TO_MASTER:
	case NST_M_RECEIVEDATA_FROM_MASTER:
	case NST_M_CONNECTING_ALL_CLST:
	case NST_M_CONNECTED_TO_ALL_CLST:
		break;
	/* servant clst status */
	case NST_S_START:
	case NST_S_MULTICAST_TO_FIND:
	case NST_S_CONNECTING_MASTER:
	case NST_S_CONNECTED_TO_MASTER:
	case NST_S_CONNECTING_TO_SECONDARY:
	case NST_S_CONNECTED_TO_SECONDARY:
	case NST_S_DETECT_MASTER_DOWN:
		break;
	default:
		CLST_LOG(LOG_ERROR,"invalid clst state(%d)\n", clst->state);
		break;
	}
}


/* sockmgr callback */
/* for master */
static int
master_node_acceptwatcher(SOCK sk)
{
	return NBR_OK;
}

static int
master_node_closewatcher(SOCK sk, int r)
{
	return NBR_OK;
}

static int
master_node_receiver(SOCK sk, char *data, int len)
{
	return NBR_OK;
}

static int
master_node_event_handler(SOCK sk, char *data, int len)
{
	return NBR_OK;
}

/* for servant */
static int
servant_node_acceptwatcher(SOCK sk)
{
	return NBR_OK;
}

static int
servant_node_closewatcher(SOCK sk, int r)
{
	return NBR_OK;
}

static int
servant_node_receiver(SOCK sk, char *data, int len)
{
	return NBR_OK;
}

static int
servant_node_event_handler(SOCK sk, char *data, int len)
{
	return NBR_OK;
}

/* for bcast */
static int
bcast_acceptwatcher(SOCK sk)
{
	return NBR_OK;
}

static int
bcast_closewatcher(SOCK sk, int r)
{
	return NBR_OK;
}

static int
bcast_receiver(SOCK sk, char *data, int len)
{
	clst_t *nd;
	node_t *ndd;
	U16 type_id;
	U8 cmd;
	char work[256], addr[32];
	switch(*data) {
	case NBR_CLST_BCAST_FIND:
		{
			POP_START(data, len);
			POP_8(cmd);
			POP_16(type_id);
			if (!(nd = clst_search_by_id(type_id))) { return NBR_OK; }
			if (!nbr_clst_is_master(nd)) { return NBR_OK; }
			if (!nbr_clst_is_ready(nd)) { return NBR_OK; }
		}
		{
			PUSH_START(work, sizeof(work));
			PUSH_8(((U8)NBR_CLST_BCAST_FIND_ACK));
			PUSH_16(type_id);
			ndd = nd->master;
			while(ndd) {
				PUSH_8A(ndd->addr, ndd->addrlen);
				ndd = ndd->next;
			}
			nbr_sock_send_bin32(sk, work, PUSH_LEN());
		}
		break;
	case NBR_CLST_BCAST_FIND_ACK:
		{
			int alen;
			POP_START(data, len);
			POP_8(cmd);
			POP_16(type_id);
			if (!(nd = clst_search_by_id(type_id))) {
				return NBR_OK;
			}
			ASSERT(nbr_clst_is_master(nd));
			POP_8A(addr, alen);
		}
		break;
	}
	return NBR_OK;
}

static int
bcast_event_handler(SOCK sk, char *data, int len)
{
	return NBR_OK;
}

/* bcast sender */
static int
bcast_send_find_clst(SOCKMGR skm, char *addr, U16 type_id)
{
	char data[16];
	U16 len = 3;
	PUSH_START(data, sizeof(data));
	PUSH_16(len);	/* bin32 proto */
	PUSH_8(((U8)NBR_CLST_BCAST_FIND));
	PUSH_16(type_id);
	return nbr_sockmgr_bcast(skm, addr, data, PUSH_LEN());
}

/* conn sender */



/*-------------------------------------------------------------*/
/* external methods											   */
/*-------------------------------------------------------------*/
int
nbr_clst_init(int bcast_port, int max_clst, int multiplex)
{
	char addr[32];
	UDPCONF ucf = { CLST_BCAST_GROUP, 1 };
	g_clst.multiplex = multiplex;
	if (!(g_clst.clst_a = nbr_array_create(max_clst, sizeof(clst_t), 0))) {
		CLST_ERROUT(ERROR,EXPIRE,"clst: clst_t array\n");
		goto bad;
	}
	if (!(g_clst.node_a = nbr_array_create(
		max_clst * multiplex, sizeof(node_t), NBR_PRIM_EXPANDABLE))) {
		CLST_ERROUT(ERROR,EXPIRE,"clst: node_t array\n");
		goto bad;
	}
	if (bcast_port <= 0) {
		CLST_ERROUT(ERROR,INVAL,"bcast portnum invalid (%d)\n", bcast_port);
		goto bad;
	}
	nbr_str_printf(addr, sizeof(addr) - 1, "0.0.0.0:%u", bcast_port);
	if (!(g_clst.bcast_skm = nbr_sockmgr_create(
						CLST_BCAST_RB, CLST_BCAST_WB,
						CLST_BCAST_SOCK, 0, CLST_TIMEO_SEC,
						addr, nbr_proto_udp(), &ucf, NBR_PRIM_EXPANDABLE))) {
		CLST_ERROUT(ERROR,EXPIRE,"create servant skm\n");
		goto bad;
	}
	nbr_sockmgr_set_callback(g_clst.bcast_skm,
				nbr_sock_rparser_bin16,
				bcast_acceptwatcher,
				bcast_closewatcher,
				bcast_receiver,
				NULL);
	g_clst.bcast_port = bcast_port;
	return NBR_OK;
bad:
	nbr_clst_fin();
	return LASTERR;
}

void
nbr_clst_fin()
{
	clst_t *c, *pc;
	node_t *nd, *pnd;
	if (g_clst.clst_a) {
		c = nbr_array_get_first(g_clst.clst_a);
		while ((pc = c)) {
			c = nbr_array_get_next(g_clst.clst_a, c);
			clst_destroy(pc);
		}
		nbr_array_destroy(g_clst.clst_a);
		g_clst.clst_a = NULL;
	}
	if (g_clst.node_a) {
		nd = nbr_array_get_first(g_clst.node_a);
		while ((pnd = nd)) {
			nd = nbr_array_get_next(g_clst.node_a, nd);
			node_destroy(pnd);
		}
		nbr_array_destroy(g_clst.node_a);
		g_clst.node_a = NULL;
	}
	g_clst.bcast_skm = NULL;
	g_clst.bcast_port = 0;
}

void
nbr_clst_poll()
{
	clst_t *clst, *pclst;
	if (g_clst.establish) { return; }
	clst = nbr_array_get_first(g_clst.clst_a);
	while ((pclst = clst)) {
		clst = nbr_array_get_next(g_clst.clst_a, clst);
		clst_poll(pclst);
	}
}

NBR_API CLST
nbr_clst_create(U16 type_id, int mstr_rb, int mstr_wb, int max_servant)
{
	char addr[32];
	clst_t *clst;
	if (g_clst.bcast_port == type_id) {
		CLST_ERROUT(ERROR,INVAL,"type_id is same as bcast (%d)\n", type_id);
		return NULL;
	}
	if ((clst = clst_search_by_id(type_id))) { return clst; }
	if (!(clst = nbr_array_alloc(g_clst.clst_a))) {
		CLST_ERROUT(ERROR,EXPIRE,"alloc clst_t\n");
		goto bad;
	}
	clst->type_id = type_id;
	if (max_servant <= 0) {	/* means servant clst */
		clst->state = NST_S_START;
		/* swap nrb and nwb (because master nrb = servant nwb and vice versa) */
		if (!(clst->servant_skm = nbr_sockmgr_create(mstr_wb, mstr_rb,
							g_clst.multiplex, 0,
							CLST_TIMEO_SEC, NULL, nbr_proto_tcp(), NULL,
							NBR_PRIM_EXPANDABLE))) {
			CLST_ERROUT(ERROR,EXPIRE,"create servant skm\n");
			goto bad;
		}
	}
	else {
		clst->state = NST_M_START;
		nbr_str_printf(addr, sizeof(addr) - 1, "0.0.0.0:%u", clst->type_id);
		if (!(clst->master_skm = nbr_sockmgr_create(mstr_rb, mstr_wb,
							max_servant, 0,
							CLST_TIMEO_SEC, addr, nbr_proto_tcp(), NULL,
							NBR_PRIM_EXPANDABLE))) {
			CLST_ERROUT(ERROR,EXPIRE,"create master skm\n");
			goto bad;
		}
		/* setup master callback */
		nbr_sockmgr_set_callback(clst->master_skm,
			nbr_sock_rparser_bin32,
			master_node_acceptwatcher,
			master_node_closewatcher,
			master_node_receiver,
			master_node_event_handler);
		/* swap nrb and nwb (because master nrb = servant nwb and vice versa) */
		if (!(clst->servant_skm = nbr_sockmgr_create(mstr_wb, mstr_rb,
							CLST_BCAST_SOCK, 0,
							CLST_TIMEO_SEC, NULL, nbr_proto_tcp(), NULL,
							NBR_PRIM_EXPANDABLE))) {
			CLST_ERROUT(ERROR,EXPIRE,"create servant skm\n");
			goto bad;
		}
	}
	/* setup servant callbacks */
	nbr_sockmgr_set_callback(clst->servant_skm,
		nbr_sock_rparser_bin32,
		servant_node_acceptwatcher,
		servant_node_closewatcher,
		servant_node_receiver,
		servant_node_event_handler);
	/* multicast to find */
	nbr_str_printf(addr, sizeof(addr) - 1, "%s:%u", CLST_BCAST_GROUP, clst->type_id);
	if (bcast_send_find_clst(g_clst.bcast_skm, addr, clst->type_id) < 0) {
		CLST_ERROUT(ERROR,SEND,"send clst search\n");
		goto bad;
	}
	clst->state = clst_is_master(clst) ?
			NST_M_MULTICAST_TO_FIND : NST_S_MULTICAST_TO_FIND;
	return (CLST)clst;
bad:
	nbr_clst_destroy(clst);
	return NULL;
}

NBR_API void
nbr_clst_destroy(CLST c)
{
	clst_t *clst = c;
	clst_destroy(clst);
}

NBR_API int
nbr_clst_is_master(CLST c)
{
	clst_t *clst = c;
	return clst_is_master(clst);
}

NBR_API int
nbr_clst_is_ready(CLST c)
{
	clst_t *clst = c;
	return clst_is_master(clst) ?
			(clst->state == NST_M_ESTABLISHED ? 1 : 0) :
			(clst->state == NST_S_ESTABLISHED ? 1 : 0);
}

NBR_API int
nbr_clst_send_master(CLST c, char *data, int len)
{
	clst_t *clst = c;
	if ((!(clst->master)) || clst->master_skm) {
		ASSERT(FALSE);
		return NBR_EINVAL;
	}
	return nbr_sock_send_bin32(clst->master->conn, data, len);
}


