/*
 * Copyright (C) 2018 Kaspar Schleiser <kaspar@schleiser.de>
 *
 * This file is subject to the terms and conditions of the GNU Lesser General
 * Public License v2.1. See the file LICENSE in the top level directory for more
 * details.
 */

#include "periph/rtt.h"
#include "ztimer/rtt.h"

#define ENABLE_DEBUG (0)
#include "debug.h"

static void _ztimer_rtt_callback(void *arg)
{
    ztimer_handler((ztimer_dev_t*) arg);
}

static void _ztimer_rtt_set(ztimer_dev_t *ztimer, uint32_t val)
{
    rtt_set_alarm(rtt_get_counter() + val, _ztimer_rtt_callback, ztimer);
}

static uint32_t _ztimer_rtt_now(ztimer_dev_t *ztimer)
{
    (void)ztimer;
    return rtt_get_counter();
}

static void _ztimer_rtt_cancel(ztimer_dev_t *ztimer)
{
    (void)ztimer;
    rtt_clear_alarm();
}

static const ztimer_ops_t _ztimer_rtt_ops = {
    .set=_ztimer_rtt_set,
    .now=_ztimer_rtt_now,
    .cancel=_ztimer_rtt_cancel,
};

void ztimer_rtt_init(ztimer_rtt_t *ztimer)
{
    ztimer->ops = &_ztimer_rtt_ops;
    rtt_init();
    rtt_poweron();
}
