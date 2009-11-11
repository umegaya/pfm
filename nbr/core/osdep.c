/****************************************************************
 * osdep.c : os dependent funtionalities
 * 2008/09/17 iyatomi : create
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
#include	"common.h"
#include	"osdep.h"
#include	"nbr_pkt.h"
#include 	<sys/resource.h>




/*-------------------------------------------------------------*/
/* macro													   */
/*-------------------------------------------------------------*/
#define OSDEP_ERROUT			NBR_ERROUT
#define NBR_TCP_LISTEN_BACKLOG	(5)
#define NULLDEV					("/dev/null")


/*-------------------------------------------------------------*/
/* internal types											   */
/*-------------------------------------------------------------*/
#if defined(__NBR_LINUX__)
typedef struct timeval ostime_t;
#elif defined(__NBR_WINDOWS__)
typedef struct ostime {
	int		tm_round;	/* how many times GetTickCount() returns 0? */
	unsigned int	tm_tick;	/* retval of GetTickCount() */
} ostime_t;
#endif



/*-------------------------------------------------------------*/
/* internal values											   */
/*-------------------------------------------------------------*/
static ostime_t	g_start; /* initial time */
static UTIME	g_clock; /* passed time */



/*-------------------------------------------------------------*/
/* internal methods											   */
/*-------------------------------------------------------------*/
NBR_INLINE UTIME
clock_from_ostime(ostime_t *t)
{
#if defined(__NBR_LINUX__)
	return (UTIME)(t->tv_sec * 1000000 + t->tv_usec);
#elif defined(__NBR_WINDOWS__)
	return (UTIME)((t->tm_round << 32 + t->tm_tick) * 1000);
#endif
}

NBR_INLINE void
clock_get_ostime(ostime_t *t)
{
#if defined(__NBR_LINUX__)
	gettimeofday(t, NULL);
#elif defined(__NBR_WINDOWS__)
	t->tm_round = (int)(g_clock >> 32 / 1000);
	t->tm_tick = GetTickCount();
#endif
}

NBR_INLINE UTIME
clock_get_time_diff(ostime_t *t)
{
#if defined(__NBR_LINUX__)
	return (UTIME)((t->tv_sec - g_start.tv_sec) * 1000000 +
			(int)(t->tv_usec - g_start.tv_usec));
#elif defined(__NBR_WINDOWS__)
	return (UTIME)((t->tm_tick - g_start.tm_tick) +
		((t->tm_round - g_start.tm_round) << 32));
#endif
}

NBR_INLINE int
fd_setoption(DSCRPTR fd, SKCONF *cfg)
{
	if (fcntl(fd, F_SETFL, O_NONBLOCK, 1) < 0) {
		OSDEP_ERROUT(ERROR,INVAL,"fcntl fail (nonblock) errno=%d", errno);
		return -5;
	}
	if (cfg->timeout >= 0) {
//		if (setsockopt(fd, SO_SNDTIMEO, &cfg->timeout, sizeof(cfg->timeout)) < 0) {
//			OSDEP_ERROUT(ERROR,SOCKOPT,"setsockopt (sndtimeo) errno=%d", errno);
//			return -1;
//		}
//		if (setsockopt(fd, SO_RCVTIMEO, &cfg->timeout, sizeof(cfg->timeout)) < 0) {
//			OSDEP_ERROUT(ERROR,SOCKOPT,"setsockopt (rcvtimeo) errno=%d", errno);
//			return -2;
//		}
	}
	if (cfg->wblen > 0) {
		if (setsockopt(fd, SOL_SOCKET, SO_SNDBUF,
			(char *)&cfg->wblen, sizeof(cfg->wblen)) < 0) {
			OSDEP_ERROUT(ERROR,SOCKOPT,"setsockopt (sndbuf) errno=%d", errno);
			return -3;
		}
	}
	if (cfg->rblen > 0) {
		if (setsockopt(fd, SOL_SOCKET, SO_RCVBUF,
			(char *)&cfg->rblen, sizeof(cfg->rblen)) < 0) {
			OSDEP_ERROUT(ERROR,SOCKOPT,"setsockopt (rcvbuf) errno=%d", errno);
			return -4;
		}
	}
	return 0;
}

NBR_INLINE int
rlimit_type(int ltype)
{
	switch(ltype) {
	case NBR_LIMIT_TYPE_CORESIZE:
		return RLIMIT_CORE;
	case NBR_LIMIT_TYPE_FDNUM:
		return RLIMIT_NOFILE;
	default:
		ASSERT(FALSE);
		return NBR_ENOTSUPPORT;
	}
}



