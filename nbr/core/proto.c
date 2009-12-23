/****************************************************************
 * proto.c : pluggable protocol engine handling
 * 2008/09/18 iyatomi : create
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
#include "proto.h"
#include "osdep.h"

/*-------------------------------------------------------------*/
/* macro													   */
/*-------------------------------------------------------------*/
#define PROTO_ERROUT		NBR_ERROUT



/*-------------------------------------------------------------*/
/* constant													   */
/*-------------------------------------------------------------*/



/*-------------------------------------------------------------*/
/* internal types											   */
/*-------------------------------------------------------------*/
typedef PROTOCOL proto_t;



/*-------------------------------------------------------------*/
/* static variable											   */
/*-------------------------------------------------------------*/
static 	ARRAY		*g_proto = NULL;
static	PROTOCOL	g_tcp = {
	"TCP",
	NULL,
	0,		/* stream socket */
	NULL,
	NULL,
	NULL,
	nbr_osdep_tcp_str2addr,
	nbr_osdep_tcp_addr2str,
	nbr_osdep_tcp_socket,
	nbr_osdep_tcp_connect,
	nbr_osdep_tcp_handshake,
	nbr_osdep_tcp_accept,
#if defined(_DEBUG)
	nbr_osdep_tcp_close,
	(RECVFUNC)nbr_osdep_tcp_recv,
	(SENDFUNC)nbr_osdep_tcp_send,
#else
	close,
	(RECVFUNC)recv,
	(SENDFUNC)send,
#endif
}, *g_tcp_p = NULL;

static	PROTOCOL	g_udp = {
	"UDP",
	NULL,
	1,		/* dgram socket */
	NULL,
	NULL,
	NULL,
	nbr_osdep_udp_str2addr,
	nbr_osdep_udp_addr2str,
	nbr_osdep_udp_socket,
	nbr_osdep_udp_connect,
	nbr_osdep_udp_handshake,
	NULL,
#if defined(_DEBUG)
	nbr_osdep_udp_close,
	(RECVFUNC)nbr_osdep_udp_recvfrom,
	(SENDFUNC)nbr_osdep_udp_sendto,
#else
	close,
	(RECVFUNC)recvfrom,
	(SENDFUNC)sendto,
#endif
}, *g_udp_p = NULL;


/*-------------------------------------------------------------*/
/* external methods											   */
/*-------------------------------------------------------------*/
int
nbr_proto_init(int max)
{
	nbr_proto_fin();

	if (!(g_proto = nbr_array_create(max, sizeof(proto_t), 1))) {
		PROTO_ERROUT(ERROR,INTERNAL,"nbr_array_create: %d", max);
		return LASTERR;
	}
	if ((g_tcp_p = nbr_proto_regist(g_tcp)) == NULL) {
		PROTO_ERROUT(ERROR,EXPIRE,"proto regist tcp: %d", max);
		return LASTERR;
	}
	if ((g_udp_p = nbr_proto_regist(g_udp)) == NULL) {
		PROTO_ERROUT(ERROR,EXPIRE,"proto regist udp: %d", max);
		return LASTERR;
	}
	return NBR_OK;
}

int
nbr_proto_fin()
{
	proto_t	*pr;
	int ret;
	if (g_proto) {
		ARRAY_SCAN(g_proto, pr) {
			if (pr->fin && (ret = pr->fin(pr->context)) < 0) {
				PROTO_ERROUT(ERROR,PROTO,"PROTO[%s]:handler=fin,err=%d",
						pr->name, ret);
				continue;
			}
		}
		nbr_array_destroy(g_proto);
		g_proto = NULL;
	}
	return NBR_OK;
}

NBR_API PROTOCOL*
nbr_proto_regist(PROTOCOL proto)
{
	int ret;
	proto_t *pr = (proto_t *)nbr_array_alloc(g_proto);

	if (pr == NULL) {
		PROTO_ERROUT(ERROR,EXPIRE,"PROTO[%s]:no mem %d used",
			proto.name, nbr_array_use(g_proto));
		goto error;
	}
	*pr = proto;
	if (pr->init && (ret = pr->init(pr->context)) < 0) {
		PROTO_ERROUT(ERROR,PROTO,"PROTO[%s]:init fail %d",
			pr->name, ret);
		goto error;
	}
	return pr;

error:
	if (pr != NULL) {
		nbr_array_free(g_proto, pr);
	}
	return NULL;
}

NBR_API PROTOCOL*
nbr_proto_from_name(const char *name)
{
	proto_t *pr;
	if (g_proto) {
		ARRAY_SCAN(g_proto, pr) {
			if (nbr_str_cmp(pr->name, sizeof(pr->name),
					name, sizeof(pr->name)) == 0) {
				return pr;
			}
		}
	}
	return NULL;
}

NBR_API PROTOCOL*
nbr_proto_tcp()
{
	ASSERT(g_tcp_p);
	return g_tcp_p;
}

NBR_API PROTOCOL*
nbr_proto_udp()
{
	ASSERT(g_udp_p);
	return g_udp_p;
}

NBR_API int
nbr_proto_unregist(PROTOCOL *proto_p)
{
	int ret;
	proto_t *pr;
	ARRAY_SCAN(g_proto, pr) {
		if (pr != proto_p) {
			continue;
		}
		if (pr->fin && (ret = pr->fin(pr->context)) < 0) {
			PROTO_ERROUT(ERROR,PROTO,"PROTO[%s]:handler=fin,err=%d",
				pr->name, ret);
			return LASTERR;
		}
		break;
	}
	if (pr != NULL) {
		nbr_array_free(g_proto, pr);
	}
	return NBR_OK;
}

