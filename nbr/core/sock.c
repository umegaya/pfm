/************************************************************
 * sock.c : socket I/O
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
#include "common.h"
#include "proto.h"
#include "sock.h"
#include "thread.h"
#include "osdep.h"
#include "macro.h"
#include "nbr_pkt.h"


/*-------------------------------------------------------------*/
/* macro													   */
/*-------------------------------------------------------------*/
#define SOCK_ERROUT		NBR_ERROUT
#define SOCK_LOG(prio,...)	fprintf(stderr, __VA_ARGS__)
#define ADRL 			(32)
#define MT_MODE()	(g_sock.worker)



/*-------------------------------------------------------------*/
/* constant													   */
/*-------------------------------------------------------------*/
typedef enum eSOCKSTATE {
	SS_INIT,
	SS_CONNECTING,
	SS_CONNECT,
	SS_DGCONNECT,
	SS_CLOSING,
	SS_CLOSE,
} SOCKSTATE;

typedef enum eSKMKIND {
	SKM_INVALID,
	SKM_CLIENT,
	SKM_SERVER,
} SKMKIND;

typedef enum eEPOLLDATAKIND {
	EPD_INVALID,
	EPD_SOCKMGR,
	EPD_SOCK,
	EPD_DGSKMGR,
} EPOLLDATAKIND;

/* sock event buffer definition */
typedef enum eESOCKEVENT{
	SOCK_EVENT_INVALID,
	SOCK_EVENT_CLOSE,
	SOCK_EVENT_ADDSOCK,
	SOCK_EVENT_SEND,
	SOCK_EVENT_APP,
	SOCK_EVENT_EPOLL,
	SOCK_EVENT_DGRAM,
	SOCK_EVENT_WORKER,
} ESOCKEVENT;



/*-------------------------------------------------------------*/
/* internal types											   */
/*-------------------------------------------------------------*/
typedef struct 	sockmgrconf
{
	int	n_record_process;
} skmconf_t;

typedef struct	sockbuf
{
	char 		*buf;
	int			blen, mlen;
} sockbuf_t;

typedef struct	epollinfo
{
	U8			type, padd[3];
	void	*ptr;
}	epollinfo_t;

typedef struct	sockmgrdata
{
	epollinfo_t	reg;		/* registeration info to epoll */
	ARRAY		skd_a;		/* array for alloc sockdata_t */
	SEARCH		skd_s;		/* search engine (key=addr mem) */
	ARRAY		wb_a, rb_a;	/* array for alloc wb, rb, eb */
	int			nwb, nrb;	/* size of wb, rb, eb */
	DSCRPTR		fd;			/* if >= 0, server socket */
	int			type;		/* sockmgr type  */
	int			seed;		/* for serial of sockdata */
	UTIME		timeout;	/* timeout (usec) */
	PROTOCOL	*proto;		/* protocol interface */
	skmconf_t	cf;			/* configuration */
	void		*data;		/* user data */
	int			(*on_accept)		(SOCK);					/* accept watcher */
	int			(*on_close)			(SOCK, int);			/* close watcher */
	char		*(*record_parser)	(char *, int *, int *);	/* protocol parcer */
	int			(*on_recv)			(SOCK, char *, int);	/* protocol callback */
	int			(*on_event)			(SOCK, char *, int);	/* event watcher */
	UTIME		(*on_poll)			(SOCK);					/* polling proc */
	int 		(*on_connect)		(SOCK, void*);			/* when connection starts
															(only from nbr_sockmgr_connect)*/
	void		(*on_mgr)			(SOCKMGR,int,char*,int);/* send event to sockmgr */
}	skmdata_t;

typedef struct 	sockdata
{
	epollinfo_t	reg;		/* registeration info to epoll */
	struct sockdata	*next;	/* next sockdata (for thread processing) */
	char		*wb, *rb;	/* read/write/event buffer */
	int			nwb, nrb;	/* used size of wb, rb, eb */
	int			serial;		/* serial number */
	UTIME		access;		/* last received time */
	UTIME 		nextpoll;	/* next polling time */
	DSCRPTR		fd;			/* abstruct socket data */
	char		addr[ADRL];	/* socket address info */
	U8			alen;		/* actual length of addr */
	U8			closereason;/* why closed? */
	U8			stat;		/* sock status (SOCKSTATE) */
	U8			padd;		/* unused */
	THREAD		thrd;		/* if processed by thread, have handle */
	U32			events;		/* epoll event */
	skmdata_t	*skm;		/* linked sockmgr */
	char		data[0];	/* application defined data */
}	sockdata_t;

typedef struct 	sockevent
{
	U32			len;
	union {
		SOCK		sk;
		skmdata_t 	*skm;
		struct { THREAD thrd; void *p; } th;
	};
	U16			type, padd;
	char		data[0];
}	sockevent_t;

typedef struct 	sockjob
{
	THREAD		thrd;		/* thread object to process this queue */
	void		*p;			/* application defined parameter to worker */
	void		(*on_event)(THREAD, THREAD, char *, size_t);/* thread event cb */
	int			active,sleep;/* thread need to active/now sleep? */
	sockdata_t	*exec;		/* list of sockdata which should handle with this task */
	sockdata_t	*free;		/* list of sockdata which should free by main thread */
	sockbuf_t	eb[2];		/* sockbuf (double buffer) */
	sockbuf_t	*reb, *web;	/* ptr to double buffer (reb for read/web for write) */
}	sockjob_t;



/*-------------------------------------------------------------*/
/* internal values											   */
/*-------------------------------------------------------------*/
static struct {
		ARRAY		skm_a;
		sockdata_t	*free;
		MUTEX		lock, evlk/* lock for event to sockmgr */;
		THPOOL		worker;
		sockjob_t	*jobs, main;
		int			jobidx, jobmax, total_fds, reserve_fds;
		sockbuf_t	eb;
		DSCRPTR		epfd;
		NIOCONF		ioc;
}		g_sock = {
					NULL,
					NULL,
					NULL, NULL,
					NULL,
					NULL, { NULL, NULL, NULL, 0, 0, NULL, NULL, {}, NULL, NULL },
					0, 0, 0, 0,
					{ NULL, 0, 0 },
					INVALID_FD,
					{0, 0}
};



/*-------------------------------------------------------------*/
/* internal methods											   */
/*-------------------------------------------------------------*/
/* skmconf_t */
NBR_INLINE void
skmconf_set_default(skmconf_t *cfg)
{
	cfg->n_record_process = 10;
}

/* skmdata_t */
NBR_INLINE int
sockmgr_new_serial(skmdata_t *skm)
{
	if (skm->seed >= 2000000000) {
		skm->seed = 1;
	}
	return skm->seed++;
}

NBR_INLINE char *
sockmgr_alloc_rb(skmdata_t *skm)
{
	return nbr_array_alloc(skm->rb_a);
}

NBR_INLINE char *
sockmgr_alloc_wb(skmdata_t *skm)
{
	return nbr_array_alloc(skm->wb_a);
}

NBR_INLINE SOCK
sockmgr_make_sock(sockdata_t *skd)
{
	SOCK sock;
	ASSERT(skd->serial != 0);
	sock.p = (void *)skd;
	sock.s = skd->serial;
	return sock;
}

NBR_INLINE void
sockmgr_free_skd(skmdata_t *skm, sockdata_t *skd)
{
	if (skd) {
		skd->thrd = NULL;
		skd->serial = 0;
		if (skd->wb) {
			nbr_array_free(skm->wb_a, skd->wb);
			skd->wb = NULL;
		}
		if (skd->rb) {
			nbr_array_free(skm->rb_a, skd->rb);
			skd->rb = NULL;
		}
		if (skm->skd_s) {
			nbr_search_mem_unregist(skm->skd_s, skd->addr, skd->alen);
		}
		nbr_array_free(skm->skd_a, skd);
	}
}

NBR_INLINE int
sock_get_wksz(skmdata_t *skm)
{
	ASSERT(nbr_array_get_size(skm->skd_a) >= sizeof(sockdata_t));
	return nbr_array_get_size(skm->skd_a) - sizeof(sockdata_t);
}

NBR_INLINE sockdata_t *
sockmgr_alloc_skd(skmdata_t *skm, char *addr, int addrlen)
{
	sockdata_t *skd;
	ASSERT(addrlen <= sizeof(skd->addr));
	if (!(skd = nbr_array_alloc(skm->skd_a))) { goto bad; }
	skd->skm = (SOCKMGR)skm;
	skd->serial = sockmgr_new_serial(skm);
	skd->stat = SS_INIT;
	skd->fd = INVALID_FD;
	skd->next = NULL;
	skd->thrd = NULL;
	skd->closereason = CLOSED_BY_INVALID;
	skd->nextpoll = skd->access = nbr_clock();
	skd->events = 0;
	skd->reg.type = EPD_SOCK;
	if (!(skd->rb = sockmgr_alloc_rb(skm))) { goto bad; }
	if (!(skd->wb = sockmgr_alloc_wb(skm))) { goto bad; }
	skd->nrb = 0;
	skd->nwb = 0;
	nbr_mem_zero(skd->data, sock_get_wksz(skm));
	if (!addr) { nbr_mem_zero(skd->addr, sizeof(skd->addr)); }
	else if (sizeof(skd->addr) < addrlen) {
		ASSERT(FALSE); goto bad;
	}
	else {
		nbr_mem_copy(skd->addr, addr, addrlen);
		skd->alen = (U8)addrlen;
	}
	if (skm->skd_s) {
		/* if skd_s is initialized, addr must not be null */
		if (!addr || addrlen <= 0) { goto bad; }
		if (nbr_search_mem_regist(skm->skd_s, skd->addr, skd->alen, skd) < 0) {
			goto bad;
		}
	}
	return skd;
bad:
	sockmgr_free_skd(skm, skd);
	return NULL;
}

