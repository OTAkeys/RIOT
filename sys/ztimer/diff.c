/*
 * Copyright (C) 2018 Kaspar Schleiser <kaspar@schleiser.de>
 *
 * This file is subject to the terms and conditions of the GNU Lesser General
 * Public License v2.1. See the file LICENSE in the top level directory for more
 * details.
 */

#include "ztimer.h"

typedef struct {
    ztimer_dev_t *ztimer;
    volatile uint32_t *val;
} callback_arg_t;

static void _callback(void* arg)
{
    callback_arg_t *callback_arg = (callback_arg_t*) arg;
    *callback_arg->val = ztimer_now(callback_arg->ztimer);
}

uint32_t ztimer_diff(ztimer_dev_t *ztimer, uint32_t base)
{
    volatile uint32_t after = 0;
    uint32_t pre;
    callback_arg_t arg = { .ztimer=ztimer, .val=&after };
    ztimer_t t = {.callback = _callback, .arg=&arg };
    pre = ztimer_now(ztimer);
    ztimer_set(ztimer, &t, base);
    while(!after) {}
    return after-pre-base;
}
