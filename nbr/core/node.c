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
#include "node.h"
#include "proto.h"
#include "sock.h"



/*-------------------------------------------------------------*/
/* macro													   */
/*-------------------------------------------------------------*/
#define NODE_ERROUT			NBR_ERROUT
#define NODE_LOG(prio,...)	fprintf(stderr, __VA_ARGS__)



/*-------------------------------------------------------------*/
/* constant													   */
/*-------------------------------------------------------------*/
/* node status */
enum {
	NST_INVALID,
	/* master node status */
	NST_M_START,
	NST_M_MULTICAST_TO_FIND,
	NST_M_CONNECTING_MASTER,
	NST_M_CONNECTED_TO_MASTER,
	NST_M_RECEIVEDATA_FROM_MASTER,
	NST_M_CONNECTING_ALL_NODE,
	NST_M_CONNECTED_TO_ALL_NODE,
	NST_M_ESTABLISHED = NST_M_CONNECTED_TO_ALL_NODE,
	/* servant node status */
	NST_S_START,
	NST_S_MULTICAST_TO_FIND,
	NST_S_CONNECTING_MASTER,
	NST_S_CONNECTED_TO_MASTER,
	NST_S_CONNECTING_TO_SECONDARY,
	NST_S_CONNECTED_TO_SECONDARY,
	NST_S_DETECT_MASTER_DOWN,
	NST_S_ESTABLISHED = NST_S_CONNECTED_TO_MASTER,
};

#define NODE_TIMEO_SEC	(10)	/* 10 sec timeout */
#define NODE_BCAST_RB	(1024)
#define NODE_BCAST_WB	(1024)
#define NODE_BCAST_SOCK	(10)



/*-------------------------------------------------------------*/
/* internal types											   */
/*-------------------------------------------------------------*/
typedef struct nodedata {
	struct nodedata	*next;
	SOCK 		conn;		/* connection to node */
	U8			addr[32];
	U16			addrlen;
	U16			padd;
	UTIME		mtime;
}	nodedata_t;

typedef struct node {
	SOCKMGR		servant_skm, master_skm;
	nodedata_t	*master;
	U8			state, padd;
	U16			type_id;	/* its port number as well */
}	node_t;



/*-------------------------------------------------------------*/
/* internal values											   */
/*-------------------------------------------------------------*/
struct {
	ARRAY	node_a, nodedata_a;
	SOCKMGR	bcast_skm;
	U8		multiplex, establish;
	U16		bcast_port;
}	g_node = {
			NULL, NULL,
			NULL,
			0, 0,
			0
};



/*-------------------------------------------------------------*/
/* internal methods											   */
/*-------------------------------------------------------------*/
NBR_INLINE void
nodedata_destroy(nodedata_t *ndd)
{
	if (ndd) {
		ndd->next = NULL;
		nbr_sock_close(ndd->conn);
		nbr_array_free(g_node.nodedata_a, ndd);
	}
}

NBR_INLINE void
node_set_establish(int est)
{
	g_node.establish = est;
}

NBR_INLINE void
node_destroy(node_t *nd)
{
	nodedata_t *ndd, *pndd;
	if (!nd) { return; }
	ndd = nd->master;
	while ((pndd = ndd)) {
		ndd = ndd->next;
		nodedata_destroy(pndd);
	}
	nd->state = NST_INVALID;
	/* skm will destroy at nbr_sock_fin */
	nd->master_skm = NULL;
	nd->servant_skm = NULL;
	nbr_array_free(g_node.node_a, nd);
}

NBR_INLINE node_t *
node_search_by_id(U16 type_id)
{
	node_t *nd;
	ARRAY_SCAN(g_node.node_a, nd) {
		if (nd->type_id == type_id) { return nd; }
	}
	return NULL;
}

