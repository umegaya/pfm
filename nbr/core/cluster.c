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
#include "cluster.h"
#include "proto.h"
#include "sock.h"
#include "osdep.h"
#include "nbr_pkt.h"



/*-------------------------------------------------------------*/
/* macro													   */
/*-------------------------------------------------------------*/
#define CLUSTER_ERROUT			NBR_ERROUT
#define CLUSTER_LOG(prio,...)	fprintf(stderr, __VA_ARGS__)



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
	NST_M_CONNECTING_ALL_CLUSTER,
	NST_M_CONNECTED_TO_ALL_CLUSTER,
	/* master clst status */
	NST_S_START,
	NST_S_MULTICAST_TO_FIND,
	NST_S_CONNECTING_MASTER,
	NST_S_CONNECTED_TO_MASTER,
	NST_S_DETECT_MASTER_DOWN,
	/* establish mark */
	NST_M_ESTABLISHED = NST_M_CONNECTED_TO_ALL_CLUSTER,
	NST_S_ESTABLISHED = NST_S_CONNECTED_TO_MASTER,
};

/* mcast proto */
enum {
	NBR_CLUSTER_MCAST_INVALID,
	NBR_CLUSTER_MCAST_FIND,
	NBR_CLUSTER_MCAST_FIND_ACK,
};

#define CLUSTER_TIMEO_SEC	(10)	/* 10 sec timeout */
#define CLUSTER_BCAST_RB	(1024)
#define CLUSTER_BCAST_WB	(1024)
#define CLUSTER_BCAST_SOCK	(5)
#define CLUSTER_BCAST_GROUP	("239.192.1.2")
#define CLUSTER_ADRL 		(32)



/*-------------------------------------------------------------*/
/* internal types											   */
/*-------------------------------------------------------------*/
typedef struct node {
	struct node		*next;		/* used by nodeset_t: next node */
	struct cluster	*belong;	/* belonged cluster */
	SOCK 			conn;		/* connection to this node */
	char			addr[CLUSTER_ADRL];	/* address string */
	char			p[0];		/* user defined node data */
}	node_t;

typedef struct nodeset {
	node_t			*list;
	SOCKMGR			skm;
	ARRAY			node_a;
	/* dispatch various event to user defined node data */
	int				(*proc)(void*, NDEVENT, char*, int);
}	nodeset_t;

typedef struct cluster {
	nodeset_t		master, servant;
	node_t			*mynode;
	U8				state, padd;
	U16				clst_id;	/* its port number as well */
	RWLOCK			lock;
	/* sort master nodes to connect (for servant node) */
	int				(*sort)(NODE*, int);
}	cluster_t;



