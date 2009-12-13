/****************************************************************
 * nbr.c : common initialize/finalize/polling routines
 * 2009/08/25 iyatomi : create
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
#include "nbr.h"
#include "array.h"
#include "search.h"
#include "proto.h"
#include "rand.h"
#include "osdep.h"
#include "thread.h"
#include "sock.h"
#include "cluster.h"
#include "sig.h"


/*-------------------------------------------------------------*/
/* internal types											   */
/*-------------------------------------------------------------*/
#define INIT_OR_DIE(ret,exp) if (NBR_OK != (ret = exp)) { nbr_fin(); return ret; }



/*-------------------------------------------------------------*/
/* internal methods											   */
/*-------------------------------------------------------------*/



/*-------------------------------------------------------------*/
/* external methods											   */
/*-------------------------------------------------------------*/
NBR_API int
nbr_init(CONFIG *c)
{
	int r;
	CONFIG dc;
	if (!c) {
		c = &dc;
		nbr_get_default(c);
	}
	nbr_err_init();
	INIT_OR_DIE(r, nbr_clock_init());
	INIT_OR_DIE(r, nbr_sig_init());
	INIT_OR_DIE(r, nbr_array_init(c->max_array));
	INIT_OR_DIE(r, nbr_lock_init(c->max_nfd));
	INIT_OR_DIE(r, nbr_rand_init());
	INIT_OR_DIE(r, nbr_thread_init(c->max_thread));
	INIT_OR_DIE(r, nbr_search_init(c->max_search));
	INIT_OR_DIE(r, nbr_proto_init(c->max_proto));
	INIT_OR_DIE(r, nbr_sock_init(c->max_sockmgr,
		c->max_nfd, c->max_thread, c->sockbuf_size));
	INIT_OR_DIE(r, nbr_cluster_init(c->ndc.mcast_port,
		c->ndc.max_node, c->ndc.multiplex));
	nbr_sock_set_nioconf(c->ioc);
	return NBR_OK;
}

NBR_API void
nbr_get_default(CONFIG *c)
{
	c->max_search = 64;
	c->max_array = 128 + c->max_search;
	c->max_proto = 3;
	c->max_sockmgr = 16;
	c->max_nfd = 1024;
	c->max_thread = 3;
	c->sockbuf_size = 1 * 1024 * 1024; /* 1MB */
	/* NIOCONF */
	c->ioc.epoll_timeout_ms = 50;	/* 50ms */
	c->ioc.job_idle_sleep_us = 10; /* 10us */
	/* NODECONF */
	c->ndc.max_node = 2;
	c->ndc.multiplex = 2;
	c->ndc.mcast_port = 8888;
}

NBR_API void
nbr_fin()
{
	nbr_cluster_fin();
	nbr_sock_fin();
	nbr_search_fin();
	nbr_thread_fin();
	nbr_proto_fin();
	nbr_rand_fin();
	nbr_lock_fin();
	nbr_err_fin();
	nbr_clock_fin();
	nbr_array_fin();
	nbr_sig_fin();
}

NBR_API void
nbr_poll()
{
	nbr_clock_poll();
	nbr_sock_poll();
}

