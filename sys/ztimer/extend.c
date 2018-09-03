/*
 * Copyright (C) 2018 Eistec AB
 *
 * This file is subject to the terms and conditions of the GNU Lesser General
 * Public License v2.1. See the file LICENSE in the top level directory for more
 * details.
 */

/**
 * @ingroup     sys_ztimer
 *
 * @{
 *
 * @file
 * @brief       ztimer_extend implementation
 *
 * @author      Joakim Nohlgård <joakim.nohlgard@eistec.se>
 *
 * @}
 */

#include <stdint.h>
#include <stdatomic.h>
#include <inttypes.h>

#include "ztimer/extend.h"

#define ENABLE_DEBUG (0)
#include "debug.h"

/**
 * @brief   Callback for alarm target in the lower clock
 *
 * @param[in]   arg     pointer to the owner @ref ztimer_extend_t instance
 */
static void ztimer_extend_alarm_callback(void* arg);

/**
 * @brief   Callback for partition update in the lower clock
 *
 * This will be scheduled in the lower clock at @ref ztimer_extend_t::partition_size intervals
 *
 * @param[in]   arg     pointer to the owner @ref ztimer_extend_t instance
 */
static void ztimer_extend_overflow_callback(void* arg);

/**
 * @brief   Update the ztimer queue for the lower clock
 *
 * @param[in]   self        instance to operate on
 */
static void ztimer_extend_update(ztimer_extend_t *self);

/**
 * @brief   Check for partition transitions and update origin accordingly
 *
 * @param[in]   self        instance to operate on
 *
 * @return  current extended counter value
 */
static uint32_t ztimer_extend_checkpoint(ztimer_extend_t *self);

static void ztimer_extend_alarm_callback(void* arg)
{
    ztimer_extend_t *self = (ztimer_extend_t *)arg;
    DEBUG("ztimer_extend_alarm_callback()\n");
    ztimer_handler(&self->super);
}

static void ztimer_extend_overflow_callback(void* arg)
{
    ztimer_extend_t *self = (ztimer_extend_t *)arg;
    DEBUG("ztimer_extend_overflow_callback()\n");

    /* Update origin and update targets */
    ztimer_extend_update(self);
    /* Ensure that there is always at least one alarm target inside
     * each partition, in order to always detect timer rollover */
    ztimer_set(self->lower, &self->lower_overflow_entry, self->partition_mask + 1);
}

static void ztimer_extend_update(ztimer_extend_t *self)
{
    /* Keep origin up to date even when no target is set */
    uint32_t now = ztimer_extend_checkpoint(self);
    if (!self->super.list.next) {
        return;
    }
    uint32_t next_target = self->super.list.offset + self->super.list.next->offset;
    uint32_t target_period = (next_target & ~(self->lower_max));
    uint32_t now_period = (now & ~(self->lower_max));
    if (now_period != target_period) {
        /* Await counter rollover first */
        return;
    }
    next_target -= now;
    DEBUG("zx: set lower_alarm %p, target=%" PRIu32 "\n",
        (void *)&self->lower_alarm_entry, next_target);
    ztimer_set(self->lower, &self->lower_alarm_entry, next_target);
}

static uint32_t ztimer_extend_checkpoint(ztimer_extend_t *self)
{
    uint32_t origin;
    uint32_t lower_now;
    uint32_t old_origin;
    do {
        do {
            old_origin = self->origin;
            lower_now = ztimer_now(self->lower);
            origin = self->origin;
        } while (origin != old_origin);
        uint32_t partition = (lower_now & ~(self->partition_mask));
        uint32_t old_partition = (old_origin & self->lower_max);
        if (partition != old_partition) {
            origin += (partition - old_partition) & (self->lower_max);
            if (!atomic_compare_exchange_strong(&self->origin, &old_origin, origin)) {
                continue;
            }
        }
        break;
    } while (1);
    /* lower_now and origin should have the same partition bits at this point */
    uint32_t now = (origin | lower_now);
    DEBUG("zx: now=%" PRIu32 "\n", now);
    return now;
}

static void ztimer_extend_op_set(ztimer_dev_t *z, uint32_t val)
{
    (void)val;
    ztimer_extend_t *self = (ztimer_extend_t *)z;

    ztimer_extend_update(self);
}

static void ztimer_extend_op_cancel(ztimer_dev_t *z)
{
    ztimer_extend_t *self = (ztimer_extend_t *) z;
    ztimer_remove(self->lower, &self->lower_alarm_entry);
}

static uint32_t ztimer_extend_op_now(ztimer_dev_t *z)
{
    ztimer_extend_t *self = (ztimer_extend_t *) z;
    return ztimer_extend_checkpoint(self);
}

static const ztimer_ops_t ztimer_extend_ops = {
    .set    = ztimer_extend_op_set,
    .now    = ztimer_extend_op_now,
    .cancel = ztimer_extend_op_cancel,
};

void ztimer_extend_init(ztimer_extend_t *self, ztimer_dev_t *lower, unsigned lower_width)
{
    uint32_t now = ztimer_now(lower);
    uint32_t lower_max = (1ul << lower_width) - 1;
    *self = (ztimer_extend_t) {
        .super = { .ops = &ztimer_extend_ops, },
        .lower = lower,
        .lower_alarm_entry = { .callback = ztimer_extend_alarm_callback, .arg = self, },
        .lower_overflow_entry = { .callback = ztimer_extend_overflow_callback, .arg = self, },
        .lower_max = lower_max,
        .partition_mask = (lower_max >> 2),
    };
    self->origin = now & ~(self->partition_mask);
    DEBUG("zx_init: %p lower=%p lower_max=0x%08" PRIx32 " partition_mask=0x%08" PRIx32 "\n",
        (void *)self, (void *)lower, lower_max, self->partition_mask);

    /* Ensure that there is always at least one alarm target inside
     * each partition, in order to always detect timer rollover */
    ztimer_set(lower, &self->lower_overflow_entry, self->partition_mask + 1);
}
