/*
 * Copyright (C) 2015 Kaspar Schleiser <kaspar@schleiser.de>
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     examples
 * @{
 *
 * @file
 * @brief       example application for setting a periodic wakeup
 *
 * @author      Kaspar Schleiser <kaspar@schleiser.de>
 *
 * @}
 */

#include <stdio.h>

#include "ztimer.h"
#include "thread.h"
#include "msg.h"
#include "timex.h"

char timer_stack[THREAD_STACKSIZE_DEFAULT];
char timer_stack_local[THREAD_STACKSIZE_DEFAULT];

struct timer_msg {
    ztimer_t timer;
    uint32_t interval;
    char *text;
    msg_t msg;
};

struct timer_msg msg_a = { .interval = (2 * US_PER_SEC),
                           .text = "Hello World" };
struct timer_msg msg_b = { .interval = (5 * US_PER_SEC),
                           .text = "This is a Test" };

void *timer_thread(void *arg)
{
    (void) arg;

    printf("This is thread %" PRIkernel_pid "\n", thread_getpid());

    /* The queue is required to avoid loss of a 2nd message, when the 1st is
     * still processed. The timing ensures that at most 1 message is queued.
     */
    msg_t msgq[1];
    msg_init_queue(msgq, 1);

    while (1) {
        msg_t m;
        msg_receive(&m);
        struct timer_msg *tmsg = m.content.ptr;
        uint32_t now = ztimer_now(ZTIMER_USEC);
        printf("now=%" PRIu32 ":%" PRIu32 " -> every %" PRIu32 ".%" PRIu32 "s: %s\n",
               (now / US_PER_SEC),
               (now % US_PER_SEC),
               tmsg->interval / US_PER_SEC,
               tmsg->interval % US_PER_SEC,
               tmsg->text);

        tmsg->msg.type = 12345;
        tmsg->msg.content.ptr = tmsg;
        ztimer_set_msg(ZTIMER_USEC, &tmsg->timer, tmsg->interval, &tmsg->msg, thread_getpid());
    }
}

void *timer_thread_local(void *arg)
{
    (void) arg;

    printf("This is thread %" PRIkernel_pid "\n", thread_getpid());

    while (1) {
        msg_t m;
        msg_receive(&m);

        uint32_t now = ztimer_now(ZTIMER_USEC);
        int sec = now / US_PER_SEC;
        int min = sec / 60;
        int hr  = sec / 3600;
        printf("sec=%d min=%d hour=%d\n", sec, min, hr);
    }
}

int main(void)
{
    msg_t m;
    kernel_pid_t pid = thread_create(
                  timer_stack,
                  sizeof(timer_stack),
                  THREAD_PRIORITY_MAIN - 1,
                  THREAD_CREATE_STACKTEST,
                  timer_thread,
                  NULL,
                  "timer");

    puts("sending 1st msg");
    m.content.ptr = &msg_a;
    msg_try_send(&m, pid);

    puts("sending 2nd msg");
    m.content.ptr = &msg_b;
    msg_try_send(&m, pid);

    kernel_pid_t pid2 = thread_create(
                   timer_stack_local,
                   sizeof(timer_stack_local),
                   THREAD_PRIORITY_MAIN - 1,
                   THREAD_CREATE_STACKTEST,
                   timer_thread_local,
                   NULL,
                   "timer local");

    while (1) {
        ztimer_sleep(ZTIMER_USEC, 1 * US_PER_SEC);
        msg_try_send(&m, pid2);
    }
}
