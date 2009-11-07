/************************************************************
 * nbr_plg.h : all plugin API definition should be placed here
 * 2009/09/21 iyatomi : create
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
#if !defined(__NBR_PLG_H__)
#define __NBR_PLG_H__

/*-------------------------------------------------------------*/
/* add your plugin declaration here :D						   */
/*-------------------------------------------------------------*/

/* SSL connection libnbr_ssl.a */
enum {
	NBR_SSL_METHOD_INVALID,
	NBR_SSL_METHOD_SERVER_V23,
	NBR_TLS_METHOD_SERVER_V1,
	NBR_SSL_METHOD_CLIENT_V23,
	NBR_TLS_METHOD_CLIENT_V1,
};
typedef struct nbr_ssl_conf_t
{
	int	max_socket;
	int server_method, client_method;
	char *pubkey, *privkey;
}	SSLCONF;
NBR_API	PROTOCOL *nbr_proto_ssl(void *p);



/* reliable UDP libnbr_rudp.a */
NBR_API PROTOCOL *nbr_proto_rudp(void *p);

#endif
