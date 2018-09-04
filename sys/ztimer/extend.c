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
 * @pre Interrupts masked
 *
 * @param[in]   arg     pointer to the owner @ref ztimer_extend_t instance
 */
static void ztimer_extend_overflow_callback(void* arg);

/**
 * @brief   Extend a timestamp from the lower counter to the full width virtual clock
 *
 * To prevent data races, the origin value should be confirmed to be the same
 * before and after the lower counter is sampled. This is however not necessary
 * if interrupts are disabled beforehand.
 *
 * @param[in]   self        instance to operate on
 * @param[in]   lower_now   counter value in the lower counter
 * @param[in]   origin      origin value at the time when the lower counter was sampled
 *
 * @return  full width extended counter value
 */
static uint32_t ztimer_extend_now32(ztimer_extend_t *self, uint32_t lower_now, uint32_t origin);

/**
 * @brief   Update the ztimer queue for the lower clock
 *
 * @pre Interrupts masked
 *
 * @param[in]   self        instance to operate on
 */
static void ztimer_extend_update(ztimer_extend_t *self);

static void ztimer_extend_alarm_callback(void* arg)
{
    ztimer_extend_t *self = (ztimer_extend_t *)arg;
    DEBUG("ztimer_extend_alarm_callback()\n");
    ztimer_handler(&self->super);
}

static void ztimer_extend_overflow_callback(void* arg)
{
    ztimer_extend_t *self = (ztimer_extend_t *)arg;
    /* Update origin and update targets */
    uint32_t lower_now = self->lower->list.offset;
    uint32_t now32 = ztimer_extend_now32(self, lower_now, self->origin);
    self->origin = now32 & ~(self->partition_mask);
    DEBUG("zx: partition, origin = 0x%08" PRIx32 "\n", self->origin);

    /* Ensure that there is always at least one alarm target inside
     * each partition, in order to always detect timer rollover */
    /* TODO: This works for the rollover in the lower counter only because of a
     * peculiarity of the ztimer_core handler which will execute all waiting
     * timers if the counter moves backwards (unsigned diff with a negative number)
     * TODO: IMPORTANT Rework this code if the implementation of core.c
     * _update_head_offset is altered */
    ztimer_set(self->lower, &self->lower_overflow_entry, self->partition_mask + 1);

    /* Update alarms */
    ztimer_extend_update(self);
}

static uint32_t ztimer_extend_now32(ztimer_extend_t *self, uint32_t lower_now, uint32_t origin)
{
    uint32_t now32 = (lower_now - origin) & self->lower_max;
    now32 += origin;
    return now32;
}

static void ztimer_extend_update(ztimer_extend_t *self)
{
    if (!self->super.list.next) {
        /* No alarms queued */
        return;
    }
    uint32_t lower_now = ztimer_now(self->lower);
    uint32_t now32 = ztimer_extend_now32(self, lower_now, self->origin);
    uint32_t target = self->super.list.offset + self->super.list.next->offset;
    target -= now32;
    if ((lower_now + target) > self->lower_max) {
        /* Await counter rollover first */
        return;
    }
    DEBUG("zx: set lower_alarm %p, target=%" PRIu32 "\n",
        (void *)&self->lower_alarm_entry, target);
    ztimer_set(self->lower, &self->lower_alarm_entry, target);
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
    uint32_t origin;
    uint32_t lower_now;
    do {
        origin = self->origin;
        lower_now = ztimer_now(self->lower);
    } while (origin != self->origin);
    uint32_t now32 = ztimer_extend_now32(self, lower_now, origin);
    DEBUG("zx: now = 0x%08" PRIx32 "\n", now32);
    return now32;
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