/*-------------------------------------------------------------*/
/* external methods											   */
/*-------------------------------------------------------------*/
NBR_API int
nbr_osdep_get_macaddr(char *ifname, U8 *addr)
{
	int				soc, ret;
	struct ifreq	req;

	if ((soc = socket(PF_INET, SOCK_DGRAM, 0)) < 0) {
		OSDEP_ERROUT(INFO,INTERNAL,"socket fail: ret=%d,errno=%d",soc,errno);
		goto error;
	}

	nbr_str_copy(req.ifr_name, sizeof(req.ifr_name), ifname, sizeof(req.ifr_name));
	req.ifr_addr.sa_family = AF_INET;

	if ((ret = ioctl(soc, SIOCGIFHWADDR, &req)) < 0) {
		OSDEP_ERROUT(INFO,IOCTL,"ioctl fail: ret=%d,errno=%d",ret,errno);
		goto error;
	}

	nbr_mem_copy(addr, &(req.ifr_addr.sa_data), 6);
	ret = 0;
//	TRACE("MAC ADDRESS (%s): [%02X:%02X:%02X:%02X:%02X:%02X]\n", ifname,
//		*addr, *(addr+1), *(addr+2), *(addr+3), *(addr+4), *(addr+5));
error:
	if (soc >= 0) {
		close(soc);
	}
	return ret;
}

/* process operation */
NBR_API int
nbr_osdep_daemonize()
{
	/* be a daemon process */
#if defined(__NBR_LINUX__)
	fflush(stdout);
	fflush(stderr);
	switch(fork()){
	case -1: return NBR_EFORK;
	case 0: break;
	default: _exit(0);
	}
	if(setsid() == -1) { return NBR_ESYSCALL; }
	switch(fork()){
	case -1: return NBR_EFORK;
	case 0: break;
	default: _exit(0);
	}
	umask(0);
	if(chdir(".") == -1) return NBR_ESYSCALL;
	close(0);
	close(1);
	close(2);
	int fd = open(NULLDEV, O_RDWR, 0);
	if(fd != -1){
		dup2(fd, 0);
		dup2(fd, 1);
		dup2(fd, 2);
		if(fd > 2) close(fd);
	}
	return NBR_OK;
#else
	/* now only linux deamonize */
	return NBR_ENOTSUPPORT;
#endif
}

NBR_API int
nbr_osdep_fork(char *cmd, char *argv[], char *envp[])
{
#if defined(__NBR_LINUX__)
	int r;
	fflush(stdout);
	fflush(stderr);
	switch((r = fork())){
	case -1: return NBR_EFORK;
	case 0: break;
	default: return r;	/* this flow is for parent process
	(call nbr_osdep_fork) */
	}
	/* this flow is for child process (change to cmd) */
	if (-1 == execve(cmd, argv, envp)) {
		return NBR_ESYSCALL;
	}
	/* never reach here (cause execve never returns) */
	return 0;
#else
	/* now only linux fork */
	return NBR_ENOTSUPPORT;
#endif
}



/* tcp related */
int
nbr_osdep_tcp_str2addr(const char *str, void *addr, int *len)
{
	char				addr_buf[256], url_buf[256];
	U16					port;
	struct hostent 		*hp;
	struct sockaddr_in	*sa = (struct sockaddr_in *)addr;

	if (*len < sizeof(*sa)) {
		OSDEP_ERROUT(INFO,INVAL,"addrlen %d not enough %d required",
			*len, sizeof(*sa));
		return LASTERR;
	}
	*len = sizeof(*sa);
	if (nbr_str_parse_url(str, sizeof(addr_buf), addr_buf, &port, url_buf) < 0) {
		OSDEP_ERROUT(INFO,INVAL,"addr (%s) invalid", str);
		return LASTERR;
	}
	if ((hp = gethostbyname(addr_buf)) == NULL) {
		OSDEP_ERROUT(INFO,HOSTBYNAME,"addr (%s) invalid", addr_buf);
		return LASTERR;
 	}
	sa->sin_family = AF_INET;
	sa->sin_addr.s_addr = GET_32(hp->h_addr);
	sa->sin_port = htons(port);
	//TRACE("str2addr: %08x,%u,%u\n", sa->sin_addr.s_addr, sa->sin_port, *len);
	return *len;
}

