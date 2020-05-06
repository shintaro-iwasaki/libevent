/*
 * Copyright 2009-2012 Niels Provos and Nick Mathewson
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "event2/event-config.h"
#include "evconfig-private.h"

#include <abt.h>

struct event_base;
#include "event2/thread.h"

#include <stdlib.h>
#include <string.h>
#include "mm-internal.h"
#include "evthread-internal.h"

static ABT_mutex_attr attr_default;
static ABT_mutex_attr attr_recursive;

static void *
evthread_argobots_lock_alloc(unsigned locktype)
{
	ABT_mutex_attr attr = attr_default;
	ABT_mutex lock;
	if (locktype & EVTHREAD_LOCKTYPE_RECURSIVE) {
		attr = attr_recursive;
	}
	if (ABT_mutex_create_with_attr(attr, &lock) != ABT_SUCCESS) {
		return NULL;
	}
	return (void *)lock;
}

static void
evthread_argobots_lock_free(void *lock_, unsigned locktype)
{
	ABT_mutex lock = (ABT_mutex)lock_;
	ABT_mutex_free(&lock);
}

static int
evthread_argobots_lock(unsigned mode, void *lock_)
{
	ABT_mutex lock = (ABT_mutex)lock_;
	int r;
	if (mode & EVTHREAD_TRY) {
		r = ABT_mutex_trylock(lock);
		return (r == ABT_SUCCESS) ? 0 : EBUSY;
	} else {
		r = ABT_mutex_lock(lock);
		return (r == ABT_SUCCESS) ? 0 : EAGAIN;
	}
}

static int
evthread_argobots_unlock(unsigned mode, void *lock_)
{
	ABT_mutex lock = (ABT_mutex)lock_;
	return ABT_mutex_unlock(lock);
}

static unsigned long
evthread_argobots_get_id(void)
{
	ABT_thread_id id;
	ABT_thread_self_id(&id);
	return (unsigned long)id;
}

static void *
evthread_argobots_cond_alloc(unsigned condflags)
{
	ABT_cond cond;
	if (ABT_cond_create(&cond) != ABT_SUCCESS)
		return NULL;
	return (void *)cond;
}

static void
evthread_argobots_cond_free(void *cond_)
{
	ABT_cond cond = (ABT_cond)cond_;
	ABT_cond_free(&cond);
}

static int
evthread_argobots_cond_signal(void *cond_, int broadcast)
{
	ABT_cond cond = (ABT_cond)cond_;
	int r;
	if (broadcast)
		r = ABT_cond_broadcast(cond);
	else
		r = ABT_cond_signal(cond);
	return (r == ABT_SUCCESS) ? 0 : -1;
}

static int
evthread_argobots_cond_wait(void *cond_, void *lock_, const struct timeval *tv)
{
	int r;
	ABT_mutex lock = (ABT_mutex)lock_;
	ABT_cond cond = (ABT_cond)cond_;

	if (tv) {
		struct timeval now, abstime;
		struct timespec ts;
		evutil_gettimeofday(&now, NULL);
		evutil_timeradd(&now, tv, &abstime);
		ts.tv_sec = abstime.tv_sec;
		ts.tv_nsec = abstime.tv_usec * 1000;
		r = ABT_cond_timedwait(cond, lock, &ts);
		if (r == ABT_ERR_COND_TIMEDOUT)
			return 1;
		else if (r != ABT_SUCCESS)
			return -1;
		else
			return 0;
	} else {
		r = ABT_cond_wait(cond, lock);
		return (r != ABT_SUCCESS) ? -1 : 0;
	}
}

int
evthread_use_pthreads_with_flags(int flags)
{
	struct evthread_lock_callbacks cbs = {
		EVTHREAD_LOCK_API_VERSION,
		EVTHREAD_LOCKTYPE_RECURSIVE,
		evthread_argobots_lock_alloc,
		evthread_argobots_lock_free,
		evthread_argobots_lock,
		evthread_argobots_unlock
	};
	struct evthread_condition_callbacks cond_cbs = {
		EVTHREAD_CONDITION_API_VERSION,
		evthread_argobots_cond_alloc,
		evthread_argobots_cond_free,
		evthread_argobots_cond_signal,
		evthread_argobots_cond_wait
	};

	ABT_init(0, NULL);

	ABT_mutex_attr_create(&attr_default);
	ABT_mutex_attr_set_recursive(attr_default, ABT_FALSE);

	/* Set ourselves up to get recursive locks. */
	ABT_mutex_attr_create(&attr_recursive);
	ABT_mutex_attr_set_recursive(attr_recursive, ABT_TRUE);

	evthread_set_lock_callbacks(&cbs);
	evthread_set_condition_callbacks(&cond_cbs);
	evthread_set_id_callback(evthread_argobots_get_id);
	return 0;
}

int
evthread_use_pthreads(void)
{
	return evthread_use_pthreads_with_flags(0);
}
