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
#include "periph_conf.h"

/* set interval to 1 second */
#define _DIV    5
#define _1SEC (RTT_FREQUENCY)
#define INTERVAL2 (5*RTT_FREQUENCY)
#define INTERVAL3 ((uint32_t)(((uint32_t)RTT_FREQUENCY)/_DIV))
#define _20MSEC (655) //@32768
#define _30MSEC (983) //@32768
#define _200MSEC (6553)
//#define _200MSEC (3277)

uint32_t cnt = 0;
ztimer_t timer1;
ztimer_t timer2;

static void callback1seg(void* arg){
    (void) arg;
    printf("cb 1 second\n");
    ztimer_set(ZTIMER_USEC, &timer1, _1SEC);
}

static void callback20msec(void* arg){
    uint32_t *cnt = (uint32_t *)arg;
    *cnt += 1;
    printf("cb msec %"PRIu32"\n", *cnt);
    *cnt %= _DIV;
    ztimer_set(ZTIMER_USEC, &timer2, _200MSEC);
}

int main(void)
{
    printf("RTT_FREQUENCY  %d\n", RTT_FREQUENCY);
    printf("INTERVAL3   %ld\n", INTERVAL3);
    timer1.callback = callback1seg;
    timer1.arg = NULL;

    timer2.callback = callback20msec;
    timer2.arg = (void*) &cnt;
    printf("Setting alarm 1\n");
    ztimer_set(ZTIMER_USEC, &timer1, _1SEC);
    printf("Setting alarm 2\n");
    ztimer_set(ZTIMER_USEC, &timer2, _200MSEC);

    while(1) {
    }

    return 0;
}

//int main(void)
//{
////    xtimer_ticks32_t last_wakeup = xtimer_now();
//
//    while(1) {
////        ztimer_sleep(ZTIMER_USEC, INTERVAL2);
////        printf("slept until 0x%08"PRIx32"\n", ztimer_now(ZTIMER_USEC));
////        ztimer_sleep(ZTIMER_USEC, INTERVAL1);
////        printf("2slept until 0x%08"PRIx32"\n", ztimer_now(ZTIMER_USEC));
//        ztimer_sleep(ZTIMER_USEC, _30MSEC);
//        printf("3slept until 0x%08"PRIx32"\n", ztimer_now(ZTIMER_USEC));
//    }
//
//    return 0;
//}
//int main(void)
//{
//    uint32_t last_wakeup = ztimer_now(ZTIMER_USEC);
//
//    while(1) {
//        ztimer_periodic_wakeup(ZTIMER_USEC, &last_wakeup, _1SEC);
//        printf("slept until %" PRIu32 "\n", ztimer_now(ZTIMER_USEC));
//    }
//
//    return 0;
//}














