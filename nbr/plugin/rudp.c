/****************************************************************
 * rudp.c : reliable UDP protocol plugin (works in progress)
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
#include "proto.h"
#include "osdep.h"

/*-------------------------------------------------------------*/
/* macro													   */
/*-------------------------------------------------------------*/
#define RUDP_ERROUT		NBR_ERROUT



/*-------------------------------------------------------------*/
/* constant													   */
/*-------------------------------------------------------------*/



/*-------------------------------------------------------------*/
/* internal types											   */
/*-------------------------------------------------------------*/



/*-------------------------------------------------------------*/
/* static variable											   */
/*-------------------------------------------------------------*/
static	PROTOCOL *g_rudp_p;




/*-------------------------------------------------------------*/
/* external methods											   */
/*-------------------------------------------------------------*/
PROTOCOL *
nbr_proto_rudp(void *p)
{
	PROTOCOL	rudp = {
		"UDP",
		p,
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
		nbr_osdep_udp_close,
		(RECVFUNC)nbr_osdep_udp_recvfrom,
		(SENDFUNC)nbr_osdep_udp_sendto,
	};
	if (!g_rudp_p) {
		if (!(g_rudp_p = nbr_proto_regist(rudp))) {
			RUDP_ERROUT(ERROR,EXPIRE,"%s:protocol register fail",rudp.name);
			return NULL;
		}
	}
	return g_rudp_p;
}
