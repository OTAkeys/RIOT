/*
 * Copyright (C) 2018 Kaspar Schleiser <kaspar@schleiser.de>
 *
 * This file is subject to the terms and conditions of the GNU Lesser General
 * Public License v2.1. See the file LICENSE in the top level directory for more
 * details.
 */

#include "ztimer/convert.h"
#include "frac.h"
#include "assert.h"

static void ztimer_convert_op_cancel(ztimer_dev_t *z)
{
    ztimer_convert_t *self = (ztimer_convert_t *)z;
    ztimer_remove(self->lower, &self->lower_entry);
}

static void ztimer_convert_op_set(ztimer_dev_t *z, uint32_t val)
{
    ztimer_convert_t *self = (ztimer_convert_t *)z;
    uint32_t target_lower = frac_scale(&self->scale_set, val);
    ztimer_set(self->lower, &self->lower_entry, target_lower);
}

static uint32_t ztimer_convert_op_now(ztimer_dev_t *z)
{
    ztimer_convert_t *self = (ztimer_convert_t *)z;
    uint32_t now_lower = ztimer_now(self->lower);
    uint32_t now_self = frac_scale(&self->scale_now, now_lower);
    return now_self;
}

static const ztimer_ops_t ztimer_convert_ops = {
    .set    = ztimer_convert_op_set,
    .now    = ztimer_convert_op_now,
    .cancel = ztimer_convert_op_cancel,
};

void ztimer_convert_init(ztimer_convert_t *self, ztimer_dev_t *lower, uint32_t freq_self, uint32_t freq_lower)
{
    *self = (ztimer_convert_t) {
        .super = { .ops = &ztimer_convert_ops, },
        .lower = lower,
        .lower_entry = { .callback = (void (*)(void *))ztimer_handler, .arg = &self->super, },
    };
    assert(freq_self);
    assert(freq_lower);
    frac_init(&self->scale_now, freq_self, freq_lower);
    frac_init(&self->scale_set, freq_lower, freq_self);
}