NBR_INLINE int
sockmgr_get_realfd(skmdata_t *skm)
{
	return skm->proto->fd ? skm->proto->fd(skm->fd) : skm->fd;
}

NBR_INLINE int
sockmgr_expand_maxfd(int max_nfd)
{
	int cur_nfd;
#if !defined(_VALGRIND)
	if ((cur_nfd = nbr_osdep_rlimit_get(NBR_LIMIT_TYPE_FDNUM)) < max_nfd) {
		if (nbr_osdep_rlimit_set(NBR_LIMIT_TYPE_FDNUM, max_nfd) < 0) {
			SOCK_ERROUT(ERROR,RLIMIT,"fd_max: cannot change: %d->%d (%d)",
				cur_nfd, max_nfd, errno);
			return NBR_ERLIMIT;
		}
		g_sock.total_fds = max_nfd;
		return max_nfd;
	}
#endif
	return cur_nfd;
}

/* default callback functions for sockman */
static int
sockmgr_accept_watcher_noop(SOCK s)		/* accept watcher */
{
	return 0;
}

static int
sockmgr_close_watcher_noop(SOCK s, int r)		/* close watcher */
{
	return 0;
}

static int
sockmgr_recv_watcher_noop(SOCK s, char *p, int len)	/* protocol callback */
{
	return 0;
}

static int
sockmgr_event_watcher_noop(SOCK s, char *p, int len)	/* protocol callback */
{
	return 0;
}

static void
sockmgr_mgr_event_noop(SOCKMGR s, int t, char *p, int len)	/* protocol callback */
{
	return;
}

/* sockbuf_t */
NBR_INLINE int
sockbuf_alloc(sockbuf_t *skb, int size)
{
	skb->mlen = size;
	skb->blen = 0;
	return ((skb->buf = nbr_mem_alloc(size)) ? 1 : 0);
}

NBR_INLINE void
sockbuf_free(sockbuf_t *skb)
{
	nbr_mem_free(skb->buf);
	skb->buf = NULL;
	skb->mlen = skb->blen = 0;
}

/* sockjob_t */
NBR_INLINE int
sockjob_init(sockjob_t *j, int skbsz)
{
	int i;
	j->exec = j->free = NULL;
	j->active = j->sleep = 0;
	for (i = 0; i < 2; i++) {
		if (!sockbuf_alloc(&(j->eb[i]), skbsz)) {
			return NBR_EMALLOC;
		}
	}
	j->reb = &(j->eb[0]);
	j->web = &(j->eb[1]);
	return NBR_OK;
}

NBR_INLINE void
ccb_bin16(char *out, char *data, int len)
{
	ASSERT(len <= 0xFFFF && len > 2);
	/* len contains additional bytes for length */
	SET_16(out, len - 2);
	nbr_mem_copy(out + 2, data, len - 2);
}

NBR_INLINE void
ccb_bin32(char *out, char *data, int len)
{
	ASSERT(len > 4);
	/* len contains additional bytes for length */
	SET_32(out, len - 4);
	nbr_mem_copy(out + 4, data, len - 4);
}

NBR_INLINE void
ccb_text(char *out, char *data, int len)
{
	ASSERT(len > 1);
	/* len contains additional bytes for null-terminate */
	nbr_mem_copy(out, data, len - 1);
	SET_8(out + len - 1, '\n');
}

NBR_INLINE int
sockbuf_setevent(sockbuf_t *skb, sockdata_t *skd, int serial,
		int type, char *data, int len, void (*ccb)(char*, char*, int))
{
	/* caution: len is not actual length of data. but it should be
	 * the length which is written by ccb func (if ccb != NULL) */
	sockevent_t *e = (sockevent_t *)(skb->buf + skb->blen);
	int dlen = sizeof(*e) + len;
	if ((skb->mlen - skb->blen) < dlen) {
		SOCK_ERROUT(ERROR,SHORT,"setevent:%d,%d,%d",dlen,skb->blen,skb->mlen);
		return NBR_ESHORT;
	}
	e->sk.p = skd;
	e->sk.s = serial;
	e->type = type;
	e->len = dlen;
	if (ccb) { ccb(e->data, data, len); }
	else { nbr_mem_copy(e->data, data, len); }
	skb->blen += dlen;
	ASSERT(e->sk.p && e->sk.s != 0);
	return NBR_OK;
}

NBR_INLINE int
sockbuf_set_mgrevent(sockbuf_t *skb, SOCKMGR skm, int type, char *data, int len)
{
	sockevent_t *e = (sockevent_t *)(skb->buf + skb->blen);
	int dlen = sizeof(*e) + len;
	if ((skb->mlen - skb->blen) < dlen) {
		SOCK_ERROUT(ERROR,SHORT,"setevent:%d,%d,%d",dlen,skb->blen,skb->mlen);
		return NBR_ESHORT;
	}
	e->skm = skm;
	e->len = dlen;
	e->type = type;
	nbr_mem_copy(e->data, data, len);
	skb->blen += dlen;
	return NBR_OK;
}

NBR_INLINE int
sockbuf_worker_event(sockbuf_t *skb, THREAD from, char *data, int len)
{
	/* caution: len is not actual length of data. but it should be
	 * the length which is written by ccb func (if ccb != NULL) */
	sockevent_t *e = (sockevent_t *)(skb->buf + skb->blen);
	int dlen = sizeof(*e) + len;
	if ((skb->mlen - skb->blen) < dlen) {
		SOCK_ERROUT(ERROR,SHORT,"setevent:%d,%d,%d",dlen,skb->blen,skb->mlen);
		return NBR_ESHORT;
	}
	e->th.p = NULL;	/* hack. separate normal sock event(in sock_process_event) */
	e->th.thrd = from;
	e->type = SOCK_EVENT_WORKER;
	e->len = dlen;
	nbr_mem_copy(e->data, data, len);
	skb->blen += dlen;
	return NBR_OK;
}

/* sockdata_t */
NBR_INLINE int
sock_get_size(int wks)
{
	return (wks + sizeof(sockdata_t));
}

NBR_INLINE int
sock_polling(skmdata_t *skm, sockdata_t *skd)
{
	if (skd->stat == SS_CLOSING) {
		return TRUE;
	}
	UTIME ut = nbr_clock();
	if ((ut - skd->access) > skm->timeout) {
		return FALSE;
	}
	if (skm->on_poll && ut > skd->nextpoll) {
		skd->nextpoll = skm->on_poll(sockmgr_make_sock(skd));
	}
	return TRUE;
}

NBR_INLINE SOCKSTATE
sock_get_stat(sockdata_t *skd)
{
	return skd->stat;
}

NBR_INLINE void
sock_rb_shrink(sockdata_t *skd, int len)
{
	if (skd->nrb > len) {
		skd->nrb -= len;
		nbr_mem_move(skd->rb, skd->rb + len, skd->nrb);
	}
	else {
		ASSERT(skd->nrb == len);
		skd->nrb = 0;
	}
}

NBR_INLINE void
sock_wb_shrink(sockdata_t *skd, int len)
{
	if (skd->nwb > len) {
		skd->nwb -= len;
		nbr_mem_move(skd->wb, skd->wb + len, skd->nwb);
	}
	else {
		ASSERT(skd->nwb == len);
		skd->nwb = 0;
	}
}

NBR_INLINE int
sock_rb_remain(skmdata_t *skm, sockdata_t *skd)
{
	return (skm->nrb - skd->nrb);
}

NBR_INLINE int
sock_wb_remain(skmdata_t *skm, sockdata_t *skd)
{
	return (skm->nwb - skd->nwb);
}

NBR_INLINE char *
sock_rb_top(sockdata_t *skd)
{
	return skd->rb + skd->nrb;
}

NBR_INLINE char *
sock_wb_top(sockdata_t *skd)
{
	return skd->wb + skd->nwb;
}

NBR_INLINE int
sock_get_realfd(sockdata_t *skd)
{
	return skd->skm->proto->fd ? skd->skm->proto->fd(skd->fd) : skd->fd;
}

NBR_INLINE void
sock_set_stat(sockdata_t *skd, SOCKSTATE stat)
{
	skd->stat = stat;
}

NBR_INLINE void
sockjob_wakeup_if_sleep(sockjob_t *j)
{
	if (j->sleep) {
//		TRACE("%u:%llu: wakeup!\n", getpid(), nbr_time());
		if (nbr_thread_signal(j->thrd, 1) == NBR_OK) { j->sleep = 0; }
	}
}

NBR_INLINE int
sock_push_event(sockdata_t *skd, int serial, int type, char *data, int len,
		void (*ccb)(char*, char*, int))
{
	sockjob_t *j;
	int r;
	THREAD thrd = skd->thrd;
	ASSERT(skd);
	ASSERT((data && len > 0) || (!data && len <= 0));
	if (!thrd) { return NBR_EINVAL; }
	j = nbr_thread_get_data(thrd);
	THREAD_LOCK(nbr_thread_signal_mutex(thrd), error);
	if (skd->serial == serial) {
		r = sockbuf_setevent(j->web, skd, serial, type, data, len, ccb);
		sockjob_wakeup_if_sleep(j);
	}
	THREAD_UNLOCK(nbr_thread_singal_mutex(thrd));
	return r;
error:
	return NBR_EPTHREAD;
}

