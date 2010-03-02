/****************************************************************
 * thread.h : control POSIX base threading
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
#if !defined(__THREAD_H__)
#define __THREAD_H__

#include <pthread.h>

int nbr_thread_init(int max);
int nbr_thread_fin();
int nbr_tls_init(void *p);
void nbr_tls_fin(void *p);
int nbr_lock_init(int max);
int nbr_lock_fin();

#define THREAD_ERROUT			NBR_ERROUT

#define THREAD_SYNC(object, expr, error, label)							\
	if (NBR_OK != nbr_mutex_lock(object)) {								\
		THREAD_ERROUT(ERROR,PTHREAD,"mutex lock: %d", LASTERR);			\
		goto label;														\
	}																	\
	pthread_cleanup_push((void (*)(void *))nbr_mutex_unlock, object);	\
	if (!(expr)) {														\
		THREAD_ERROUT(ERROR,error,#expr": fail %d", LASTERR);			\
		nbr_mutex_unlock(object);										\
		goto label;														\
	}																	\
	pthread_cleanup_pop(1);

#define THREAD_LOCK(object, label)										\
	if (NBR_OK != nbr_mutex_lock(object)) {								\
		THREAD_ERROUT(ERROR,PTHREAD,"mutex lock: %d", LASTERR);			\
		goto label;														\
	}																	\
	pthread_cleanup_push((void (*)(void *))nbr_mutex_unlock, object);

#define RW_WRITE_LOCK(object)											\
	if (NBR_OK != nbr_rwlock_wrlock(object)) {							\
		THREAD_ERROUT(ERROR,PTHREAD,"rwlock wrlock: %d", LASTERR);		\
		goto label;														\
	}																	\
	pthread_cleanup_push((void (*)(void *))nbr_rwlock_unlock, object);

#define RW_READ_LOCK(object, label)										\
	if (NBR_OK != nbr_rwlock_rdlock(object)) {							\
		THREAD_ERROUT(ERROR,PTHREAD,"rwlock rdlock: %d", LASTERR);		\
		goto label;														\
	}																	\
	pthread_cleanup_push((void (*)(void *))nbr_rwlock_unlock, object);


#define THREAD_UNLOCK(object)				pthread_cleanup_pop(1);
#define THREAD_UNLOCK_ONEXCEPTION(object)	nbr_mutex_unlock(object);
#define RW_UNLOCK(object)					pthread_cleanup_pop(1);
#define RW_UNLOCK_ON_EXCEPTION(object)		nbr_rwlock_unlock(object)

#endif//__THREAD_H__
