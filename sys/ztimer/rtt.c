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

static void _ztimer_rtt_overflow_callback(void *arg)
{
    _ztimer_overflow_callback((ztimer_dev_t*) arg);
}

static void _ztimer_rtt_set_overflow_alarm(ztimer_dev_t *ztimer)
{
    rtt_set_overflow_cb(_ztimer_rtt_overflow_callback, ztimer);
}


static void _ztimer_rtt_set(ztimer_dev_t *ztimer, uint32_t val)
{   uint16_t now = ztimer->ops->now(ztimer);
    DEBUG("_ztimer_rtt_set 16bit: now=0x%04"PRIx16",target=0x%08"PRIx32"\n", now, val);
    rtt_set_alarm(val + now, _ztimer_rtt_callback, ztimer);
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

static void _ztimer_rtt_cancel_ovf(ztimer_dev_t *ztimer)
{
    (void)ztimer;
    rtt_clear_overflow_cb();
}


static const ztimer_ops_t _ztimer_rtt_ops = {
    .set =_ztimer_rtt_set,
    .set_ovf_alarm = _ztimer_rtt_set_overflow_alarm,
    .now =_ztimer_rtt_now,
    .cancel =_ztimer_rtt_cancel,
    .cancel_ovf = _ztimer_rtt_cancel_ovf
};

void ztimer_rtt_init(ztimer_rtt_t *ztimer)
{
    ztimer->ops = &_ztimer_rtt_ops;
    //set overflow alarm
    ztimer->ops->set_ovf_alarm(ztimer);
    rtt_init();
    rtt_poweron();

}