NBR_INLINE int
sock_push_worker_event(THREAD from, THREAD to, char *data, int len)
{
	sockjob_t *j;
	int r;
	ASSERT((data && len > 0) || (!data && len <= 0));
	if (!to && !from) { return NBR_EINVAL; }
	j = nbr_thread_get_data(to);
	THREAD_LOCK(nbr_thread_signal_mutex(to), error);
	r = sockbuf_worker_event(j->web, from, data, len);
	sockjob_wakeup_if_sleep(j);
	THREAD_UNLOCK(nbr_thread_singal_mutex(to));
	return r;
error:
	return NBR_EPTHREAD;
}

NBR_INLINE int
sock_addjob(sockdata_t *skd)
{
	int r;
	sockjob_t *assign;
	if (!MT_MODE()) { return 0; }
	else {
		ASSERT(g_sock.jobmax > 0 && skd->thrd == NULL);
		assign = &(g_sock.jobs[g_sock.jobidx++]);
		g_sock.jobidx = (g_sock.jobidx % g_sock.jobmax);
		skd->thrd = assign->thrd;
		if ((r = sock_push_event(skd, skd->serial,
			SOCK_EVENT_ADDSOCK, NULL, 0, NULL)) == NBR_OK) {
			/* because if assign->exec, then sock_process_job keep on running
			 * and execute event handle loop, so no need to wake up. */
			if (assign->sleep) {
				if (nbr_thread_signal(assign->thrd, 1) == NBR_OK) {
					assign->sleep = 0;
				}
			}
		}
		return r;
	}
}