/*-------------------------------------------------------------*/
/* internal values											   */
/*-------------------------------------------------------------*/
struct {
	ARRAY	cluster_a;
	SEARCH	node_s;
	SOCKMGR	mcast_skm;
	U16		mcast_port;
	U8		multiplex, padd;
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
node_destroy(nodeset_t *nds, node_t *nd)
{
	if (!nd) { return; }
	nd->next = NULL;
	nbr_sock_close(nd->conn);
	nbr_array_free(nds->node_a, nd);
}

NBR_INLINE int
nodeset_create(nodeset_t *nds, const char *addr, int max, int ndlen, int rb, int wb,
		int (*on_event)(void*, NDEVENT, char *, int))
{
	nds->list = NULL;
	/* swap nrb and nwb (because servant nrb = master nwb and vice versa) */
	if (!(nds->skm = nbr_sockmgr_create(rb, wb, max, 0,
						CLUSTER_TIMEO_SEC, (char *)addr, nbr_proto_tcp(), NULL,
						NBR_PRIM_EXPANDABLE))) {
		CLUSTER_ERROUT(ERROR,EXPIRE,"create master skm\n");
		return LASTERR;
	}
	if (!(nds->node_a = nbr_array_create(max,
		sizeof(node_t) + ndlen, (NBR_PRIM_THREADSAFE | NBR_PRIM_EXPANDABLE)))) {
		CLUSTER_ERROUT(ERROR,EXPIRE,"create array for node_t\n");
		return LASTERR;
	}
	return NBR_OK;
}

NBR_INLINE void
nodeset_destroy(nodeset_t *nds)
{
	node_t *nd = nds->list, *pnd;
	while ((pnd = nd)) {
		nd = nd->next;
		node_destroy(nds, pnd);
	}
	if (nds->node_a) {
		nbr_array_destroy(nds->node_a);
		nds->node_a = NULL;
	}
	nds->skm = NULL;
	nds->list = NULL;
}

NBR_INLINE int
nodeset_reconstruct(cluster_t *c, nodeset_t *nds, node_t **list, int n_list)
{
	int n_node = nbr_array_use(nds->node_a), i, cnt = 0;
	node_t *a_nodes[n_node], *nd;
	if (!list) {
		ARRAY_SCAN(nds->node_a, nd) {
			a_nodes[cnt++] = nd;
		}
		list = a_nodes;
		n_list = cnt;
	}
	c->sort((NODE *)list, n_list);
	nds->list = list[0];
	nds->list->belong = c;
	for (i = 1; i < (n_list - 1); i++) {
		list[i - 1]->next = list[i];
		list[i - 1]->belong = c;
	}
	list[i]->next = NULL;
	list[i]->belong = c;
	cnt = nbr_cluster_is_master(c) ? n_list : g_clst.multiplex;
	for (i = 0, nd = nds->list; nd && i < cnt; i++, nd = nd->next) {
		if (!nbr_sock_valid(nd->conn)) {
			nd->conn = nbr_sockmgr_connect(c->master.skm, nd->addr, NULL, NULL);
			if (!nbr_sock_valid(nd->conn)) {
				return NBR_ECONNECT;
			}
		}
	}
	return n_list;
}

NBR_INLINE int
cluster_is_master(cluster_t *c)
{
	return c->mynode ? 1 : 0;
}

NBR_INLINE void
cluster_destroy(cluster_t *c)
{
	if (!c) { return; }
	nodeset_destroy(&(c->master));
	nodeset_destroy(&(c->servant));
	c->state = NST_INVALID;
	if (c->lock) {
		nbr_rwlock_destroy(c->lock);
		c->lock = NULL;
	}
	nbr_array_free(g_clst.cluster_a, c);
}

NBR_INLINE cluster_t *
cluster_search_by_id(U16 clst_id)
{
	cluster_t *c;
	ARRAY_SCAN(g_clst.cluster_a, c) {
		if (c->clst_id == clst_id) { return c; }
	}
	return NULL;
}

NBR_INLINE char *
cluster_addr_from_remote_sock(U16 clst_id, SOCK sk, char *buff, int len)
{
	char addr[256], url[256];
	U16 port;
	if (nbr_sock_get_addr(sk, buff, len) < 0) {
		return "";
	}
	if (nbr_str_parse_url(buff, sizeof(addr), addr, &port, url) < 0) {
		ASSERT(FALSE);
		return "";
	}
	nbr_str_printf(buff, len, "%s:%hu", addr, clst_id);
	return buff;
}

NBR_INLINE void
cluster_poll(cluster_t *clst)
{
	switch(clst->state) {
	/* master clst status */
	case NST_M_START:
	case NST_M_MULTICAST_TO_FIND:
	case NST_M_CONNECTING_MASTER:
	case NST_M_CONNECTED_TO_MASTER:
	case NST_M_RECEIVEDATA_FROM_MASTER:
	case NST_M_CONNECTING_ALL_CLUSTER:
	case NST_M_CONNECTED_TO_ALL_CLUSTER:
		break;
	/* master clst status */
	case NST_S_START:
	case NST_S_MULTICAST_TO_FIND:
	case NST_S_CONNECTING_MASTER:
	case NST_S_CONNECTED_TO_MASTER:
	case NST_S_DETECT_MASTER_DOWN:
		break;
	default:
		CLUSTER_LOG(LOG_ERROR,"invalid clst state(%d)\n", clst->state);
		break;
	}
}


/* bcast sender */
NBR_INLINE int
mcast_send_find_master(SOCKMGR skm, U16 clst_id)
{
	char data[16], addr[CLUSTER_ADRL];
	U16 len = 3;
	PUSH_START(data, sizeof(data));
	PUSH_16(len);	/* bin32 proto */
	PUSH_8(((U8)NBR_CLUSTER_MCAST_FIND));
	PUSH_16(clst_id);
	nbr_str_printf(addr, sizeof(addr) - 1, "%s:%u",
		CLUSTER_BCAST_GROUP, clst_id);
	return nbr_sockmgr_mcast(skm, addr, data, PUSH_LEN());
}

/* conn sender */


/* mcast handler */
NBR_INLINE int
mcast_on_find(SOCK sk, cluster_t *c, char *p, int l)
{
	char work[1024 * 1024];
	char tmp[1024];
	node_t *nd;
	int n_node, len;
	PUSH_START(work, sizeof(work));
	PUSH_8(((U8)NBR_CLUSTER_MCAST_FIND_ACK));
	PUSH_16(c->clst_id);
	n_node = nbr_array_use(c->master.node_a);
	PUSH_32(n_node);
	ARRAY_SCAN(c->master.node_a, nd) {
		PUSH_STR(nd->addr);
		if ((len = c->master.proc(
			nd->p, NDEV_GET_DATA, tmp, sizeof(tmp))) < 0) {
			return NBR_ECBFAIL;
		}
		PUSH_8A(tmp, len);
	}
	if (nbr_sock_send_bin32(sk, work, PUSH_LEN()) < 0) {
		ASSERT(FALSE);
		return NBR_ESEND;
	}
	return NBR_OK;
}

NBR_INLINE int
mcast_on_find_ack(SOCK sk, cluster_t *c, char *data, int len)
{
	U16 clst_id;
	U8 cmd;
	int n_node, i, r = NBR_OK;
	char tmp[1024];
	if (cluster_is_master(c)) {
		if (c->state != NST_M_MULTICAST_TO_FIND) {
			return NBR_EINVAL;
		}
	}
	else if (c->state != NST_S_MULTICAST_TO_FIND) {
		return NBR_EINVAL;
	}
	POP_START(data, len);
	POP_8(cmd);
	POP_16(clst_id);
	POP_32(n_node);
	if (n_node > 0) {
		node_t *a_nodes[n_node];
		int cnt = 0;
		nbr_mem_zero(a_nodes, n_node * sizeof(node_t *));
		for (i = 0; i < n_node; i++) {
			if (!(a_nodes[cnt] = nbr_array_alloc(c->master.node_a))) {
				r = NBR_EEXPIRE;
				break;
			}
			POP_STR(a_nodes[cnt]->addr, sizeof(a_nodes[cnt]->addr));
			POP_8A(tmp, len);
			if (c->master.proc(a_nodes[cnt]->p, NDEV_PUT_DATA, tmp, len) < 0) {
				r = NBR_ECBFAIL;
				break;
			}
			nbr_sock_clear(&(a_nodes[cnt]->conn));
			cnt++;
		}
		if (r == NBR_OK) {
			if ((r = nodeset_reconstruct(c, &(c->master), a_nodes, cnt)) < 0) {
				c->state = cluster_is_master(c) ?
					NST_M_CONNECTING_MASTER : NST_S_CONNECTING_MASTER;
			}
		}
		else {
			for (i = 0; i < cnt; i++) {
				if (!a_nodes[i]) { continue; }
				c->master.proc(a_nodes[i]->p, NDEV_DELETE, NULL, 0);
				nbr_array_free(c->master.node_a, a_nodes[i]);
			}
		}
	}
	return r;
}


/* master_node handler */
/* servant_node handler */


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

/* for master */
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

/* for mcast */
static int
mcast_acceptwatcher(SOCK sk)
{
	return NBR_OK;
}

static int
mcast_closewatcher(SOCK sk, int r)
{
	return NBR_OK;
}

static int
mcast_receiver(SOCK sk, char *data, int len)
{
	cluster_t *c;
	U16 clst_id;
	U8 cmd;
	int r = NBR_OK;
	POP_START(data, len);
	POP_8(cmd);
	POP_16(clst_id);
	if (!(c = cluster_search_by_id(clst_id))) { return NBR_EINVAL; }
	if (nbr_rwlock_wrlock(c->lock) != NBR_OK) { return NBR_OK; }
	switch(GET_8(data)) {
	case NBR_CLUSTER_MCAST_FIND:
		if (!nbr_cluster_is_master(c)) { return NBR_EINVAL; }
		if (!nbr_cluster_is_ready(c)) { return NBR_EINVAL; }
		r = mcast_on_find(sk, c, data, len);
		break;
	case NBR_CLUSTER_MCAST_FIND_ACK:
		r = mcast_on_find_ack(sk, c, data, len);
		break;
	}
	nbr_rwlock_unlock(c->lock);
	nbr_sock_close(sk); /* prevent from expiring sock buffer */
	return NBR_OK;
}

static int
mcast_event_handler(SOCK sk, char *data, int len)
{
	return NBR_OK;
}



/*-------------------------------------------------------------*/
/* external methods											   */
/*-------------------------------------------------------------*/
int
nbr_cluster_init(int mcast_port, int max_clst, int multiplex)
{
	char addr[32];
	UDPCONF ucf = { CLUSTER_BCAST_GROUP, 1/* TTL=1hop */ };
	g_clst.multiplex = multiplex;
	if (!(g_clst.cluster_a = nbr_array_create(max_clst, sizeof(cluster_t),
		NBR_PRIM_THREADSAFE/* for cluster_search_by_id */))) {
		CLUSTER_ERROUT(ERROR,EXPIRE,"clst: cluster_t array\n");
		goto bad;
	}
	if (!(g_clst.node_s = nbr_search_init_mem_engine(
		1000, NBR_PRIM_EXPANDABLE, 1000, CLUSTER_ADRL))) {
		CLUSTER_ERROUT(ERROR,EXPIRE,"clst: node_t array\n");
		goto bad;
	}
	if (mcast_port <= 0) {
		CLUSTER_ERROUT(ERROR,INVAL,"bcast port invalid (%d)\n", mcast_port);
		goto bad;
	}
	nbr_str_printf(addr, sizeof(addr) - 1, "0.0.0.0:%u", mcast_port);
	if (!(g_clst.mcast_skm = nbr_sockmgr_create(
						CLUSTER_BCAST_RB, CLUSTER_BCAST_WB,
						CLUSTER_BCAST_SOCK, 0, CLUSTER_TIMEO_SEC,
						addr, nbr_proto_udp(), &ucf, NBR_PRIM_EXPANDABLE))) {
		CLUSTER_ERROUT(ERROR,EXPIRE,"create master skm\n");
		goto bad;
	}
	nbr_sockmgr_set_callback(g_clst.mcast_skm,
				nbr_sock_rparser_bin16,
				mcast_acceptwatcher, mcast_closewatcher,
				mcast_receiver, mcast_event_handler, NULL);
	g_clst.mcast_port = mcast_port;
	return NBR_OK;
bad:
	nbr_cluster_fin();
	return LASTERR;
}

void
nbr_cluster_fin()
{
	cluster_t *c, *pc;
	if (g_clst.cluster_a) {
		c = nbr_array_get_first(g_clst.cluster_a);
		while ((pc = c)) {
			c = nbr_array_get_next(g_clst.cluster_a, c);
			cluster_destroy(pc);
		}
		nbr_array_destroy(g_clst.cluster_a);
		g_clst.cluster_a = NULL;
	}
	if (g_clst.node_s) {
		nbr_search_destroy(g_clst.node_s);
		g_clst.node_s = NULL;
	}
	g_clst.mcast_skm = NULL;
	g_clst.mcast_port = 0;
}

void
nbr_cluster_poll()
{
	cluster_t *c, *pc;
	c = nbr_array_get_first(g_clst.cluster_a);
	while ((pc = c)) {
		c = nbr_array_get_next(g_clst.cluster_a, c);
		cluster_poll(pc);
	}
}

NBR_API CLUSTER
nbr_cluster_create(U16 clst_id, int max_servant, int nrb, int nwb,
		void *my_nodedata, int my_ndlen,
		int (*mstrproc)(void*, NDEVENT, char *, int), int mstr_ndlen,
		int (*svntproc)(void*, NDEVENT, char *, int), int svnt_ndlen,
		int (*sort)(NODE*, int))
{
	char addr[32];
	int r;
	cluster_t *c;
	if (g_clst.mcast_port == clst_id) {
		CLUSTER_ERROUT(ERROR,INVAL,"clst_id is same as mcast (%d)\n", clst_id);
		return NULL;
	}
	if ((c = cluster_search_by_id(clst_id))) { return c; }
	if (!(c = nbr_array_alloc(g_clst.cluster_a))) {
		CLUSTER_ERROUT(ERROR,EXPIRE,"alloc cluster_t\n");
		goto bad;
	}
	c->clst_id = clst_id;
	c->servant.list = NULL;
	c->master.list = NULL;
	c->mynode = NULL;
	c->sort = sort;
	if (!(c->lock = nbr_rwlock_create())) {
		CLUSTER_ERROUT(ERROR,PTHREAD,"create rwlock\n");
		goto bad;
	}
	/* create nodeset */
	c->state = NST_S_START;
	/* swap nrb and nwb (because servant nrb = master nwb and vice versa) */
	if ((r = nodeset_create(&(c->master), NULL, g_clst.multiplex,
		mstr_ndlen, nwb, nrb, mstrproc))) {
		goto bad;
	}
	/* setup master callbacks */
	nbr_sockmgr_set_callback(c->master.skm,
		nbr_sock_rparser_bin32,
		master_node_acceptwatcher, master_node_closewatcher,
		master_node_receiver, master_node_event_handler, NULL);

	if (max_servant > 0) {	/* means master node */
		c->state = NST_M_START;
		nbr_str_printf(addr, sizeof(addr) - 1, "0.0.0.0:%u", c->clst_id);
		if ((r = nodeset_create(&(c->servant), addr, max_servant,
			svnt_ndlen, nrb, nwb, svntproc))) {
			goto bad;
		}
		/* setup servant callback */
		nbr_sockmgr_set_callback(c->servant.skm,
			nbr_sock_rparser_bin32,
			servant_node_acceptwatcher, servant_node_closewatcher,
			servant_node_receiver, servant_node_event_handler, NULL);
		/* initialize mynode */
		if (!(c->mynode = nbr_array_alloc(c->master.node_a))) {
			CLUSTER_ERROUT(ERROR,EXPIRE,"alloc node_t\n");
			goto bad;
		}
		if (c->master.proc(c->mynode->p, NDEV_PUT_DATA, my_nodedata, my_ndlen) < 0) {
			CLUSTER_ERROUT(ERROR,CBFAIL,"master proc fail\n");
			goto bad;
		}
		nbr_sock_clear(&(c->mynode->conn));
		if (nbr_osdep_tcp_addr_from_fd(nbr_sockmgr_get_listenfd(c->master.skm),
			c->mynode->addr, sizeof(c->mynode->addr)) < 0) {
			CLUSTER_ERROUT(ERROR,EXPIRE,"alloc node_t (%d)\n", errno);
			goto bad;
		}
	}
	/* multicast to find master node */
	if (mcast_send_find_master(g_clst.mcast_skm, c->clst_id) < 0) {
		CLUSTER_ERROUT(ERROR,SEND,"send cluster search\n");
		goto bad;
	}
	c->state = cluster_is_master(c) ?
			NST_M_MULTICAST_TO_FIND : NST_S_MULTICAST_TO_FIND;
	return (CLUSTER)c;
bad:
	nbr_cluster_destroy(c);
	return NULL;
}

NBR_API void
nbr_cluster_destroy(CLUSTER c)
{
	cluster_t *clst = c;
	cluster_destroy(clst);
}

NBR_API int
nbr_cluster_is_master(CLUSTER c)
{
	cluster_t *clst = c;
	return cluster_is_master(clst);
}

NBR_API int
nbr_cluster_is_ready(CLUSTER c)
{
	cluster_t *clst = c;
	return cluster_is_master(clst) ?
			(clst->state == NST_M_ESTABLISHED ? 1 : 0) :
			(clst->state == NST_S_ESTABLISHED ? 1 : 0);
}

NBR_API int
nbr_cluster_send_master(CLUSTER c, char *data, int len)
{
	cluster_t *clst = c;
	if (!(clst->servant.list)) {
		ASSERT(FALSE);
		return NBR_EINVAL;
	}
	return nbr_sock_send_bin32(clst->master.list->conn, data, len);
}


