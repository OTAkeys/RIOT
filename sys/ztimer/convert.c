/*
 * Copyright (C) 2018 Kaspar Schleiser <kaspar@schleiser.de>
 *
 * This file is subject to the terms and conditions of the GNU Lesser General
 * Public License v2.1. See the file LICENSE in the top level directory for more
 * details.
 */

#include <stdint.h>

#include "frac.h"
#include "assert.h"
#include "irq.h"
#include "ztimer/convert.h"

#define ENABLE_DEBUG (0)
#include "debug.h"

/**
 * @brief   Compute the scaling parameters for the given two frequencies
 *
 * @param[in]   self        pointer to instance to operate on
 * @param[in]   freq_self   desired frequency of this clock
 * @param[in]   freq_lower  frequency of the underlying clock
 */
static void ztimer_convert_compute_scale(ztimer_convert_t *self, uint32_t freq_self, uint32_t freq_lower);

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
    uint32_t offset = ztimer_now(self->lower) - self->origin_lower;
    uint32_t now_self = self->origin_self + frac_scale(&self->scale_now, offset);
    return now_self;
}

static const ztimer_ops_t ztimer_convert_ops = {
    .set    = ztimer_convert_op_set,
    .now    = ztimer_convert_op_now,
    .cancel = ztimer_convert_op_cancel,
};

static void ztimer_convert_compute_scale(ztimer_convert_t *self, uint32_t freq_self, uint32_t freq_lower)
{
    assert(freq_self);
    assert(freq_lower);
    frac_init(&self->scale_now, freq_self, freq_lower);
    frac_init(&self->scale_set, freq_lower, freq_self);
}

void ztimer_convert_init(ztimer_convert_t *self, ztimer_dev_t *lower, uint32_t freq_self, uint32_t freq_lower)
{
    *self = (ztimer_convert_t) {
        .super = { .ops = &ztimer_convert_ops, },
        .lower = lower,
        .lower_entry = { .callback = (void (*)(void *))ztimer_handler, .arg = &self->super, },
        .origin_self = 0,
        .origin_lower = 0,
    };
    DEBUG("xc_init: %p->%p fs=%" PRIu32 " fl=%" PRIu32 "\n",
        (void *)self, (void *)lower, freq_self, freq_lower);
    ztimer_convert_compute_scale(self, freq_self, freq_lower);
}

void ztimer_convert_change_rate(ztimer_convert_t *self, uint32_t freq_self, uint32_t freq_lower)
{
    /* Updating the member variables must be atomic */
    unsigned mask = irq_disable();
    uint32_t now_lower = ztimer_now(self->lower);
    uint32_t offset = now_lower - self->origin_lower;
    uint32_t now_self = self->origin_self + frac_scale(&self->scale_now, offset);
    self->origin_self = now_self;
    self->origin_lower = now_lower;
    ztimer_convert_compute_scale(self, freq_self, freq_lower);
    irq_restore(mask);
    DEBUG("xc: %p new rate: fs=%" PRIu32 " fl=%" PRIu32 " origin: (%" PRIu32 ", %" PRIu32 ")\n",
        (void *)self, freq_self, freq_lower, now_self, now_lower);
}