#if defined(_DEBUG)
#define sock_set_close(skd, r) sock_set_close_dbg(skd, r, __FILE__, __LINE__)
NBR_INLINE void
sock_set_close_dbg(sockdata_t *skd, int reason, const char *file, int line)
{
//	char buf[256];
//	nbr_sock_get_local_addr(sockmgr_make_sock(skd), buf, sizeof(buf));
//	TRACE("%u: sock_set_close(%u:%s) from %s(%u)\n", getpid(), skd->fd, buf, file, line);
#else
NBR_INLINE void
sock_set_close(sockdata_t *skd, int reason)
{
#endif
	sock_set_stat(skd, SS_CLOSING);
	skd->closereason = reason;
}

#if defined(_DEBUG)
#define sock_mark_close(skd, s, r) sock_mark_close_dbg(skd, s, r, __FILE__, __LINE__)
NBR_INLINE int
sock_mark_close_dbg(sockdata_t *skd, int serial, int reason, const char *file, int line)
{
//	char buf[256];
//	nbr_sock_get_local_addr(sockmgr_make_sock(skd), buf, sizeof(buf));
//	TRACE("%u: sock_mark_close(%u:%s) from %s(%u)\n", getpid(), skd->fd, buf, file, line);
#else
NBR_INLINE int
sock_mark_close(sockdata_t *skd, int serial, int reason)
{
#endif
	if (!MT_MODE()) {
		sock_set_close(skd, reason);
		return NBR_OK;
	}
	return sock_push_event(skd, serial, SOCK_EVENT_CLOSE,
			(char *)&reason, sizeof(reason), NULL);
}

NBR_INLINE int
sock_mark_epoll(sockdata_t *skd, int serial, U32 events)
{
	if (!MT_MODE()) {
		skd->events = events;
		return NBR_OK;
	}
	return sock_push_event(skd, serial, SOCK_EVENT_EPOLL,
			(char *)&events, sizeof(events), NULL);
}

NBR_INLINE int
sock_send_data(sockdata_t *skd, int serial, char *data, int len,
		void (*ccb)(char*, char*, int))
{
	return sock_push_event(skd, serial, SOCK_EVENT_SEND, data, len, ccb);
}

NBR_INLINE int
sock_recv_dgram(sockdata_t* skd, int serial, char *data, int len)
{
	if (!MT_MODE()) {
		if (sock_rb_remain(skd->skm, skd) >= len) {
			nbr_mem_copy(skd->rb, data, len);
			skd->nrb += len;
			return len;
		}
		return NBR_ESHORT;
	}
	return sock_push_event(skd, serial, SOCK_EVENT_DGRAM,
			data, len, NULL);
}

NBR_INLINE int
sock_send_event(SOCK sk, char *data, int len)
{
	sockdata_t *skd = sk.p;
	if (!MT_MODE()) {
		return sockbuf_setevent(&(g_sock.eb), skd, sk.s,
				SOCK_EVENT_APP, data, len, NULL);
	}
	return sock_push_event(skd, sk.s, SOCK_EVENT_APP, data, len, NULL);
}


/* epoll */
NBR_INLINE int
sockmgr_attach_epoll(int epfd, skmdata_t *skm)
{
	struct epoll_event e;
	e.events = EPOLLIN;
	e.data.ptr = &(skm->reg);
	return epoll_ctl(epfd, EPOLL_CTL_ADD, sockmgr_get_realfd(skm), &e);
}

NBR_INLINE int
sockmgr_detach_epoll(int epfd, skmdata_t *skm)
{
	struct epoll_event e;
	return epoll_ctl(epfd, EPOLL_CTL_DEL, sockmgr_get_realfd(skm), &e);
}

//STATIC_ASSERT(sizeof(void*) == sizeof(U32));
NBR_INLINE int
sock_attach_epoll(int epfd, sockdata_t *skd, int modify, int r, int w)
{
	struct epoll_event e;
	if (!r && !w) { return 0; }
	e.events = (EPOLLONESHOT | EPOLLRDHUP);
	if (r) { e.events |= EPOLLIN; }
	if (w) { e.events |= EPOLLOUT; }
	skd->reg.ptr = skd;
	e.data.ptr = &(skd->reg);
	//TRACE("%u: attach epoll: %08x %u %u\n", getpid(), e.events, r, w);
	if (modify) { return epoll_ctl(epfd, EPOLL_CTL_MOD, sock_get_realfd(skd), &e); }
	else { return epoll_ctl(epfd, EPOLL_CTL_ADD, sock_get_realfd(skd), &e); }
}

NBR_INLINE int
sock_detach_epoll(int epfd, sockdata_t *skd)
{
	struct epoll_event e;
	return epoll_ctl(epfd, EPOLL_CTL_DEL, sock_get_realfd(skd), &e);
}

NBR_INLINE void
sockmgr_cleanup_skd(sockdata_t *skd)
{
	if (!skd->skm->proto->dgram) {
		sock_detach_epoll(g_sock.epfd, skd);
		skd->skm->proto->close(skd->fd);
	}
	sockmgr_free_skd(skd->skm, skd);
}

NBR_INLINE U32
events_from(EVENT *e)
{
	return e->events;
}

NBR_INLINE epollinfo_t *
reginfo_from(EVENT *e)
{
	return (epollinfo_t *)(e->data.ptr);
}

NBR_INLINE EPOLLDATAKIND
type_from(EVENT *e)
{
	return reginfo_from(e)->type;
}

NBR_INLINE skmdata_t *
sockmgr_from(EVENT *e)
{
	return (skmdata_t *)reginfo_from(e)->ptr;
}

NBR_INLINE sockdata_t *
sock_from(EVENT *e)
{
	return (sockdata_t *)reginfo_from(e)->ptr;;
}

NBR_INLINE BOOL
sock_writable(sockdata_t *skd)
{
	return skd->events & EPOLLOUT;
}

NBR_INLINE void
sock_set_writable(sockdata_t *skd, BOOL on)
{
	if (on) { skd->events |= EPOLLOUT; }
	else { skd->events &= ~(EPOLLOUT); }
}

NBR_INLINE BOOL
sock_readable(sockdata_t *skd)
{
	return skd->events & EPOLLIN;
}

NBR_INLINE void
sock_set_readable(sockdata_t *skd, BOOL on)
{
	if (on) { skd->events |= EPOLLIN; }
	else { skd->events &= ~(EPOLLIN); }
}

NBR_INLINE BOOL
sock_closed(sockdata_t *skd)
{
	return skd->events & EPOLLRDHUP;
}

NBR_INLINE BOOL
sock_error(sockdata_t *skd)
{
	return (skd->events & EPOLLERR) || (skd->events & EPOLLHUP);
}

NBR_INLINE int
sock_attach_epoll_on_accept(sockdata_t *skd)
{
	int r, w;
	switch(sock_get_stat(skd)) {
	case SS_CONNECTING:
		r = 0; w = 1; break;
	case SS_DGCONNECT:
		sock_set_writable(skd, TRUE);
		r = 0; w = 0; break;
	case SS_CONNECT:
		r = w = 1; break;
	default:
		ASSERT(FALSE);
		return -1;
	}
	/* FALSE means first register to epoll */
	return sock_attach_epoll(g_sock.epfd, skd, FALSE, r, w);
}

NBR_INLINE int
sock_handshake(sockdata_t *skd)
{
	int r;
	if (0 == (r = skd->skm->proto->handshake(skd->fd,
			sock_readable(skd), sock_writable(skd)))) {
		sock_set_writable(skd, FALSE);
		sock_set_readable(skd, FALSE);
	}
	return r;
}

NBR_INLINE void
sockjob_process_event(sockjob_t *j, sockevent_t *e)
{
	int dlen;
	sockdata_t *skd = e->sk.p;
	if (!skd || skd->serial != e->sk.s) {
		if (e->type == SOCK_EVENT_WORKER && j->on_event) {
			j->on_event(e->th.thrd, j->thrd, e->data, (e->len - sizeof(*e)));
			return;
		}
		TRACE("old event %p %p %u %u\n", skd, skd->thrd, skd->serial, e->sk.s);
		return;
	}
	switch(e->type) {
	case SOCK_EVENT_CLOSE:
		sock_set_close(skd, GET_32(e->data));
		break;
	case SOCK_EVENT_ADDSOCK:
		skd->next = j->exec;
		j->exec = skd;
		ASSERT(skd->thrd == j->thrd);
		if (sock_attach_epoll_on_accept(skd) < 0) {
			sock_set_close(skd, CLOSED_BY_ERROR);
		}
		break;
	case SOCK_EVENT_SEND:
		dlen = (e->len - sizeof(*e));
		if (sock_wb_remain(skd->skm, skd) >= dlen) {
//			TRACE("%u: sock_event_send: to %p, wb: %u->", getpid(), skd, skd->nwb);
//	ASSERT(sock_writable(skd)); if addsock and send is processed same iter, it happen
			nbr_mem_copy(sock_wb_top(skd), e->data, dlen);
			skd->nwb += dlen;
//			TRACE("%ubyte\n", skd->nwb);
		}
		else { sock_set_close(skd, CLOSED_BY_ERROR); }
		break;
	case SOCK_EVENT_APP:
		dlen = (e->len - sizeof(*e));
		//TRACE("sock_event_app: to %p, %ubyte\n", skd, dlen);
		skd->skm->on_event(e->sk, (char *)e->data, dlen);
		break;
	case SOCK_EVENT_EPOLL:
		ASSERT((e->len - sizeof(*e)) == sizeof(U32));
		skd->events |= GET_32(e->data);
		if (sock_error(skd)) {
//			TRACE("%u: event=%08x/new=%08x\n", getpid(), /skd->events, GET_32(e->data));
			sock_set_close(skd, CLOSED_BY_ERROR);
		}
		else if (sock_closed(skd)) {
			sock_set_close(skd, CLOSED_BY_REMOTE);
		}
//		TRACE("%u: epoll skd(%p):ev=%08x %08x\n", getpid(), skd, skd->events, GET_32(e->data));
		break;
	case SOCK_EVENT_DGRAM:
		dlen = (e->len - sizeof(*e));
		skd->access = nbr_clock();
		if (sock_rb_remain(skd->skm, skd) >= dlen) {
			nbr_mem_copy(sock_rb_top(skd), e->data, dlen);
			skd->nrb += dlen;
//			TRACE("event dgram %p %u\n", skd, skd->nrb);
		}
		else {
			ASSERT(FALSE);
			sock_set_close(skd, CLOSED_BY_ERROR);
		}
		break;
	}
}

void *
sock_process_job(THREAD thrd)
{
	sockjob_t *j = nbr_thread_get_data(thrd);
	sockevent_t *e, *le;
	sockbuf_t  *skb;
	sockdata_t *sk, *psk, *ppsk, *last;
	int n_sleep;
	nbr_thread_setcancelstate(0);	/* non-cancelable */
	for (;;) {
//		if (g_sock.ioc.job_idle_sleep_us > 0) {
//			nbr_osdep_sleep(g_sock.ioc.job_idle_sleep_us);
//		}
		if (j->web->blen <= 0) {
		}
		else if (NBR_OK == nbr_mutex_lock(nbr_thread_signal_mutex(thrd))) {
			/* flip double buffer */
			skb = j->reb;
			j->reb = j->web;
			j->web = skb;
			if (NBR_OK != nbr_mutex_unlock(nbr_thread_signal_mutex(thrd))) {
				break;
			}
			/* process event */
			e = (sockevent_t *)j->reb->buf;
			le = (sockevent_t *)(j->reb->buf + j->reb->blen);
			while (e < le) {
				ASSERT(e->len > 0);
				sockjob_process_event(j, e);
				e = (sockevent_t *)(((char *)e) + e->len);
			}
			j->reb->blen = 0;
		}
		nbr_thread_testcancel();	/* if cancel request, stop this thread */
		j->active = 0;
		sk = j->exec;
		ppsk = NULL;
		while((psk = sk)) {
			sk = psk->next;
			if (nbr_sock_io(psk)) {
				if (sock_readable(psk)) { j->active = 1; }
				ppsk = psk;
			}
			else {
				if (!j->free) { last = psk; }
				psk->next = j->free;
				j->free = psk;
				if (ppsk) { ppsk->next = sk; }
				else { j->exec = sk; }
			}
		}
		if (j->free) {
			if (NBR_OK == nbr_mutex_lock(g_sock.lock)) {
				last->next = g_sock.free;
				g_sock.free = j->free;
				j->free = NULL;
				if (NBR_OK != nbr_mutex_unlock(g_sock.lock)) { break; }
			}
		}
#if 1
		if (j->active || j->web->blen > 0) {}
		else {
			n_sleep = j->exec ? g_sock.ioc.job_idle_sleep_us : -1;
//			TRACE("%u: %llu: sleep : n_sleep = %d\n", getpid(), nbr_time(), n_sleep);
			nbr_thread_setcancelstate(1);	/* cancelable */
			j->sleep = 1;
			if (NBR_OK == nbr_thread_wait_signal(thrd, 1, n_sleep)) {
				nbr_mutex_unlock(nbr_thread_signal_mutex(thrd));
			}
			j->sleep = 0;
			nbr_thread_setcancelstate(0);	/* non-cancelable */
		}
#endif
	}
	return thrd;
}

/*-------------------------------------------------------------*/
/* external methods											   */
/*-------------------------------------------------------------*/

NBR_API void	nbr_sock_set_nioconf(NIOCONF ioc)
{
	nbr_mem_copy(&(g_sock.ioc), &ioc, sizeof(ioc));
}

NBR_API NIOCONF nbr_sock_nioconf()
{
	return g_sock.ioc;
}

/* SOCKMGR */
int
nbr_sock_init(int max, int max_nfd, int n_worker, int skbsz, int skbmain)
{
	int i;
	sockjob_t *j;
	nbr_sock_fin();

	g_sock.skm_a = nbr_array_create(max, sizeof(skmdata_t), 1);
	if (!g_sock.skm_a) {
		SOCK_ERROUT(ERROR,INTERNAL,"array_create: %d", max);
		goto error;
	}
	if (n_worker > 0) {
		if (!(g_sock.worker = nbr_thpool_create(n_worker))) {
			SOCK_ERROUT(ERROR,PTHREAD,"thpool_create: %d", n_worker);
			goto error;
		}
		if (!(g_sock.jobs = nbr_mem_alloc(n_worker * sizeof(sockjob_t)))) {
			SOCK_ERROUT(ERROR,MALLOC,"mem_alloc: %d", n_worker);
			goto error;
		}
		nbr_mem_zero(g_sock.jobs, n_worker * sizeof(sockjob_t));
		g_sock.jobmax = n_worker;
		g_sock.jobidx = 0;
		for (i = 0; i < n_worker; i++) {
			j = g_sock.jobs + i;
			if (sockjob_init(j, skbsz) < 0) {
				SOCK_ERROUT(ERROR,MALLOC,"sockjob_init: %d,%u", LASTERR, skbsz);
				goto error;
			}
			if (!(j->thrd = nbr_thread_create(g_sock.worker, j, sock_process_job))) {
				SOCK_ERROUT(ERROR,PTHREAD,"thread_create: %d", LASTERR);
				goto error;
			}
		}
		if (sockjob_init(&(g_sock.main), skbmain) < 0) {
			SOCK_ERROUT(ERROR,MALLOC,"sockjob_init(main): %d,%u", LASTERR, skbmain);
			goto error;
		}
	}
	else {
		if (!sockbuf_alloc(&(g_sock.eb), skbsz)) {
			SOCK_ERROUT(ERROR,MALLOC,"nbr_mem_alloc: g_sock.eb");
			goto error;
		}
	}
	if ((g_sock.epfd = epoll_create(max_nfd)) < 0) {
		SOCK_ERROUT(ERROR,EPOLL,"epoll_create: %d,errno=%d",max_nfd,errno);
		goto error;
	}
	g_sock.reserve_fds = 1;/* for epoll fd */
	if ((g_sock.total_fds = sockmgr_expand_maxfd(max_nfd)) < 0) {
		SOCK_ERROUT(ERROR,EXPIRE,"expand maxfd: %d,errno=%d",max_nfd,errno);
		goto error;
	}
	if (!(g_sock.lock = nbr_mutex_create())) {
		SOCK_ERROUT(ERROR,PTHREAD,"mutex create: errno=%d",errno);
		goto error;
	}
	if (!(g_sock.evlk = nbr_mutex_create())) {
		SOCK_ERROUT(ERROR,PTHREAD,"mutex create (event): errno=%d",errno);
		goto error;
	}
	return NBR_OK;
error:
	nbr_sock_fin();
	return LASTERR;
}

int
nbr_sock_fin()
{
	int i, ii;
	sockjob_t *j;
	SOCKMGR *skm, *pskm;
	sockdata_t *skd, *pskd;
	if (g_sock.worker) {
		nbr_thpool_destroy(g_sock.worker);
		g_sock.worker = NULL;
	}
	skd = g_sock.free;
	while((pskd = skd)) {
		skd = skd->next;
		sockmgr_cleanup_skd(pskd);
	}
	if (g_sock.skm_a) {
		skm = nbr_array_get_first(g_sock.skm_a);
		while((pskm = skm)) {
			skm = nbr_array_get_next(g_sock.skm_a, skm);
			nbr_sockmgr_destroy(pskm);
		}
		nbr_array_destroy(g_sock.skm_a);
		g_sock.skm_a = NULL;
	}
	g_sock.free = NULL;
	if (g_sock.epfd != INVALID_FD) {
		nbr_epoll_destroy(g_sock.epfd);
		g_sock.epfd = INVALID_FD;
	}
	if (g_sock.eb.buf) {
		sockbuf_free(&(g_sock.eb));
		g_sock.eb.buf = NULL;
	}
	if (g_sock.jobs) {
		for (i = 0; i < g_sock.jobmax; i++) {
			j = g_sock.jobs + i;
			for (ii = 0; ii < 2; ii++) {
				sockbuf_free(&(j->eb[ii]));
			}
		}
		nbr_mem_free(g_sock.jobs);
		g_sock.jobs = NULL;
	}
	for (i = 0; i < 2; i++) {
		sockbuf_free(&(g_sock.main.eb[i]));
	}
	if (g_sock.lock) {
		nbr_mutex_destroy(g_sock.lock);
		g_sock.lock = NULL;
	}
	if (g_sock.evlk) {
		nbr_mutex_destroy(g_sock.evlk);
		g_sock.evlk = NULL;
	}
	g_sock.total_fds = 0;
	return NBR_OK;
}

DSCRPTR
nbr_sockmgr_get_listenfd(SOCKMGR s)
{
	skmdata_t *skm = s;
	return skm->fd;
}

NBR_API int
nbr_sock_get_worker(THREAD ths[], int n_th)
{
	int i;
	if (g_sock.jobmax > n_th) {
		return NBR_ESHORT;
	}
	for (i = 0; i < n_th; i++) {
		ths[i] = g_sock.jobs[i].thrd;
	}
	return g_sock.jobmax;
}

NBR_API void*
nbr_sock_get_worker_data_from(SOCK s)
{
	sockdata_t *skd = s.p;
	THREAD th = skd->thrd;
	return nbr_sock_get_worker_data(th);
}

NBR_API void *
nbr_sock_get_worker_data(THREAD th)
{
	ASSERT(th);
	sockjob_t *j = nbr_thread_get_data(th);
	return j ? j->p : NULL;
}

NBR_API int
nbr_sock_set_worker_data(THREAD th, void *p,
		void (*on_event)(THREAD, THREAD, char *, size_t))
{
	ASSERT(th);
	sockjob_t *j = nbr_thread_get_data(th);
	if (!j) {
		return NBR_ENOTFOUND;
	}
	j->p = p;
	j->on_event = on_event;
	return NBR_OK;
}

NBR_API int
nbr_sock_worker_bcast_event(THREAD from, char *p, size_t l)
{
	int i, r;
	for (i = 0; i < g_sock.jobmax; i++) {
		if ((r = nbr_sock_worker_event(from, g_sock.jobs[i].thrd, p, l)) < 0) {
			return r;
		}
	}
	return l;
}


NBR_API int
nbr_sock_worker_event(THREAD from, THREAD to, char *p, size_t l)
{
	return sock_push_worker_event(from, to, p, l);
}

NBR_API int
nbr_sock_worker_is_current(SOCK sk)
{
	sockdata_t *skd = (sockdata_t *)sk.p;
	return nbr_thread_is_current(skd->thrd);
}

NBR_API SOCKMGR
nbr_sockmgr_create(
			int nrb, int nwb,
			int max_sock,
			int workmem,
			int timeout_sec,
			const char *addr,
			PROTOCOL *proto,
			void *proto_p,
			int option)
{
	int fd_max;
	skmdata_t *skm;
	SKCONF skc = {timeout_sec, nrb, nwb, proto_p};
	if (!(skm = (skmdata_t *)nbr_array_alloc(g_sock.skm_a))) {
		nbr_mem_zero(skm, sizeof(*skm));
		skm->fd = INVALID_FD;
		goto bad;
	}
	if (!proto) {
		proto = nbr_proto_tcp();	/* default */
	}
	nbr_mem_zero(skm, sizeof(*skm));
	skm->fd	= INVALID_FD;
	fd_max = (g_sock.reserve_fds + max_sock + 16);	/* +16 for misc work (eg. gethostbyname) */
	if ((g_sock.total_fds = sockmgr_expand_maxfd(fd_max)) < 0) {
		SOCK_ERROUT(ERROR,INTERNAL,"expand_maxfd: to %d(%d)",
			fd_max,g_sock.reserve_fds);
		goto bad;
	}
	g_sock.reserve_fds += max_sock;
	/* first, set invalid value for goto bad. */
	skm->type = SKM_INVALID;
	skm->proto = proto;
	skm->seed = 1;
	skm->timeout = TO_UTIME(timeout_sec);
	skm->skd_s = NULL;
	/* create mempool */
	if (!(skm->skd_a = nbr_array_create(
		max_sock, sock_get_size(workmem), option))) {
		SOCK_ERROUT(ERROR,INTERNAL,"array_create: skd: %d", max_sock);
		goto bad;
	}
	if (!(skm->rb_a = nbr_array_create(max_sock, nrb, option))) {
		SOCK_ERROUT(ERROR,INTERNAL,"array_create: rb: %d", max_sock);
		goto bad;
	}
	if (!(skm->wb_a = nbr_array_create(max_sock, nwb, option))) {
		SOCK_ERROUT(ERROR,INTERNAL,"array_create: wb: %d", max_sock);
		goto bad;
	}
	skm->nwb = nwb;
	skm->nrb = nrb;
	/* setup default callback */
	skm->on_accept = sockmgr_accept_watcher_noop;	/* accept watcher */
	skm->on_close = sockmgr_close_watcher_noop;		/* close watcher */
	skm->record_parser = nbr_sock_rparser_raw;		/* protocol parcer */
	skm->on_recv = sockmgr_recv_watcher_noop;		/* protocol callback */
	skm->on_event = sockmgr_event_watcher_noop;		/* event watcher */
	skm->on_poll = NULL;
	skm->on_connect = NULL;
	skm->on_mgr = sockmgr_mgr_event_noop;
	/* set default config */
	skmconf_set_default(&(skm->cf));
	/* listener socket? */
	if (addr) {
		if (proto->dgram) {
			if (!(skm->skd_s = nbr_search_init_mem_engine(
				max_sock, option, max_sock, ADRL))) {
				SOCK_ERROUT(ERROR,MALLOC,"mem_engine: %u\n", max_sock);
				goto bad;
			}
			skc.rblen *= max_sock;
			skc.wblen *= max_sock;
		}
		if ((skm->fd = proto->socket(addr, &skc)) == INVALID_FD) {
			SOCK_ERROUT(ERROR,SOCKET,"socket: %s,errno=%d", addr,errno);
			goto bad;
		}
		skm->reg.type = skm->proto->dgram ? EPD_DGSKMGR : EPD_SOCKMGR;
		skm->reg.ptr = skm;
		if (sockmgr_attach_epoll(g_sock.epfd, skm) < 0) {
			SOCK_ERROUT(ERROR,EPOLL,"attach: %s,errno=%d", addr,errno);
			goto bad;
		}
	}
	return (SOCKMGR)skm;
bad:
	if (skm) {
		nbr_sockmgr_destroy(skm);
	}
	return NULL;
}

NBR_API int
nbr_sockmgr_destroy(SOCKMGR s)
{
	ASSERT(s);
	skmdata_t *skm = s;
	sockdata_t *skd, *pskd;
	if (skm->skd_a) {
		skd = nbr_array_get_first(skm->skd_a);
		while((pskd = skd)) {
			skd = nbr_array_get_next(skm->skd_a, skd);
			pskd->thrd = NULL;	/* detach from worker thread */
			sock_set_stat(pskd, SS_CLOSE);
			nbr_sock_io(pskd);
			sockmgr_cleanup_skd(pskd);
		}
		nbr_array_destroy(skm->skd_a);
		skm->skd_a = NULL;
	}
	if (skm->rb_a) {
		nbr_array_destroy(skm->rb_a);
		skm->rb_a = NULL;
	}
	if (skm->wb_a) {
		nbr_array_destroy(skm->wb_a);
		skm->wb_a = NULL;
	}
	if (skm->fd != INVALID_FD) {
		sockmgr_detach_epoll(g_sock.epfd, skm);
		ASSERT(skm->proto);
		skm->proto->close(skm->fd);
		skm->fd = INVALID_FD;
	}
	return nbr_array_free(g_sock.skm_a, skm);
}

NBR_API SOCK
nbr_sockmgr_connect(SOCKMGR s, const char *address, void *proto_p, void *p)
{
	ASSERT(s);
	skmdata_t *skm = s;
	sockdata_t *skd = NULL;
	SOCK sk;
	char addr[256];
	int addrlen = sizeof(addr), r;
	SKCONF skc = {skm->timeout, skm->nrb, skm->nwb, proto_p };

	if (skm->proto->str2addr(address, addr, &addrlen) < 0) {
		SOCK_ERROUT(ERROR,HOSTBYNAME,"s2a: %s,errno=%d", address,errno);
		goto bad;
	}
	if (skm->skd_s && (skd = nbr_search_mem_get(skm->skd_s, addr, addrlen))) {
		TRACE("%u: use existing sock %p\n", getpid(), skd);
		ASSERT(FALSE);
		return sockmgr_make_sock(skd);
	}
	if (!(skd = sockmgr_alloc_skd(skm, addr, addrlen))) {
		SOCK_ERROUT(ERROR,EXPIRE,"alloc_skd: %s,errno=%d",address,errno);
		goto bad;
	}
	if ((skd->fd = skm->proto->socket(NULL, &skc)) == INVALID_FD) {
		SOCK_ERROUT(ERROR,SOCKET,"socket: %s,errno=%d", address,errno);
		goto bad;
	}
	if (skm->proto->connect(skd->fd, addr, addrlen) < 0) {
		SOCK_ERROUT(ERROR,CONNECT,"connect: %s,errno=%d", address,errno);
		goto bad;
	}
	/* if callback specified, call it. */
	if (skm->on_connect) {
		if (skm->on_connect(sockmgr_make_sock(skd), p) < 0) {
			SOCK_LOG(INFO,"connect blocked by app\n");
			goto bad;
		}
	}
	sock_set_stat(skd, SS_CONNECTING);
	if ((r = sock_addjob(skd)) < 0) {
		SOCK_ERROUT(ERROR,SHORT,"epoll: addjob fail %d", r);
		goto bad;
	}
	return sockmgr_make_sock(skd);
bad:
	if (skd) {
		if (skd->fd != INVALID_FD) { skm->proto->close(skd->fd); }
		sockmgr_free_skd(skm, skd);
	}
	nbr_sock_clear(&sk);
	return sk;
}

NBR_API int
nbr_sockmgr_mcast(SOCKMGR s, const char *address, char *data, int len)
{
	char addr[256];
	int addrlen = sizeof(addr);
	skmdata_t *skm = s;
	if (!skm->proto->dgram || skm->fd == INVALID_FD) {
		SOCK_ERROUT(ERROR,INVAL,"mcast: %s,%d,%d", address,skm->proto->dgram,skm->fd);
		return NBR_EINVAL;
	}
	if (skm->proto->str2addr(address, addr, &addrlen) < 0) {
		SOCK_ERROUT(ERROR,HOSTBYNAME,"mcast: s2a: %s,errno=%d", address,errno);
		return NBR_EHOSTBYNAME;
	}
	if (skm->proto->send(skm->fd, data, len, 0, addr, addrlen) < 0) {
		SOCK_ERROUT(ERROR,SOCKET,"mcast: send: %s,errno=%d", address,errno);
		return NBR_ESOCKET;
	}
	return NBR_OK;
}

NBR_API void
nbr_sockmgr_set_data(SOCKMGR s, void *p)
{
	skmdata_t *skm = s;
	skm->data = p;
}

NBR_API void*
nbr_sockmgr_get_data(SOCKMGR s)
{
	skmdata_t *skm = s;
	return skm->data;
}

NBR_API int
nbr_sockmgr_get_addr(SOCKMGR s, char *buf, int len)
{
	skmdata_t *skm = s;
	char addr[ADRL];
	int alen = ADRL, r;
	if ((r = nbr_osdep_sockname(sockmgr_get_realfd(skm), addr, &alen)) < 0) {
		return r;
	}
	return skm->proto->addr2str(addr, alen, buf, len);
}

NBR_API int
nbr_sockmgr_get_ifaddr(SOCKMGR s, const char *ifn, char *buf, int len)
{
	skmdata_t *skm = s;
	char addr[ADRL];
	int alen = ADRL, r;
	if ((r = nbr_osdep_sockname(sockmgr_get_realfd(skm), addr, &alen)) < 0) {
		return r;
	}
	return nbr_osdep_ifaddr(sockmgr_get_realfd(skm), ifn, buf, &len, addr, alen);
}

NBR_API int
nbr_sockmgr_event(SOCKMGR s, int type, char *p, int len)
{
	int r = NBR_EPTHREAD;
	if (!MT_MODE()) {
		return NBR_ENOTSUPPORT;
	}
	THREAD_LOCK(g_sock.evlk, error);
	r = sockbuf_set_mgrevent(g_sock.main.web, s, type, p, len);
	THREAD_UNLOCK(g_sock.evlk);
error:
	return r;
}

NBR_API int
nbr_sockmgr_get_stat(SOCKMGR s, SKMSTAT *st)
{
	skmdata_t *skm = s;
	st->n_connection = nbr_array_use(skm->skd_a);
	st->recv_sec = st->send_sec = 0;
	st->total_recv = st->total_send = 0;
	return NBR_OK;
}

NBR_API SOCKMGR
nbr_sock_get_mgr(SOCK s)
{
	sockdata_t *skd = s.p;
	return skd->skm;
}

NBR_API int
nbr_sockmgr_workmem_size(SOCKMGR s)
{
	ASSERT(s);
	skmdata_t *skm = s;
	return (nbr_array_get_size(skm->skd_a) - sizeof(sockdata_t));
}

NBR_API int
nbr_sockmgr_acceptable_size(SOCKMGR s)
{
	ASSERT(s);
	skmdata_t *skm = s;
	return nbr_array_max(skm->skd_a);
}

NBR_API int
nbr_sockman_iterate_sock(SOCKMGR s, void *p, int (*fn)(SOCK, void*))
{
	ASSERT(s);
	skmdata_t *skm = s;
	sockdata_t *skd, *pskd;
	int ret, cnt = 0;
	skd = nbr_array_get_first(skm->skd_a);
	while((pskd = skd)) {
		if ((ret = fn(sockmgr_make_sock(skd), p)) < 0) {
			return ret;
		}
		cnt++;
	}
	return cnt;
}


/* SOCK */
NBR_API int
nbr_sock_close(SOCK s)
{
	ASSERT(s.p);
	sockdata_t *skd = s.p;
//TRACE("nbr_sock_close: %p(%d) close\n", skd, skd->serial);
	if (nbr_sock_valid(s)) {
		if (skd->thrd) {
			sock_mark_close(skd, s.s, CLOSED_BY_APPLICATION);
		}
		else {
			sock_set_stat(skd, SS_CLOSING);
		}
	}
	return NBR_OK;
}

NBR_API int
nbr_sock_event(SOCK s, char *p, int len)
{
	if (!nbr_sock_valid(s)) { return NBR_EINVAL; }
	return sock_send_event(s, p, len);
}

NBR_API int
nbr_sock_valid(SOCK s)
{
	sockdata_t *skd = s.p;
	if (!skd) {
		return 0;	/* maybe initialized by nbr_sock_clear */
	}
	return (int)(skd->serial == s.s &&
		(skd->stat <= SS_CLOSING || skd->stat >= SS_CONNECTING));
}

NBR_API void
nbr_sock_clear(SOCK *p)
{
	p->s = 0;
	p->p = NULL;
}

NBR_API int
nbr_sock_is_same(SOCK s1, SOCK s2)
{
	return (s1.s == s2.s && s1.p == s2.p);
}

NBR_API int
nbr_sock_writable(SOCK s)
{
	sockdata_t *skd = s.p;
	if (!skd) { return 0; }
//	TRACE("sock_writable: stat=%u\n", skd->stat);
	if ((skd->stat == SS_CONNECT || skd->stat == SS_DGCONNECT)
		&& skd->serial == s.s) {
		ASSERT(skd->skm->nwb >= skd->nwb);
		return skd->skm->nwb - skd->nwb;
	}
	return 0;
}

NBR_API int
nbr_sock_get_addr(SOCK s, char *buf, int len)
{
	ASSERT(s.p);
	sockdata_t *skd = s.p;
	if (!skd) { return NBR_EINVAL; }
	return skd->skm->proto->addr2str(skd->addr, skd->alen, buf, len);
}

NBR_API int
nbr_sock_get_local_addr(SOCK s,char *buf, int len)
{
	sockdata_t *skd = s.p;
	char addr[ADRL];
	int alen = ADRL, r;
	if ((r = nbr_osdep_sockname(sock_get_realfd(skd), addr, &alen)) < 0) {
		ASSERT(FALSE);
		return r;
	}
	return skd->skm->proto->addr2str(addr, alen, buf, len);
}

NBR_API const void*
nbr_sock_get_ref(SOCK s, int *len)
{
	ASSERT(s.p);
	sockdata_t *skd = s.p;
	if (len) { *len = sock_get_wksz(skd->skm); }
	/* check this connection already differed or not */
	return (skd && skd->serial == s.s) ? (void *)skd->data : NULL;
}

NBR_API void*
nbr_sock_get_data(SOCK s, int *len)
{
	ASSERT(s.p);
	sockdata_t *skd = s.p;
	if (len) { *len = sock_get_wksz(skd->skm); }
	if (!skd->thrd) { goto getdata; }
	if (nbr_thread_get_id(skd->thrd) != nbr_thread_get_curid()) {
		TRACE("from another thread: belong=%u,from=%u\n",
				nbr_thread_get_id(skd->thrd), nbr_thread_get_curid());
		return NULL;
	}
	/* check this connection already differed or not */
getdata:
	return (skd && skd->serial == s.s) ? (void *)skd->data : NULL;
}

/* configure callback: for sock_t */
NBR_API void
nbr_sockmgr_set_callback(SOCKMGR s,
					char *(*rp)(char*, int*, int*),
					int (*aw)(SOCK),
					int (*cw)(SOCK, int),
					int (*pp)(SOCK, char*, int),
					int (*eh)(SOCK, char*, int),
					UTIME (*poll)(SOCK))
{
	ASSERT(s);
	skmdata_t *skm = s;
	skm->record_parser = rp ? rp : nbr_sock_rparser_raw;
	skm->on_accept = aw ? aw : sockmgr_accept_watcher_noop;
	skm->on_close = cw ? cw : sockmgr_close_watcher_noop;
	skm->on_recv = pp ? pp : sockmgr_recv_watcher_noop;
	skm->on_event = eh ? eh : sockmgr_event_watcher_noop;
	skm->on_poll = poll;
}

NBR_API void
nbr_sockmgr_set_connect_cb(SOCKMGR s,
					int (*oc)(SOCK, void*))
{
	skmdata_t *skm = s;
	skm->on_connect = oc;
}

NBR_API void
nbr_sockmgr_set_mgrevent_cb(SOCKMGR s,
					void (*meh)(SOCKMGR, int, char *, int))
{
	skmdata_t *skm = s;
	skm->on_mgr = meh ? meh : sockmgr_mgr_event_noop;
}


/* protocol parser/unparser */
NBR_API int
nbr_sock_send_bin16(SOCK s, char *p, int len)
{
	sockdata_t *skd = s.p;
	int dlen;
	if (!nbr_sock_valid(s)) { return NBR_EINVAL; }
	dlen = len + sizeof(U16);
	if (!MT_MODE() || nbr_thread_is_current(skd->thrd)) {
		if (sock_wb_remain(skd->skm, skd) >= dlen) {
			ccb_bin16(sock_wb_top(skd), p, dlen);
			skd->nwb += dlen;
			return len;
		}
		return NBR_ESHORT;
	}
	return sock_send_data(skd, s.s, p, dlen, ccb_bin16);
}

NBR_API int
nbr_sock_send_bin32(SOCK s, char *p, int len)
{
	sockdata_t *skd = s.p;
	int dlen;
	if (!nbr_sock_valid(s)) { return NBR_EINVAL; }
	dlen = (len + sizeof(U32));
	if (!MT_MODE() || nbr_thread_is_current(skd->thrd)) {
		if (sock_wb_remain(skd->skm, skd) >= dlen) {
			ccb_bin32(sock_wb_top(skd), p, dlen);
			skd->nwb += dlen;
			return len;
		}
		return NBR_ESHORT;
	}
	return sock_send_data(skd, s.s, p, dlen, ccb_bin32);
}

NBR_API int
nbr_sock_send_text(SOCK s, char *p, int len)
{
	sockdata_t *skd = s.p;
	int dlen;
	if (!nbr_sock_valid(s)) { return NBR_EINVAL; }
	dlen = len + sizeof(U8);
	if (!MT_MODE() || nbr_thread_is_current(skd->thrd)) {
		if (sock_wb_remain(skd->skm, skd) >= dlen) {
			ccb_text(sock_wb_top(skd), p, dlen);
			skd->nwb += dlen;
			return len;
		}
		return NBR_ESHORT;
	}
	return sock_send_data(skd, s.s, p, dlen, ccb_text);
}

NBR_API int
nbr_sock_send_raw(SOCK s, char *p, int len)
{
	sockdata_t *skd = s.p;
	if (!nbr_sock_valid(s)) { return NBR_EINVAL; }
	if (!MT_MODE() || nbr_thread_is_current(skd->thrd)) {
		if (sock_wb_remain(skd->skm, skd) >= len) {
			nbr_mem_copy(sock_wb_top(skd), p, len);
			skd->nwb += len;
			return len;
		}
		return NBR_ESHORT;
	}
	return sock_send_data(skd, s.s, p, len, NULL);
}

NBR_API char*
nbr_sock_rparser_bin16(char *p, int *len, int *rlen)
{
	int tmp = GET_16(p);
	ASSERT(tmp > 0);
	if (tmp <= *len) {
		*len = tmp;
		*rlen = tmp + sizeof(U16);
		return p + sizeof(U16);
	}
	else { ASSERT(FALSE); }
	return NULL;
}

NBR_API char*
nbr_sock_rparser_bin32(char *p, int *len, int *rlen)
{
	int tmp = GET_32(p);
	ASSERT(tmp > 0);
	if (tmp <= *len) {
		*len = tmp;
		*rlen = tmp + sizeof(U32);
		return p + sizeof(U32);
	}
	return NULL;
}

NBR_API char*
nbr_sock_rparser_text(char *p, int *len, int *rlen)
{
	char *w = p;
	int tmp = *len;
	while (1) {
		if (GET_8(w) == '\n') {
			SET_8(w, 0);
			tmp = 1;
			break;
		}
		if (GET_16(w) == htons(0x0d0a)) {
			SET_16(w, 0);
			tmp = 2;
			break;
		}
		if ((w - p) > tmp) {
			return NULL;
		}
		w++;
	}
	*len = (w - p);
	*rlen = (w - p) + tmp; /* +1 for \n */
	return p;
}

NBR_API char*
nbr_sock_rparser_raw(char *p, int *len, int *rlen)
{
	*rlen = *len;
	return p;
}


/* TCP I/O main routine */
NBR_INLINE void *
sock_rw(void *ptr, int rf, int wf, int dg)
{
	sockdata_t *skd = ptr;
	int rsz, ssz, trsz;
	int cnt = 0;
	socklen_t asz;
	char *p, addr[ADRL];
	PROTOCOL *ifp = skd->skm->proto;
	skmdata_t *skm = skd->skm;

	if (rf && skd->nrb < skm->nrb) {
		asz = sizeof(addr);
		rsz = ifp->recv(skd->fd, skd->rb + skd->nrb, skm->nrb - skd->nrb,
				MSG_DONTWAIT, addr, &asz);
//TRACE("%u: time = %llu %s(%u) %u\n", nbr_osdep_getpid(), nbr_time(), __FILE__, __LINE__, rsz);
		switch(rsz) {
		case -1:
//			TRACE("%u: sock_io(%p) read: errno=%d\n", getpid(), skd, errno);
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
//				TRACE("sock_io(%p) read again\n", skd);
				sock_set_readable(skd, FALSE);
			}
			else if (errno == EPIPE) { break; }
			else { /* TRACE("%u: recv errno=%d\n", getpid(),errno); */
				sock_set_close(skd, CLOSED_BY_ERROR);
			}
			break;
		case 0:
			/* recv EOF or error occured: maybe remote peer socket closed */
			sock_set_close(skd, CLOSED_BY_REMOTE);
			break;
		default:
//			TRACE("%u: sock_io(%p) result %d byte\n", getpid(),skd,rsz);
			ASSERT(rsz > 0);
			skd->nrb += rsz;
			skd->access = nbr_clock();
			break;
		}
	}
	/* even if no read event, it is possible to remain unhandled receive data */
	/* rsz means record length (receive multipul records, reclength < rcvd byte) */
	trsz = 0;
	while (skd->nrb > trsz) {
		rsz = skd->nrb - trsz;
		if ((p = skm->record_parser(skd->rb + trsz, &rsz, &ssz))) {
			if ((rsz = skm->on_recv(sockmgr_make_sock(skd), p, rsz)) >= 0) {
				trsz += ssz;
//				TRACE("%p: trsz -> %u(add %u(%u))\n", skd, trsz, ssz, rsz);
			}
		}
		if (rsz < 0) {
			SOCK_LOG(INFO,"%u: sock closed by app: result=%d\n", getpid(), rsz);
			sock_set_close(skd, CLOSED_BY_APPLICATION);
			break;
		}
		if (cnt++ >= skm->cf.n_record_process) { break; }
	}
	sock_rb_shrink(skd, trsz);
	/* polling will write something here */
	if (!sock_polling(skm, skd)) {
		sock_set_close(skd, CLOSED_BY_TIMEOUT);
		return skd;
	}
	if (wf && skd->nwb > 0) {
		//TRACE("%u: time = %llu %s(%u) %u\n", nbr_osdep_getpid(), nbr_time(), __FILE__, __LINE__, skd->nwb);
		ssz = ifp->send(skd->fd, skd->wb, skd->nwb, 0, skd->addr, skd->alen);
		switch(ssz) {
		case -1:
			//TRACE("%u: sock_io(%p) write: errno=%d\n", getpid(), skd, errno);
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				if (dg) { sock_set_writable(skd, FALSE); }
			}
			else if (errno == EPIPE) { break; }
			else { sock_set_close(skd, CLOSED_BY_ERROR); }
			break;
		case 0:
			/* write error: maybe remote peer socket closed. */
			sock_set_close(skd, CLOSED_BY_REMOTE);
			break;
		default:
			ASSERT(ssz > 0);
			//TRACE("%u: sock_io(%p) write %u byte/%u\n", getpid(),skd, ssz, skd->skm->nwb);
			sock_wb_shrink(skd, ssz);
			break;
		}
	}
	if (!dg) {
		if (sock_attach_epoll(g_sock.epfd, skd, TRUE,
			!sock_readable(skd), !sock_writable(skd)) < 0) { return NULL; }
	}
	return skd;
}

