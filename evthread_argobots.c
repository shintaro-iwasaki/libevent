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

static int argobots_log_level = 0;

#define abt_dprintf(debug_level, ...)            \
	do {                                         \
		if (argobots_log_level >= (debug_level)) \
			printf(__VA_ARGS__); fflush(0);      \
	} while (0)

static void *
evthread_argobots_lock_alloc(unsigned locktype)
{
	ABT_mutex_attr attr = attr_default;
	ABT_mutex lock;
	if (locktype & EVTHREAD_LOCKTYPE_RECURSIVE) {
		attr = attr_recursive;
	}
	if (ABT_mutex_create_with_attr(attr, &lock) != ABT_SUCCESS) {
		abt_dprintf(1, "[libevent] evthread_argobots_lock_alloc(): fail.\n");
		return NULL;
	}
	abt_dprintf(2, "[libevent] evthread_argobots_lock_alloc(): %p\n", (void *)lock);
	return (void *)lock;
}

static void
evthread_argobots_lock_free(void *lock_, unsigned locktype)
{
	ABT_mutex lock = (ABT_mutex)lock_;
	ABT_mutex_free(&lock);
	abt_dprintf(2, "[libevent] evthread_argobots_lock_free(): %p\n", (void *)lock);
}

static int
evthread_argobots_lock(unsigned mode, void *lock_)
{
	int r;
	ABT_mutex lock = (ABT_mutex)lock_;
	if (mode & EVTHREAD_TRY) {
		abt_dprintf(1, "[libevent] evthread_argobots_lock(): enter (trylock) %p\n", (void *)lock);
		r = ABT_mutex_trylock(lock);
		abt_dprintf(1, "[libevent] evthread_argobots_lock(): exit (trylock) %p "
			           "r = %s\n", (void *)lock, (r == ABT_SUCCESS) ? "success" : "fail");
		return (r == ABT_SUCCESS) ? 0 : EBUSY;
	} else {
		abt_dprintf(1, "[libevent] evthread_argobots_lock(): enter (lock) %p\n", (void *)lock);
		r = ABT_mutex_lock(lock);
		abt_dprintf(1, "[libevent] evthread_argobots_lock(): exit (lock) %p\n", (void *)lock);
		return (r == ABT_SUCCESS) ? 0 : EAGAIN;
	}
}

static int
evthread_argobots_unlock(unsigned mode, void *lock_)
{
	ABT_mutex lock = (ABT_mutex)lock_;
	abt_dprintf(1, "[libevent] evthread_argobots_unlock(): %p\n", (void *)lock);
	return ABT_mutex_unlock(lock);
}

static unsigned long
evthread_argobots_get_id(void)
{
	ABT_thread_id id;
	ABT_thread_self_id(&id);
	abt_dprintf(2, "[libevent] evthread_argobots_get_id(): %lld\n", (long long)id);
	return (unsigned long)id;
}

static void *
evthread_argobots_cond_alloc(unsigned condflags)
{
	ABT_cond cond;
	if (ABT_cond_create(&cond) != ABT_SUCCESS) {
		abt_dprintf(1, "[libevent] evthread_argobots_cond_alloc(): fail.\n");
		return NULL;
	}
	abt_dprintf(2, "[libevent] evthread_argobots_cond_alloc(): %p\n", (void *)cond);
	return (void *)cond;
}

static void
evthread_argobots_cond_free(void *cond_)
{
	ABT_cond cond = (ABT_cond)cond_;
	abt_dprintf(2, "[libevent] evthread_argobots_cond_free(): %p\n", (void *)cond);
	ABT_cond_free(&cond);
}

static int
evthread_argobots_cond_signal(void *cond_, int broadcast)
{
	ABT_cond cond = (ABT_cond)cond_;
	int r;
	if (broadcast) {
		abt_dprintf(1, "[libevent] evthread_argobots_cond_signal(): (broadcast) %p\n", (void *)cond);
		r = ABT_cond_broadcast(cond);
	} else {
		abt_dprintf(1, "[libevent] evthread_argobots_cond_signal(): (signal) %p\n", (void *)cond);
		r = ABT_cond_signal(cond);
	}
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
		abt_dprintf(1, "[libevent] evthread_argobots_cond_wait(): (timedwait) enter c: %p m: %p\n", (void *)cond, (void *)lock);
		r = ABT_cond_timedwait(cond, lock, &ts);
		if (r == ABT_ERR_COND_TIMEDOUT) {
			abt_dprintf(1, "[libevent] evthread_argobots_cond_wait(): (timedwait) timeout c: %p m: %p\n", (void *)cond, (void *)lock);
			return 1;
		} else if (r != ABT_SUCCESS) {
			abt_dprintf(1, "[libevent] evthread_argobots_cond_wait(): (timedwait) fail c: %p m: %p\n", (void *)cond, (void *)lock);
			return -1;
		} else {
			abt_dprintf(1, "[libevent] evthread_argobots_cond_wait(): (timedwait) exit c: %p m: %p\n", (void *)cond, (void *)lock);
			return 0;
		}
	} else {
		abt_dprintf(1, "[libevent] evthread_argobots_cond_wait(): (wait) enter c: %p m: %p\n", (void *)cond, (void *)lock);
		r = ABT_cond_wait(cond, lock);
		abt_dprintf(1, "[libevent] evthread_argobots_cond_wait(): (wait) exit c: %p m: %p\n", (void *)cond, (void *)lock);
		return (r != ABT_SUCCESS) ? -1 : 0;
	}
}

int
evthread_use_pthreads_with_flags(int flags)
{
	char *argobots_log_level_env = getenv("EVENT_ABT_LOG_LEVEL");
	if (argobots_log_level_env) {
		argobots_log_level = atoi(argobots_log_level_env);
	}
	abt_dprintf(1, "[libevent] evthread_use_pthreads_with_flags()\n");
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

	/* Set ourselves up to get recursive locks. */
	ABT_mutex_attr_create(&attr_default);
	ABT_mutex_attr_set_recursive(attr_default, ABT_FALSE);
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