NBR_INLINE void
node_poll(node_t *nd)
{
	switch(nd->state) {
	/* master node status */
	case NST_M_START:
	case NST_M_MULTICAST_TO_FIND:
	case NST_M_CONNECTING_MASTER:
	case NST_M_CONNECTED_TO_MASTER:
	case NST_M_RECEIVEDATA_FROM_MASTER:
	case NST_M_CONNECTING_ALL_NODE:
	case NST_M_CONNECTED_TO_ALL_NODE:
		break;
	/* servant node status */
	case NST_S_START:
	case NST_S_MULTICAST_TO_FIND:
	case NST_S_CONNECTING_MASTER:
	case NST_S_CONNECTED_TO_MASTER:
	case NST_S_CONNECTING_TO_SECONDARY:
	case NST_S_CONNECTED_TO_SECONDARY:
	case NST_S_DETECT_MASTER_DOWN:
		break;
	default:
		NODE_LOG(LOG_ERROR,"invalid node state(%d)\n", nd->state);
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
master_node_parser(SOCK sk, char *data, int len)
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
servant_node_parser(SOCK sk, char *data, int len)
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
bcast_parser(SOCK sk, char *data, int len)
{
	return NBR_OK;
}

static int
bcast_event_handler(SOCK sk, char *data, int len)
{
	return NBR_OK;
}



/*-------------------------------------------------------------*/
/* external methods											   */
/*-------------------------------------------------------------*/
int
nbr_node_init(int bcast_port, int max_node, int multiplex)
{
	char addr[32];
	g_node.multiplex = multiplex;
	if (!(g_node.node_a = nbr_array_create(max_node, sizeof(node_t), 0))) {
		NODE_ERROUT(ERROR,EXPIRE,"node: node_t array\n");
		goto bad;
	}
	if (!(g_node.nodedata_a = nbr_array_create(
		max_node * multiplex, sizeof(nodedata_t), NBR_PRIM_EXPANDABLE))) {
		NODE_ERROUT(ERROR,EXPIRE,"node: nodedata_t array\n");
		goto bad;
	}
	if (bcast_port <= 0) {
		NODE_ERROUT(ERROR,INVAL,"bcast portnum invalid (%d)\n", bcast_port);
		goto bad;
	}
	nbr_str_printf(addr, sizeof(addr) - 1, "0.0.0.0:%u", bcast_port);
	if (!(g_node.bcast_skm = nbr_sockmgr_create(
						NODE_BCAST_RB, NODE_BCAST_WB,
						NODE_BCAST_SOCK, 0, NODE_TIMEO_SEC,
						addr, nbr_proto_udp(), NBR_PRIM_EXPANDABLE))) {
		NODE_ERROUT(ERROR,EXPIRE,"create servant skm\n");
		goto bad;
	}
	nbr_sockmgr_set_callback(g_node.bcast_skm,
				nbr_sock_rparser_bin32,
				bcast_acceptwatcher,
				bcast_closewatcher,
				bcast_parser,
				bcast_event_handler);
	g_node.bcast_port = bcast_port;
	return NBR_OK;
bad:
	nbr_node_fin();
	return LASTERR;
}

void
nbr_node_fin()
{
	node_t *nd, *pnd;
	nodedata_t *ndd, *pndd;
	if (g_node.node_a) {
		nd = nbr_array_get_first(g_node.node_a);
		while ((pnd = nd)) {
			nd = nbr_array_get_next(g_node.node_a, nd);
			node_destroy(pnd);
		}
		g_node.node_a = NULL;
	}
	if (g_node.nodedata_a) {
		ndd = nbr_array_get_first(g_node.nodedata_a);
		while ((pndd = ndd)) {
			ndd = nbr_array_get_next(g_node.nodedata_a, ndd);
			nodedata_destroy(pndd);
		}
		g_node.nodedata_a = NULL;
	}
	g_node.bcast_skm = NULL;
}

void
nbr_node_poll()
{
	node_t *nd, *pnd;
	if (g_node.establish) { return; }
	nd = nbr_array_get_first(g_node.node_a);
	while ((pnd = nd)) {
		nd = nbr_array_get_next(g_node.node_a, nd);
		node_poll(pnd);
	}
}


NBR_API NODE
nbr_node_create(U16 type_id, int nrb, int nwb, int max_servant)
{
	char addr[32];
	node_t *nd;
	if (g_node.bcast_port == type_id) { ASSERT(FALSE); return NULL; }
	if ((nd = node_search_by_id(type_id))) { return nd; }
	if (!(nd = nbr_array_alloc(g_node.node_a))) {
		NODE_ERROUT(ERROR,EXPIRE,"alloc node_t\n");
		goto bad;
	}
	nd->type_id = type_id;
	if (max_servant <= 0) {
		nd->state = NST_S_START;
		nd->master_skm = NULL;
	}
	else {
		nd->state = NST_M_START;
		nbr_str_printf(addr, sizeof(addr) - 1, "0.0.0.0:%u", nd->type_id);
		if (!(nd->master_skm = nbr_sockmgr_create(nrb, nwb, max_servant, 0,
							NODE_TIMEO_SEC, addr, nbr_proto_tcp(), NBR_PRIM_EXPANDABLE))) {
			NODE_ERROUT(ERROR,EXPIRE,"create master skm\n");
			goto bad;
		}
		/* setup master callback */
		nbr_sockmgr_set_callback(nd->master_skm,
			nbr_sock_rparser_bin32,
			master_node_acceptwatcher,
			master_node_closewatcher,
			master_node_parser,
			master_node_event_handler);
	}
	/* swap nrb and nwb (because master nrb = servant nwb and vice versa) */
	if (!(nd->servant_skm = nbr_sockmgr_create(nwb, nrb, g_node.multiplex, 0,
						NODE_TIMEO_SEC, NULL, nbr_proto_tcp(), NBR_PRIM_EXPANDABLE))) {
		NODE_ERROUT(ERROR,EXPIRE,"create servant skm\n");
		goto bad;
	}
	/* setup servant callbacks */
	nbr_sockmgr_set_callback(nd->servant_skm,
		nbr_sock_rparser_bin32,
		servant_node_acceptwatcher,
		servant_node_closewatcher,
		servant_node_parser,
		servant_node_event_handler);
	return (NODE)nd;
bad:
	nbr_node_destroy(nd);
	return NULL;
}

NBR_API void
nbr_node_destroy(NODE nd)
{
	node_t *ndp = nd;
	node_destroy(ndp);
}

NBR_API int
nbr_node_is_master(NODE nd)
{
	node_t *ndp = nd;
	return ndp->master_skm ? 1 : 0;
}

NBR_API int
nbr_node_is_ready(NODE nd)
{
	node_t *ndp = nd;
	return ndp->master_skm ?
			(ndp->state == NST_M_ESTABLISHED ? 1 : 0) :
			(ndp->state == NST_S_ESTABLISHED ? 1 : 0);
}

NBR_API int
nbr_node_send_master(NODE nd, char *data, int len)
{
	node_t *ndp = nd;
	if ((!(ndp->master)) || ndp->master_skm) {
		ASSERT(FALSE);
		return NBR_EINVAL;
	}
	return nbr_sock_send_bin32(ndp->master->conn, data, len);
}


