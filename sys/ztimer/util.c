/*
 * Copyright (C) 2018 Kaspar Schleiser <kaspar@schleiser.de>
 *
 * This file is subject to the terms and conditions of the GNU Lesser General
 * Public License v2.1. See the file LICENSE in the top level directory for more
 * details.
 */

#include <assert.h>

#include "mutex.h"
#include "thread.h"
#include "ztimer.h"

typedef struct {
    mutex_t *mutex;
    thread_t *thread;
    int timeout;
} mutex_thread_t;

static void _callback_unlock_mutex(void* arg)
{
    mutex_t *mutex = (mutex_t *) arg;
    mutex_unlock(mutex);
}

void ztimer_sleep(ztimer_dev_t *ztimer, uint32_t duration)
{
/*    if (irq_is_in()) {
        _ztimer_spin(duration);
        return;
    }
*/
    ztimer_t timer;
    mutex_t mutex = MUTEX_INIT_LOCKED;

    timer.callback = _callback_unlock_mutex;
    timer.arg = (void*) &mutex;

    ztimer_set(ztimer, &timer, duration);
    mutex_lock(&mutex);
}

void ztimer_periodic_wakeup(ztimer_dev_t *ztimer, uint32_t *last_wakeup, uint32_t period)
{
    uint32_t now = ztimer_now(ztimer);
    uint32_t target = *last_wakeup + period;
    uint32_t offset = target - now;

    if (offset <= period) {
        ztimer_sleep(ztimer, offset);
        *last_wakeup = target;
    }
    else {
        *last_wakeup = now;
    }
}

static void _callback_msg(void* arg)
{
    msg_t *msg = (msg_t*)arg;
    msg_send_int(msg, msg->sender_pid);
}

static inline void _setup_msg(ztimer_t *timer, msg_t *msg, kernel_pid_t target_pid)
{
    timer->callback = _callback_msg;
    timer->arg = (void*) msg;

    /* use sender_pid field to get target_pid into callback function */
    msg->sender_pid = target_pid;
}

void ztimer_set_msg(ztimer_dev_t *dev, ztimer_t *timer, uint32_t offset, msg_t *msg, kernel_pid_t target_pid)
{
    _setup_msg(timer, msg, target_pid);
    ztimer_set(dev, timer, offset);
}

#define MSG_ZTIMER (12345U)

int ztimer_msg_receive_timeout(ztimer_dev_t *dev, msg_t *msg, uint32_t timeout)
{
    ztimer_t t;
    msg_t m = { .type=MSG_ZTIMER, .content.ptr=&m };

    ztimer_set_msg(dev, &t, timeout, &m, sched_active_pid);

    msg_receive(msg);
    if (m.type == MSG_ZTIMER && m.content.ptr == &m) {
        /* we hit the timeout */
        return -1;
    }
    else {
        ztimer_remove(dev, &t);
        return 1;
    }
}