NBR_INLINE void
dgsk_accept(skmdata_t *skm)
{
	sockdata_t *skd;
	char addr[ADRL], rb[skm->nrb];
	socklen_t addrlen;
	int cnt = 0, rsz, r;
	PROTOCOL *ifp = skm->proto;
	while(cnt++ < skm->cf.n_record_process) {
		addrlen = sizeof(addr);
//		TRACE("dgsk_io(%p): try read %d byte\n", skm, skm->nrb);
		rsz = ifp->recv(skm->fd, rb, skm->nrb, 0, addr, &addrlen);
//		TRACE("dgsk_io(%p): read %d byte\n", skm, rsz);
		if (rsz == -1) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) { break; }
			else if (errno == EPIPE) { break; }
			continue;
		}
		else {
			if (!(skd = nbr_search_mem_get(skm->skd_s, addr, addrlen))) {
				if (!(skd = sockmgr_alloc_skd(skm, addr, addrlen))) {
					/* Todo: mem short. process packet immediately */
					continue;
				}
				skd->fd = skm->fd;
				/* even if you copy fd by using dup, you cannot call connect to
				 * connect this fd to this new node(addr, addrlen). because such a
				 * connect call affects original skm->fd also... thus if connect
				 * duped fd here, then skm->fd cannot recv the packet from new node*/
				sock_set_stat(skd, SS_DGCONNECT);
				if (!MT_MODE()) {
					if (sock_attach_epoll_on_accept(skd) < 0) {
						SOCK_ERROUT(ERROR,EPOLL,"attach_epoll: %d", LASTERR);
						sockmgr_free_skd(skm, skd);
						continue;
					}
				}
				if ((r = skm->on_accept(sockmgr_make_sock(skd))) < 0) {
					SOCK_LOG(INFO,"accept blocked by app: result=%d\n", r);
					sockmgr_free_skd(skm, skd);
					continue;
				}
				if (sock_addjob(skd) < 0) {
					SOCK_ERROUT(ERROR,PTHREAD,"addjob: %d", LASTERR);
					sockmgr_free_skd(skm, skd);
					continue;
				}
			}
			if (sock_recv_dgram(skd, skd->serial, rb, rsz) < 0) {
				ASSERT(FALSE);
				sock_mark_close(skd, skd->serial, CLOSED_BY_ERROR);
				continue;
			}
			continue;
		}
	}
	return;
}

