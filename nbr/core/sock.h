/************************************************************
 * sock.h : socket I/O
 * 2008/09/21 iyatomi : create
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
#if !defined(__SOCK_H__)
#define __SOCK_H__

/* extern function declaration */
extern int		nbr_sock_init(int max, int max_nfd,
					int worker, int skbsz, int skbmain);
extern int		nbr_sock_fin();
extern void 	nbr_sock_poll();
extern void 	*nbr_sock_io(void *skd);
extern DSCRPTR	nbr_sockmgr_get_listenfd(SOCKMGR s);
extern int		nbr_sockmgr_get_accept_size(SOCKMGR s);
extern int		nbr_sockmgr_get_conn_worksize(SOCKMGR s);
extern int		nbr_sockmgr_iterate_sock(SOCKMGR s, void *p, int (*fn) (SOCK, void*));

#endif
