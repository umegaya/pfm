/****************************************************************
 * thread.c
 * 2008/07/14 iyatomi : create
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
#include "osdep.h"
#include "thread.h"
#include "rand.h"	/* for tls related initialization */



/*-------------------------------------------------------------*/
/* macro													   */
/*-------------------------------------------------------------*/
#define THREAD_LOG(prio,...)	fprintf(stderr, __VA_ARGS__)
#define INVALID_PTHREAD			(0)

#define MUTEX_SYNC(expr,error,label)							\
	if (0 != pthread_mutex_lock(&g_lock.lock)) {				\
		THREAD_ERROUT(ERROR,PTHREAD,"mutex lock");				\
		goto label;												\
	}															\
	pthread_cleanup_push((void (*)(void *))pthread_mutex_unlock, &g_lock.lock);	\
	if (!(expr)) {												\
		THREAD_ERROUT(ERROR,error,#expr": fail %d", LASTERR);	\
		pthread_mutex_unlock(&g_lock.lock);						\
		goto label;												\
	}															\
	pthread_cleanup_pop(1);

#define RWLOCK_SYNC 	MUTEX_SYNC

#define COND_WAIT(cond,timeout,ret)								\
	pthread_cleanup_push((void (*)(void *))nbr_mutex_unlock, (cond).mtx); \
	if (NBR_OK != nbr_mutex_lock((cond).mtx)) {					\
		THREAD_ERROUT(ERROR,PTHREAD,"mutex lock: %d", LASTERR);	\
		ret = EBUSY;											\
	}															\
	else { ret = cond_timedwait(&(cond),(cond).mtx,timeout); }

#define COND_UNLOCK(cond, success)								\
	pthread_cleanup_pop(success);


/*-------------------------------------------------------------*/
/* internal types											   */
/*-------------------------------------------------------------*/
typedef struct cond
{
	pthread_cond_t 	cnd;
	MUTEX			mtx;
}	cond_t;

typedef struct job
{
	struct job	*next;
	void		*p;
	void		*(*fn)	(void *);
}	job_t;

typedef struct jobqueue
{
	job_t		*top, *last;
}	jobqueue_t;

typedef struct threadpool
{
	ARRAY		th_a, jq_a;
	MUTEX		lock;
	cond_t		event;
	jobqueue_t	queue;
}	thpool_t;

typedef struct thread
{
	pthread_t	id;
	thpool_t	*belong;
	cond_t		event;
	void		*p;
	void		*(*fn)	(void *);
}	thread_t;

typedef struct mutex
{
	pthread_mutex_t	mtx;
}	mutex_t;

typedef struct rwlock
{
	pthread_rwlock_t rwl;
}	rwlock_t;


/*-------------------------------------------------------------*/
/* internal values											   */
/*-------------------------------------------------------------*/
static struct {
	ARRAY	pool_a;
	MUTEX	lock;
}			g_thread = { NULL, NULL };
static struct {
	ARRAY			mtx_a;
	ARRAY			rwl_a;
	pthread_mutex_t	lock;
}			g_lock = { NULL, NULL, PTHREAD_MUTEX_INITIALIZER };



/*-------------------------------------------------------------*/
/* internal methods											   */
/*-------------------------------------------------------------*/
NBR_INLINE jobqueue_t *
jobqueue_add_entry(ARRAY job_a, jobqueue_t *q, void *p,	void* (*fn)	(void *))
{
	job_t *node;
	if (!(node = (job_t*)nbr_array_alloc(job_a))) {
		ASSERT(FALSE);
		return NULL;
	}
	node->p = p;
	node->fn = fn;
	node->next = NULL;
	if (q->last){ q->last->next = node; }
	if (q->top)	{ q->last = node; }
	else 		{ q->top = q->last = node; }
	return q;
}

NBR_INLINE jobqueue_t *
jobqueue_remove_entry(ARRAY job_a, jobqueue_t *q, job_t *rmv)
{
	ASSERT(rmv && q);
	job_t *prev, *now;
	if (!q->top){
		ASSERT(FALSE);
		return q;
	}
	else if (q->top == rmv) {
		//TRACE("remove top entry:%p %p %p %p\n",
		//q->top, q->last, q->top->next);
		/* this means queue only has 1 element */
		if (q->last == rmv) { q->last = NULL; }
		q->top = q->top->next;
		nbr_array_free(job_a, rmv);
		//TRACE("remove top entry2:%p %p %p\n", q->top, q->last,
		//q->top ? q->top->next : q->top);
		return q;
	}
	else {
		//TRACE("remove entry: %p %p\n", q->top, rmv);
		prev = q->top;
		now = q->top->next;
		while(now) {
			if (now == rmv) {
				//TRACE("remove element found: %p %p\n", now->next, prev);
				prev->next = now->next;
				if (q->last == now) {
					q->last = prev;
				}
				break;
			}
			prev = now;
			now = now->next;
		}
	}
	ASSERT(now == rmv);
	if (now == rmv) { nbr_array_free(job_a, rmv); }
	return q;
}

NBR_INLINE int
thread_terminate(thread_t *th)
{
	void *r;
	int err = 0;
	THREAD_LOG(INFO, "thread_terminate (%08x)\n", (U32)th->id);
	if (th->id == INVALID_PTHREAD) {
		THREAD_ERROUT(ERROR,PTHREAD,"thread not running");
		return 0;
	}
	if (pthread_cancel(th->id) != 0) {
		THREAD_ERROUT(ERROR,PTHREAD,"err cancel: %d", th->id);
		err = -1;
	}
	if (pthread_join(th->id, &r) != 0) {
		THREAD_ERROUT(ERROR,PTHREAD,"err join: %d", th->id);
		err = -2;
	}
	if(r && r != PTHREAD_CANCELED) {
		THREAD_ERROUT(ERROR,PTHREAD,"error: %d %p", th->id, r);
		err = -3;
	}
	return err;
}

NBR_INLINE int
mutex_destroy(mutex_t *mx)
{
	int err = 0;
	if (0 != pthread_mutex_destroy(&(mx->mtx))) {
		THREAD_ERROUT(ERROR,PTHREAD,"mtx destroy: %p", mx);
		err = -1;
	}
	return err;
}

NBR_INLINE int
rwlock_destroy(rwlock_t *rw)
{
	int err = 0;
	if (0 != pthread_rwlock_destroy(&(rw->rwl))) {
		THREAD_ERROUT(ERROR,PTHREAD,"rwl destroy: %p", rw);
		err = -1;
	}
	return err;
}

NBR_INLINE int
cond_create(cond_t *c)
{
	if (!(c->mtx = nbr_mutex_create())) {
		return LASTERR;
	}
	return pthread_cond_init(&(c->cnd), NULL);
}

NBR_INLINE int
cond_destroy(cond_t *c)
{
	if (c->mtx) {
		nbr_mutex_destroy(c->mtx);
	}
	return pthread_cond_destroy(&(c->cnd));
}

NBR_INLINE int
cond_timedwait(cond_t *c, MUTEX m, int timeout)
{
	ASSERT(c && m);
	/* timeout = msec */
	struct timeval tv;
	struct timespec ts;
	mutex_t *mx = m;
	if (timeout < 0) {
		return pthread_cond_wait(&(c->cnd), &(mx->mtx));
	}
	gettimeofday(&tv, NULL);
	ts.tv_sec = tv.tv_sec + (timeout / 1000);
	ts.tv_nsec = (tv.tv_usec * 1000 + ((timeout % 1000) * 1000000));
	if (ts.tv_nsec >= 1000000000) {
		ts.tv_nsec -= 1000000000;
		ts.tv_sec++;
	}
	return pthread_cond_timedwait(&(c->cnd), &(mx->mtx), &ts);
}

NBR_INLINE int
cond_signal(cond_t *c)
{
	return pthread_cond_signal(&(c->cnd));
}

static void *
thpool_job_exec(void *p)
{
	void *ptr;
	void *(*fn)(void *);
	int ret;
	thread_t *th = p;
	thpool_t *tp = th->belong;
	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
	for (;;) {
		pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
		COND_WAIT(tp->event, -1, ret);
		pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
		ptr = NULL;
		if (ret == 0 || ret == EINTR || ret == ETIMEDOUT) {
			if (tp->queue.top) {
				ptr = tp->queue.top->p;
				fn = tp->queue.top->fn;
				jobqueue_remove_entry(tp->jq_a, &(tp->queue), tp->queue.top);
				ASSERT(!tp->queue.top || tp->queue.top != tp->queue.top->next);
			}
		}
		COND_UNLOCK(tp->event.mtx, ret != EBUSY ? 1 : 0);
		if (ptr) {
			if ((ptr = fn(ptr))) {
				nbr_thpool_addjob(tp, ptr, fn);
			}
		}
	}
	return p;
}

static void	*
thread_launcher(void *p)
{
	thread_t *th = p;
	/* do TLS related initialization here */
	if (nbr_tls_init(th->p) != NBR_OK) {
		return NULL;
	}
	pthread_cleanup_push((void (*)(void *))nbr_tls_fin, th->p);
	p = th->fn(th);
	pthread_cleanup_pop(1);
	return p;
}



/*-------------------------------------------------------------*/
/* external methods											   */
/*-------------------------------------------------------------*/
/* thpool */
int
nbr_thread_init(int max)
{
	nbr_thread_fin();

	if (!(g_thread.pool_a = nbr_array_create(max, sizeof(thpool_t), 0))) {
		THREAD_ERROUT(ERROR,INTERNAL,"array_create: thread: %d", max);
		nbr_thread_fin();
		return LASTERR;
	}
	if (!(g_thread.lock = nbr_mutex_create())) {
		THREAD_ERROUT(ERROR,INTERNAL,"mutex_create");
		nbr_thread_fin();
		return LASTERR;
	}
	return NBR_OK;
}

int
nbr_thread_fin()
{
	thpool_t *thp, *pthp;
	if (g_thread.pool_a) {
		thp = nbr_array_get_first(g_thread.pool_a);
		while((pthp = thp)) {
			thp = nbr_array_get_next(g_thread.pool_a, thp);
			if (nbr_thpool_destroy(pthp) < 0) {
				ASSERT(FALSE);
			}
		}
		nbr_array_destroy(g_thread.pool_a);
		g_thread.pool_a = NULL;
	}
	if (g_thread.lock) {
		nbr_mutex_destroy(g_thread.lock);
		g_thread.lock = NULL;
	}
	return NBR_OK;
}

NBR_API THPOOL
nbr_thpool_create(int max)
{
	thpool_t *thp = NULL;
	THREAD_LOCK(g_thread.lock, bad);
	if (!(thp = nbr_array_alloc(g_thread.pool_a))) {
		THREAD_ERROUT(ERROR,INTERNAL,"array_alloc: thpool: %d",
			nbr_array_use(g_thread.pool_a));
		goto bad;
	}
	if (!(thp->th_a = nbr_array_create(max, sizeof(thread_t), 0))) {
		THREAD_ERROUT(ERROR,MALLOC,"array_create: thread: %d", max);
		goto bad;
	}
	if (!(thp->lock = nbr_mutex_create())) {
		THREAD_ERROUT(ERROR,PTHREAD,"mutex_create: %d", LASTERR);
		goto bad;
	}
	if (cond_create(&(thp->event)) != 0) {
		THREAD_ERROUT(ERROR,PTHREAD,"cond_init fail");
		goto bad;
	}
	thp->jq_a = NULL;
	THREAD_UNLOCK(g_thread.lock);
	return (THPOOL)thp;
bad:
	THREAD_UNLOCK_ONEXCEPTION(g_thread.lock);
	if (thp) { nbr_thpool_destroy(thp); }
	return NULL;
}

NBR_API int
nbr_thpool_init_jobqueue(THPOOL thpl, int max_job)
{
	int i;
	thpool_t *thp = thpl;
	thread_t *th;
	if (!(thp->jq_a = nbr_array_create(max_job, sizeof(job_t), 0))) {
		THREAD_ERROUT(ERROR,MALLOC,"array_create: jobqueue: %d", max_job);
		goto bad;
	}
	nbr_mem_zero(&(thp->queue), sizeof(thp->queue));
	for (i = 0; i < nbr_array_max(thp->th_a); i++) {
		if (!nbr_thread_create(thp, NULL, thpool_job_exec)) {
			THREAD_ERROUT(ERROR,INTERNAL,"create workder fail %d", LASTERR);
			goto bad;
		}
	}
	return NBR_OK;
bad:
	if (thp->jq_a) {
		nbr_array_destroy(thp->jq_a);
	}
	ARRAY_SCAN(thp->th_a, th) {
		if (thread_terminate(th) < 0) {
			ASSERT(FALSE);
		}
	}
	return LASTERR;
}

NBR_API int
nbr_thpool_destroy(THPOOL thp)
{
	ASSERT(thp);
	thpool_t *thpl = thp;
	thread_t *th;
	THREAD_LOCK(g_thread.lock, bad);
	if (thpl->th_a) {
		ARRAY_SCAN(thpl->th_a, th) {
			if (thread_terminate(th) < 0) {
				ASSERT(FALSE);
			}
			cond_destroy(&(th->event));
		}
		nbr_array_destroy(thpl->th_a);
		thpl->th_a = NULL;
	}
	if (thpl->jq_a) {
		nbr_array_destroy(thpl->jq_a);
		thpl->jq_a = NULL;
	}
	if (thpl->lock) {
		nbr_mutex_destroy(thpl->lock);
		thpl->lock = NULL;
	}
	cond_destroy(&(thpl->event));
	nbr_array_free(g_thread.pool_a, thp);
	THREAD_UNLOCK(g_thread.lock);
	return NBR_OK;
bad:
	THREAD_UNLOCK_ONEXCEPTION(g_thread.lock);
	return LASTERR;
}

NBR_API int
nbr_thpool_size(THPOOL thp)
{
	ASSERT(thp);
	thpool_t *thpl = thp;
	return nbr_array_max(thpl->th_a);
}

NBR_API int
nbr_thpool_addjob(THPOOL thp, void *p, void *(*fn)(void *))
{
	ASSERT(thp);
	thpool_t *thpl = thp;
	THREAD_LOCK(thpl->event.mtx, bad);
	if (!thpl->jq_a) {
		THREAD_ERROUT(ERROR,PTHREAD,"job queue not initialized");
		goto bad;
	}
	if (!jobqueue_add_entry(thpl->jq_a, &(thpl->queue), p, fn)) {
		THREAD_ERROUT(ERROR,PTHREAD,"err job create: %d", LASTERR);
		goto bad;
	}
	THREAD_UNLOCK(thpl->event.mtx);
	cond_signal(&(thpl->event));
	return NBR_OK;
bad:
	THREAD_UNLOCK_ONEXCEPTION(thpl->event.mtx);
	return LASTERR;
}



/* thread */
NBR_API THREAD
nbr_thread_create(THPOOL thp, void *p, void *(*fn)(void *))
{
	ASSERT(thp);
	int r;
	thread_t *th = NULL;
	thpool_t *thpl = thp;
	THREAD_LOCK(thpl->lock, bad);
	if ((th = nbr_array_alloc(thpl->th_a))) {
		/* not maximum thread created: create new */
		th->id = INVALID_PTHREAD;
		th->belong = thp;
		th->p = p;
		th->fn = fn;
		if (cond_create(&(th->event)) != 0) {
			THREAD_ERROUT(ERROR,PTHREAD,"cond_init fail");
			goto bad;
		}
		if (0 != (r = pthread_create(&(th->id), NULL, thread_launcher, th))) {
			THREAD_ERROUT(ERROR,PTHREAD,"err create thread: %d", r);
			goto bad;
		}
		THREAD_LOG(INFO, "new thread(%08x) create\n", (int)th->id);
	}
	else {
		THREAD_ERROUT(ERROR,PTHREAD,"err create thread obj: %d",
				nbr_array_use(thpl->th_a));
		goto bad;
	}
	THREAD_UNLOCK(thpl->lock);
	return (THREAD)th;
bad:
	THREAD_UNLOCK_ONEXCEPTION(thpl->lock);
	if (th) { nbr_thread_destroy(thpl, th); }
	return NULL;
}

NBR_API int
nbr_thread_destroy(THPOOL thp, THREAD th)
{
	thpool_t *thpl = thp;
	int err = thread_terminate(th);
	cond_destroy(&(((thread_t *)th)->event));
	THREAD_SYNC(thpl->lock, nbr_array_free(thpl->th_a, th) == NBR_OK, INTERNAL, bad);
bad:
	return err < 0 ? LASTERR : NBR_OK;
}

NBR_API int
nbr_thread_wait_signal(THREAD th, int mine, int ms)
{
	thread_t *thrd = th;
	int ret;
	cond_t *c = mine ? &(thrd->event) : &(thrd->belong->event);
	/* do not exit function by using return between
	 * pthread_cleanup_push / pop. it seems that such a code
	 * crushes thread stack. it appears as crush inside
	 * pthread_test_cancel() */
	COND_WAIT(*c, ms, ret);
//	TRACE("%u:%llu: wait singal: ret=%u\n", getpid(), nbr_time(), ret);
	COND_UNLOCK(c->mtx, ret != 0 && ret != EBUSY);
	return ret;
}

NBR_API int
nbr_thread_signal(THREAD th, int mine)
{
	thread_t *thrd = th;
	cond_t *c = mine ? &(thrd->event) : &(thrd->belong->event);
	return cond_signal(c) == 0 ? NBR_OK : NBR_EPTHREAD;
}

NBR_API void*
nbr_thread_get_data(THREAD th)
{
	thread_t *thrd = th;
	return thrd->p;
}

NBR_API MUTEX
nbr_thread_signal_mutex(THREAD th)
{
	thread_t *thrd = th;
	return thrd->event.mtx;
}

NBR_API int
nbr_thread_setcancelstate(int enable)
{
	return pthread_setcancelstate(
		enable ? PTHREAD_CANCEL_ENABLE : PTHREAD_CANCEL_DISABLE, NULL
	);
}

NBR_API void
nbr_thread_testcancel()
{
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	pthread_testcancel();
	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
}

NBR_API THRID
nbr_thread_get_id(THREAD th)
{
	thread_t *thrd = th;
	return (THRID)thrd->id;
}

NBR_API THRID
nbr_thread_get_curid()
{
	return pthread_self();
}

NBR_API int
nbr_thread_is_current(THREAD th)
{
	thread_t *thrd = th;
	return pthread_self() == thrd->id;
}



/* tls */
int
nbr_tls_init(void *p)
{
	int r;
	if ((r = nbr_rand_init()) != NBR_OK) {
		return r;
	}
	return NBR_OK;
}

void
nbr_tls_fin(void *p)
{
	nbr_rand_fin();
}



/* mutex */
NBR_API int
nbr_lock_init(int max)
{
	nbr_lock_fin();
	if (!(g_lock.mtx_a = nbr_array_create(max, sizeof(mutex_t), 0))) {
		THREAD_ERROUT(ERROR,INTERNAL,"array_create: mutex: %d", max);
		return LASTERR;
	}
	if (!(g_lock.rwl_a = nbr_array_create(max, sizeof(rwlock_t), 0))) {
		THREAD_ERROUT(ERROR,INTERNAL,"array_create: rwlock: %d", max);
		return LASTERR;
	}
	return NBR_OK;
}

NBR_API int
nbr_lock_fin()
{
	mutex_t	*mx;
	rwlock_t *rwl;
	if (g_lock.mtx_a) {
		ARRAY_SCAN(g_lock.mtx_a, mx) {
			mutex_destroy(mx);
		}
		nbr_array_destroy(g_lock.mtx_a);
		g_lock.mtx_a = NULL;
	}
	if (g_lock.rwl_a) {
		ARRAY_SCAN(g_lock.rwl_a, rwl) {
			rwlock_destroy(rwl);
		}
		nbr_array_destroy(g_lock.rwl_a);
		g_lock.rwl_a = NULL;
	}
	return NBR_OK;
}

NBR_API MUTEX
nbr_mutex_create()
{
	mutex_t *mx;
	int r;
	MUTEX_SYNC(mx = nbr_array_alloc(g_lock.mtx_a), EXPIRE, bad)
	if (0 != (r = pthread_mutex_init(&(mx->mtx), NULL))) {
		THREAD_ERROUT(ERROR,PTHREAD,"err mutex_init: %d", r);
		goto bad;
	}
	return (MUTEX)mx;
bad:
	if (mx) {
		nbr_mutex_destroy(mx);
	}
	return NULL;
}

NBR_API int
nbr_mutex_destroy(MUTEX mx)
{
	int err = mutex_destroy(mx);
	MUTEX_SYNC(NBR_OK == nbr_array_free(g_lock.mtx_a, mx), INTERNAL, bad);
bad:
	return err < 0 ? LASTERR : NBR_OK;
}

NBR_API int
nbr_mutex_lock(MUTEX mx)
{
	ASSERT(mx);
	int r;
	mutex_t *m = mx;
	if (0 != (r = pthread_mutex_lock(&(m->mtx)))) {
		THREAD_ERROUT(ERROR,PTHREAD,"mutex lock: %d", r);
		return LASTERR;
	}
	return NBR_OK;
}

NBR_API int
nbr_mutex_unlock(MUTEX mx)
{
	ASSERT(mx);
	int r;
	mutex_t *m = mx;
	if (0 != (r = pthread_mutex_unlock(&(m->mtx)))) {
		THREAD_ERROUT(ERROR,PTHREAD,"mutex unlock: %d", r);
		return LASTERR;
	}
	return NBR_OK;
}

NBR_API RWLOCK
nbr_rwlock_create()
{
	rwlock_t *rw;
	int r;
	RWLOCK_SYNC(rw = nbr_array_alloc(g_lock.rwl_a), EXPIRE, bad)
	if (0 != (r = pthread_rwlock_init(&(rw->rwl), NULL))) {
		THREAD_ERROUT(ERROR,PTHREAD,"err rwlock_init: %d", r);
		goto bad;
	}
	return (RWLOCK)rw;
bad:
	if (rw) {
		nbr_rwlock_destroy(rw);
	}
	return NULL;
}

NBR_API int
nbr_rwlock_destroy(RWLOCK rw)
{
	int err = rwlock_destroy(rw);
	RWLOCK_SYNC(NBR_OK == nbr_array_free(g_lock.rwl_a, rw), INTERNAL, bad);
bad:
	return err < 0 ? LASTERR : NBR_OK;
}

NBR_API int
nbr_rwlock_rdlock(RWLOCK rw)
{
	ASSERT(rw);
	int r;
	rwlock_t *rwl = rw;
	if (0 != (r = pthread_rwlock_rdlock(&(rwl->rwl)))) {
		THREAD_ERROUT(ERROR,PTHREAD,"rwlock rdlock: %d", r);
		return LASTERR;
	}
	return NBR_OK;
}

NBR_API int
nbr_rwlock_wrlock(RWLOCK rw)
{
	ASSERT(rw);
	int r;
	rwlock_t *rwl = rw;
	if (0 != (r = pthread_rwlock_wrlock(&(rwl->rwl)))) {
		THREAD_ERROUT(ERROR,PTHREAD,"rwlock wrlock: %d", r);
		return LASTERR;
	}
	return NBR_OK;
}

NBR_API int
nbr_rwlock_unlock(RWLOCK rw)
{
	ASSERT(rw);
	int r;
	rwlock_t *rwl = rw;
	if (0 != (r = pthread_rwlock_unlock(&(rwl->rwl)))) {
		THREAD_ERROUT(ERROR,PTHREAD,"rwlock unlock: %d", r);
		return LASTERR;
	}
	return NBR_OK;
}