int
nbr_osdep_tcp_addr2str(void *addr, int len, char *str_addr, int str_len)
{
	struct sockaddr_in	*sa = (struct sockaddr_in *)addr;
	if (len != sizeof(*sa)) {
		OSDEP_ERROUT(INFO,INVAL,"addrlen %d is not match %d required",
			len, sizeof(*sa));
		return LASTERR;
	}
//	TRACE("addr2str: %08x(%s):%hu\n", sa->sin_addr.s_addr, inet_ntoa(sa->sin_addr), sa->sin_port);
	return nbr_str_printf(str_addr, str_len, "%s:%hu", inet_ntoa(sa->sin_addr),
		ntohs(sa->sin_port));
}

DSCRPTR
nbr_osdep_tcp_socket(char *addr, SKCONF *cfg)
{
	struct sockaddr_in sa;
	int alen, reuse;
	DSCRPTR fd = socket(PF_INET, SOCK_STREAM, 0);
	if (fd < 0) {
		OSDEP_ERROUT(ERROR,INVAL,"TCP: create socket fail errno=%d", errno);
		goto error;
	}
	if (fd_setoption(fd, cfg) < 0) {
		goto error;
	}
	if (addr) {
		alen = sizeof(sa);
		if (nbr_osdep_tcp_str2addr(addr, &sa, &alen) < 0) {
			OSDEP_ERROUT(ERROR,INTERNAL,"TCP: str2addr fail errno=%d,addr=%s",
				errno, addr);
			goto error;
		}
		reuse = 1;
		if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
			OSDEP_ERROUT(ERROR,BIND,"TCP: setsockopt(reuseaddr) fail errno=%d", errno);
			goto error;
		}
		if (bind(fd, (struct sockaddr *)&sa, alen) < 0) {
			OSDEP_ERROUT(ERROR,BIND,"TCP: bind fail errno=%d,len=%u,fd=%d",
				errno, alen, fd);
			goto error;
		}
		if (listen(fd, NBR_TCP_LISTEN_BACKLOG) < 0) {
			OSDEP_ERROUT(ERROR,LISTEN,"TCP: listen fail errno=%d", errno);
			goto error;
		}
	}
	return fd;

error:
	if (fd >= 0) {
		close(fd);
	}
	return INVALID_FD;
}

int
nbr_osdep_tcp_connect(DSCRPTR fd, void *addr, int len)
{
	ASSERT(addr);
	struct sockaddr_in *sa;
	if (len != sizeof(struct sockaddr_in)) {
		OSDEP_ERROUT(ERROR,INVAL,"addrlen %d is not match %d required",
			len, sizeof(*sa));
		return LASTERR;
	}
	sa = addr;
	if (connect(fd, addr, len) < 0) {
		if (errno != EINPROGRESS) {
			OSDEP_ERROUT(ERROR,CONNECT,"connect fail errno=%d", errno);
			return LASTERR;
		}
	}
	return NBR_OK;
}

int
nbr_osdep_tcp_handshake(DSCRPTR fd, int r, int w)
{
	return w;
}

DSCRPTR
nbr_osdep_tcp_accept(DSCRPTR fd, void *addr, int *len, SKCONF *cfg)
{
	ASSERT(addr);
	struct sockaddr_in *sa;
	DSCRPTR cfd;
	if (*len < sizeof(struct sockaddr_in)) {
		OSDEP_ERROUT(ERROR,INVAL,"TCP: addrlen %d is not match %d required",
			*len, sizeof(*sa));
		return INVALID_FD;
	}
	if ((cfd = accept(fd, addr, (U32 *)len)) == -1) {
		OSDEP_ERROUT(ERROR,ACCEPT,"TCP: accept fail errno=%d", errno);
		return INVALID_FD;
	}
	if (fd_setoption(fd, cfg) < 0) {
		close(cfd);
		return INVALID_FD;
	}
	return cfd;
}

int
nbr_osdep_tcp_close(DSCRPTR fd)
{
//	TRACE("fd(%d) closed\n", fd);
	struct timeval timeout = { 0, 1 };
	if (setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0) {
		OSDEP_ERROUT(ERROR,SOCKOPT,"setsockopt (sndtimeo) errno=%d", errno);
	}
	if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
		OSDEP_ERROUT(ERROR,SOCKOPT,"setsockopt (rcvtimeo) errno=%d", errno);
	}
	close(fd);
	return NBR_OK;
}

int
nbr_osdep_tcp_recv(DSCRPTR fd, void *data, size_t len, int flag)
{
	int r = recv(fd, data, len, flag);
//	TRACE("recv(%d,%d)=%d\n", fd, len, r);
	return r;
}

int
nbr_osdep_tcp_send(DSCRPTR fd, const void *data, size_t len, int flag)
{
//	TRACE("send(%d,%d)\n", fd, len);
	return send(fd, data, len, flag);
}



