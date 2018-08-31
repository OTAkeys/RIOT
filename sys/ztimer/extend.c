/*
 * Copyright (C) 2018 Kaspar Schleiser <kaspar@schleiser.de>
 *
 * This file is subject to the terms and conditions of the GNU Lesser General
 * Public License v2.1. See the file LICENSE in the top level directory for more
 * details.
 */

#include <inttypes.h>

#include "ztimer/extend.h"

#define ENABLE_DEBUG (0)
#include "debug.h"

static void _ztimer_extend_cancel(ztimer_dev_t *ztimer);
static void _ztimer_extend_overflow_callback(void* arg);
static void _ztimer_extend_update(ztimer_extend_t *ztimer_extend);

static void _ztimer_extend_callback(void* arg)
{
    ztimer_extend_t *ztimer_extend = (ztimer_extend_t*) arg;
    DEBUG("_ztimer_extend_callback()\n");
    ztimer_handler(&ztimer_extend->super);
}

static void _ztimer_extend_update(ztimer_extend_t *ztimer_extend)
{
    unsigned shift = ztimer_extend->shift;
    uint32_t target = ztimer_extend->super.list.offset + ztimer_extend->super.list.next->offset;
    uint32_t masked_tgt = target & (0xffffffff << shift);
    uint32_t now = ztimer_now((ztimer_dev_t*)ztimer_extend);
    uint32_t masked_now = now & (0xffffffff << shift);

    if (masked_now == masked_tgt) {
        if (now < target) {
            target -= now;
        }
        else {
            target = 0;
        }
        target &= (0xffffffff >> shift);
        DEBUG("_ztimer_extend_update() now=%"PRIu32" masked=%"PRIu32" offset=%"PRIu32" tgt=%"PRIu32"\n",
                ztimer_now((ztimer_dev_t*)ztimer_extend),
                masked_now,
                ztimer_extend->super.list.next->offset, target);

        ztimer_set(ztimer_extend->parent, &ztimer_extend->parent_entry, target);
    }
}

static void _ztimer_extend_overflow_callback(void* arg)
{
    ztimer_extend_t *ztimer_extend = (ztimer_extend_t*) arg;

    ztimer_extend->overflows++;

    /* calculate and set interval to end of next half-period */
    uint32_t target = (0xffffffff - ztimer_now(ztimer_extend->parent)) & (0xffffffff >> (32 - ztimer_extend->shift + 1));
    ztimer_set(ztimer_extend->parent, &ztimer_extend->parent_overflow_entry, target);

    if (ztimer_extend->super.list.next) {
        _ztimer_extend_update(ztimer_extend);
    }
}

static void _ztimer_extend_set(ztimer_dev_t *ztimer, uint32_t val)
{
    (void)val;

    ztimer_extend_t *ztimer_extend = (ztimer_extend_t*) ztimer;

    DEBUG("_ztimer_extend_set() val=%"PRIu32" overflows=%"PRIu32"\n", val,
            ztimer_extend->overflows);

    _ztimer_extend_update(ztimer_extend);
}

static void _ztimer_extend_cancel(ztimer_dev_t *ztimer)
{
    ztimer_extend_t *ztimer_extend = (ztimer_extend_t*) ztimer;
    ztimer_remove(ztimer_extend->parent, &ztimer_extend->parent_entry);
}

static uint32_t _ztimer_extend_now(ztimer_dev_t *ztimer)
{
    ztimer_extend_t *ztimer_extend = (ztimer_extend_t*) ztimer;

    uint32_t low;
    uint32_t overflows;
    unsigned shift = ztimer_extend->shift;

    do {
        overflows = ztimer_extend->overflows;
        low = ztimer_now(ztimer_extend->parent) & (0xffffffff >> (32 - shift));
    } while (overflows != ztimer_extend->overflows);

    uint32_t res = (overflows << (shift-1)) | low;

    if ((overflows & 1) && !(low >> (shift-1))) {
        res += 1 << (shift - 1);
    }

    return res;
}

static const ztimer_ops_t _ztimer_extend_ops = {
    .set=_ztimer_extend_set,
    .now=_ztimer_extend_now,
    .cancel=_ztimer_extend_cancel,
};

void ztimer_extend_init(ztimer_extend_t *ztimer_extend, ztimer_dev_t *parent, unsigned shift)
{
    uint32_t now = ztimer_now(parent);
    ztimer_extend_t tmp = {
        .shift = shift,
        .parent = parent,
        .super.ops=&_ztimer_extend_ops,
        .parent_entry.callback=_ztimer_extend_callback,
        .parent_entry.arg=ztimer_extend,
        .parent_overflow_entry.callback=_ztimer_extend_overflow_callback,
        .parent_overflow_entry.arg=ztimer_extend,
        .overflows=((now >> (shift - 1)) & 1)
    };

    *ztimer_extend = tmp;
    ztimer_set(parent, &ztimer_extend->parent_overflow_entry, 1 << (shift - 1));
}