NBR_INLINE void
sock_accept(skmdata_t *skm)
{
	char addr[ADRL];
	int addrlen = ADRL;
	DSCRPTR fd;
	sockdata_t *skd;
	SKCONF skc = {skm->timeout, skm->nrb, skm->nwb };
	if ((fd = skm->proto->accept(skm->fd, addr, &addrlen, &skc)) == INVALID_FD) {
		SOCK_ERROUT(ERROR,ACCEPT,"%s: accept: errno=%d",
			skm->proto->name, errno);
		return;
	}
	if (!(skd = sockmgr_alloc_skd(skm, addr, addrlen))) {
		SOCK_ERROUT(ERROR,EXPIRE,"alloc_skd: %d",
			nbr_array_use(skm->skd_a));
		skm->proto->close(fd);
		return;
	}
	ASSERT(skd->thrd == NULL);
	skd->fd = fd;
	sock_set_stat(skd, SS_CONNECTING);
	if (!MT_MODE()) {
		if (sock_attach_epoll_on_accept(skd) < 0) {
			SOCK_ERROUT(ERROR,EPOLL,"attach_epoll: %d", LASTERR);
			skm->proto->close(fd);
			sockmgr_free_skd(skm, skd);
		}
	}
	if (skm->on_connect) {
		if (skm->on_connect(sockmgr_make_sock(skd), NULL) < 0) {
			SOCK_LOG(INFO,"accept blocked by app\n");
			skm->proto->close(fd);
			sockmgr_free_skd(skm, skd);
			return;
		}
	}
	/* this pass skd to thread, so need to put after on_accept
	 * otherwise nbr_sock_get_addr returns NULL inside of it */
	if (sock_addjob(skd) < 0) {
		SOCK_ERROUT(ERROR,PTHREAD,"addjob: %d", LASTERR);
		skm->proto->close(fd);
		sockmgr_free_skd(skm, skd);
		return;
	}
}

