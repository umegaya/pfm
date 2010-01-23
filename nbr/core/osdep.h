/***************************************************************
 * osdep.h : OS dependent functionalities
 * 2008/07/23 iyatomi : create
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
#if !defined(__OSDEP_H__)
#define __OSDEP_H__

#if defined(WIN32)
	//windows
#define	__NBR_WINDOWS__
	#include	<sys/types.h>
	#include	<sys/stat.h>
	#include	<sys/socket.h>
	#include	<windows.h>
	#include	<netinet/in.h>
	#include	<net/if.h>
	#include	<sys/ioctl.h>
	#include	<errno.h>
	#include	<unistd.h>
	#include	<netdb.h>
	#include	<arpa/inet.h>
	#include	<fcntl.h>
#elif defined(linux)
	//linux
#define	__NBR_LINUX__
	#include	<sys/types.h>
	#include	<sys/stat.h>
	#include	<sys/socket.h>
	#include	<sys/time.h>
	#include	<sys/epoll.h>
	#include	<netinet/in.h>
	#include	<net/if.h>
	#include	<sys/ioctl.h>
	#include	<errno.h>
	#include	<unistd.h>
	#include	<netdb.h>
	#include	<arpa/inet.h>
	#include	<fcntl.h>
#endif

/* TCP related */
int		nbr_osdep_tcp_str2addr(const char *str, void *addr, int *len);
int		nbr_osdep_tcp_addr2str(void *addr, int len, char *str, int slen);
DSCRPTR	nbr_osdep_tcp_socket(const char *addr, SKCONF *cfg);
int		nbr_osdep_tcp_connect(DSCRPTR fd, void *addr, int len);
int		nbr_osdep_tcp_handshake(DSCRPTR fd, int r, int w);
DSCRPTR	nbr_osdep_tcp_accept(DSCRPTR fd, void *addr, int *len, SKCONF *cfg);
int		nbr_osdep_tcp_close(DSCRPTR fd);
int		nbr_osdep_tcp_recv(DSCRPTR fd, void *data, size_t len, int);
int		nbr_osdep_tcp_send(DSCRPTR fd, const void *data, size_t len, int);
int 	nbr_osdep_tcp_addr_from_fd(DSCRPTR fd, char *addr, int alen);

/* UDP related */
#define nbr_osdep_udp_str2addr	nbr_osdep_tcp_str2addr
#define nbr_osdep_udp_addr2str	nbr_osdep_tcp_addr2str
DSCRPTR	nbr_osdep_udp_socket(const char *addr, SKCONF *cfg);
#define	nbr_osdep_udp_connect	nbr_osdep_tcp_connect
#define nbr_osdep_udp_handshake	nbr_osdep_tcp_handshake
DSCRPTR	nbr_osdep_udp_accept(DSCRPTR fd, void *addr, int *len, SKCONF *cfg);
#define	nbr_osdep_udp_close		nbr_osdep_tcp_close
#define nbr_osdep_udp_recv		nbr_osdep_tcp_recv
#define nbr_osdep_udp_send		nbr_osdep_tcp_send
int		nbr_osdep_udp_recvfrom(DSCRPTR fd, void *data, size_t len, int, void *addr, size_t *alen);
int		nbr_osdep_udp_sendto(DSCRPTR fd, const void *data, size_t len, int, const void *addr, size_t alen);

/* common socket related */
int		nbr_osdep_sockname(DSCRPTR fd, char *addr, int *len);

/* clock related */
int		nbr_clock_init();
void	nbr_clock_poll();
void	nbr_clock_fin();

/* ulimit related */
enum {
	NBR_LIMIT_TYPE_INVALID,
	NBR_LIMIT_TYPE_CORESIZE,
	NBR_LIMIT_TYPE_FDNUM,
};
int 	nbr_osdep_rlimit_get(int ltype);
int		nbr_osdep_rlimit_set(int ltype, int val);

/* epoll related */
#if !defined(EPOLLRDHUP)
#define EPOLLRDHUP 0x2000
#endif
#if defined(__NBR_LINUX__)
#define nbr_epoll_create	epoll_create
#define nbr_epoll_ctl		epoll_ctl
#define nbr_epoll_wait		epoll_wait
#define nbr_epoll_destroy	close
typedef struct epoll_event	EVENT;
#elif defined(__NBR_WINDOWS__)
typedef struct epoll_event {
    U32 events;      /* epoll イベント */
    union {
		void *ptr;
		int fd;
		U32 u32;
		U64 u64;
	} data;      /* ユーザデータ変数 */
} EVENT;
int		nbr_epoll_create(int max);
int		nbr_epoll_ctl(int epfd, int op, int fd, EVENT *events);
int		nbr_epoll_wait(int epfd, struct EVENT *events,
               int maxevents, int timeout);
int		nbr_epoll_destroy(int epfd);
#endif

/* TLS related */
#if defined(__NBR_LINUX__)
#define NBR_TLS	static __thread
#elif defined(__NBR_WINDOWS__)
#define NBR_TLS __declspec(thread) static
#endif

#endif//__OSDEP_H__