/* UDP related */
DSCRPTR
nbr_osdep_udp_socket(char *addr, SKCONF *cfg)
{
	struct sockaddr_in sa;
	struct ip_mreqn mreq;
	int alen, reuse;
	UDPCONF *ucf = cfg->proto_p;
	DSCRPTR fd = socket(PF_INET, SOCK_DGRAM, 0);
	if (fd < 0) {
		OSDEP_ERROUT(ERROR,INVAL,"UDP: create socket fail errno=%d", errno);
		goto error;
	}
	if (fd_setoption(fd, cfg) < 0) {
		goto error;
	}
	if (addr) {
		alen = sizeof(sa);
		if (nbr_osdep_udp_str2addr(addr, &sa, &alen) < 0) {
			OSDEP_ERROUT(ERROR,INTERNAL,"UDP: str2addr fail errno=%d,addr=%s",
				errno, addr);
			goto error;
		}
		reuse = 1;
		if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
			OSDEP_ERROUT(ERROR,SOCKOPT,"TCP: setsockopt(reuseaddr) fail errno=%d",
				errno);
			goto error;
		}
		if (bind(fd, (struct sockaddr *)&sa, alen) < 0) {
			OSDEP_ERROUT(ERROR,BIND,"UDP: bind fail errno=%d", errno);
			goto error;
		}
	}
	if (ucf && ucf->mcast_addr) {
		if (0 == inet_aton(ucf->mcast_addr, &(mreq.imr_multiaddr))) {
			OSDEP_ERROUT(ERROR,SOCKET,"get mcast addr: (%s)\n", ucf->mcast_addr);
			goto error;
		}
		mreq.imr_address.s_addr = addr ? sa.sin_addr.s_addr : INADDR_ANY;
		mreq.imr_ifindex = 0;
		if (setsockopt(fd,
			IPPROTO_IP, IP_MULTICAST_IF, (char *)&(mreq), sizeof(mreq)) == -1) {
			OSDEP_ERROUT(ERROR,SOCKOPT,"setmcastif : %d\n", errno);
			goto error;
		}
		if (addr) { /* if bind is done */
			if (setsockopt(fd,
				IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&(mreq), sizeof(mreq)) == -1) {
				OSDEP_ERROUT(ERROR,SOCKOPT,"add member ship : %d\n", errno);
				goto error;
			}
		}
		if (setsockopt(fd,
			IPPROTO_IP, IP_MULTICAST_TTL, (char *)&(ucf->ttl), sizeof(ucf->ttl)) == -1) {
			printf("set multicast ttl : %d %d\n", errno, ucf->ttl);
			goto error;
		 }
	}
	return fd;

error:
	if (fd >= 0) {
		close(fd);
	}
	return INVALID_FD;
}

int
nbr_osdep_udp_recvfrom(DSCRPTR fd, void *data, size_t len, int flag, void *addr, size_t *alen)
{
//	TRACE("recv(%d,%d)\n", fd, len);
	return recvfrom(fd, data, len, 0, addr, alen);
}
int
nbr_osdep_udp_sendto(DSCRPTR fd, const void *data, size_t len, int flag, const void *addr, size_t alen)
{
//	TRACE("send(%d,%d)\n", fd, len);
	return sendto(fd, data, len, 0, addr, alen);
}


/* ulimit related */
int 	nbr_osdep_rlimit_get(int ltype)
{
	struct rlimit rl;
	int resource = rlimit_type(ltype);
	if (resource == NBR_ENOTSUPPORT) {
		return resource;
	}
	getrlimit(resource, &rl);
	return rl.rlim_cur;
}

int		nbr_osdep_rlimit_set(int ltype, int val)
{
	struct rlimit rl;
	int resource = rlimit_type(ltype);
	if (resource == NBR_ENOTSUPPORT) {
		return resource;
	}
	getrlimit(resource, &rl);
	if (val < 0) {
		rl.rlim_max = RLIM_INFINITY;
		rl.rlim_cur = RLIM_INFINITY;
	}
	else {
		rl.rlim_cur = val;
		if (rl.rlim_max < val) {
			rl.rlim_max = val;
		}
	}
	return setrlimit(resource, &rl) == -1 ? NBR_ERLIMIT : NBR_OK;
}




/* clock related */
int nbr_clock_init()
{
	clock_get_ostime(&g_start);
	return NBR_OK;
}

void nbr_clock_fin()
{
}

void nbr_clock_poll()
{
	ostime_t	ost;
	clock_get_ostime(&ost);
	g_clock = clock_get_time_diff(&ost);
}

UTIME nbr_clock()
{
	return g_clock;
}