void *
nbr_sock_io(void *ptr)
{
	int r;
	sockdata_t *skd = ptr;
	switch(sock_get_stat(skd)) {
	case SS_CONNECTING:
		r = sock_handshake(skd);
		/* r < 0 means handshake end in failure */
		if (r < 0 || sock_attach_epoll(g_sock.epfd, skd, TRUE, 1, 1) < 0) {
			sock_set_close(skd, CLOSED_BY_ERROR);
			break;
		}
		/* r == 0, means need to more handshake to connect */
		if (r == 0){ break; }
		sock_set_stat(skd, SS_CONNECT);
		if ((r = skd->skm->on_accept(sockmgr_make_sock(skd))) < 0) {
			SOCK_LOG(INFO,"accept blocked by app: result=%d\n", r);
			sock_set_close(skd, CLOSED_BY_APPLICATION);
		}
		break;
	case SS_CONNECT:
		ptr = sock_rw(ptr, sock_readable(skd), sock_writable(skd), FALSE);
		break;
	case SS_DGCONNECT:/* dgsocket, recv is done by main thread (dbsock_accept) */
		ptr = sock_rw(ptr, 0, sock_writable(skd), TRUE);
		break;
	case SS_CLOSING:	/* try to send all buffer */
		ptr = sock_rw(ptr, 0, sock_writable(skd), skd->skm->proto->dgram);
		if (ptr && skd->nwb > 0) { break; }
		sock_set_stat(skd, SS_CLOSE);
		/* fall through */
	case SS_CLOSE:
finalize:
		skd->skm->on_close(sockmgr_make_sock(skd), skd->closereason);
		return NULL;
	default:
		SOCK_ERROUT(ERROR,INVAL,"invalid state: %d", sock_get_stat(skd));
		ASSERT(FALSE);
		goto finalize;
		break;
	}
	return ptr;
}

NBR_INLINE void
nbr_sock_process_main()
{
	skmdata_t *skm;
	sockdata_t *skd, *tmp;
	sockevent_t *e, *le;
	sockjob_t *j;
	sockbuf_t *skb;

	if (MT_MODE()) {
		if (!g_sock.free) { goto skmevent; }
		if (NBR_OK != nbr_mutex_lock(g_sock.lock)) { goto skmevent; }
		skd = g_sock.free;
		g_sock.free = NULL;
		if (NBR_OK != nbr_mutex_unlock(g_sock.lock)) { ASSERT(FALSE); }
		while((tmp = skd)) {
			skd = skd->next;
			sockmgr_cleanup_skd(tmp);
		}
skmevent:
		if (g_sock.main.web->blen <= 0) {}
		else if (NBR_OK == nbr_mutex_lock(g_sock.evlk)) {
			j = &(g_sock.main);
			/* flip double buffer */
			skb = j->reb;
			j->reb = j->web;
			j->web = skb;
			if (NBR_OK != nbr_mutex_unlock(g_sock.evlk)) { return; }
			/* process event */
			e = (sockevent_t *)j->reb->buf;
			le = (sockevent_t *)(j->reb->buf + j->reb->blen);
			while (e < le) {
				ASSERT(e->len > sizeof(*e));
				ASSERT(e->skm->on_mgr != sockmgr_mgr_event_noop);
				e->skm->on_mgr(e->skm, e->type, e->data, e->len - sizeof(*e));
				e = (sockevent_t *)(((char *)e) + e->len);
			}
			j->reb->blen = 0;
		}
	}
	else {
		e = (sockevent_t *)g_sock.eb.buf;
		le = (sockevent_t *)(g_sock.eb.buf + g_sock.eb.blen);
		while (e < le) {
			ASSERT(e->len > sizeof(*e));
			if (e->sk.p) {
				skd = e->sk.p;
				if (skd->serial == e->sk.s) {
					skd->skm->on_event(e->sk, (char *)e->data, e->len - sizeof(*e));
				}
			}
			e = (sockevent_t *)(((char *)e) + e->len);
		}
		g_sock.eb.blen = 0;
		ARRAY_SCAN(g_sock.skm_a, skm) {
			skd = nbr_array_get_first(skm->skd_a);
			while ((tmp = skd)) {
				skd = nbr_array_get_next(skm->skd_a, skd);
				if (!nbr_sock_io(tmp)) {
					sockmgr_cleanup_skd(tmp);
				}
			}
		}
	}
}

void
nbr_sock_poll()
{
	int n_events, i, tot = g_sock.total_fds;
	EVENT events[tot], *ev;
	sockdata_t *skd;
	sockjob_t *j;

	n_events = nbr_epoll_wait(g_sock.epfd, events, tot, g_sock.ioc.epoll_timeout_ms);
	if (n_events < 0) {
		switch(errno) {
		case EINTR: return;
		default:
			SOCK_ERROUT(EMERG,EPOLL,"epoll_wait: %d", errno);
			return;
		}
	}
	for (i = 0; i < n_events; i++) {
		ev = events + i;
		switch(type_from(ev)) {
		case EPD_SOCKMGR:
			sock_accept(sockmgr_from(ev));
			break;
		case EPD_DGSKMGR:
			dgsk_accept(sockmgr_from(ev));
			break;
		case EPD_SOCK:
			skd = sock_from(ev);
			skd->events |= events_from(ev);
			if (MT_MODE() && sock_readable(skd)) {
				j = nbr_thread_get_data(skd->thrd);
				sockjob_wakeup_if_sleep(j);
			}
			break;
		default:
			ASSERT(FALSE);
			break;
		}
	}
	nbr_sock_process_main();
}
