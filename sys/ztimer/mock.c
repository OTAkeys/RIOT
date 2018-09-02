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
 * @brief       ztimer mock implementation
 *
 * @author      Joakim Nohlgård <joakim.nohlgard@eistec.se>
 * @}
 */

#include <stdint.h>
#include <inttypes.h>
#include "ztimer/mock.h"

#define ENABLE_DEBUG (0)
#include "debug.h"

/* Functions for controlling the mock clock below */

void ztimer_mock_advance(ztimer_mock_t *self, uint32_t val)
{
    DEBUG("zmock_advance: now=0x%08" PRIx32 " + 0x%08" PRIx32 ", target=0x%08" PRIx32 " (%u)\n",
        self->now, val, self->target, self->armed);
    self->now = (self->now + val) & self->mask;
    if (self->armed) {
        /* Update target */
        if (val >= self->target) {
            /* Target was hit */
            ztimer_mock_fire(self);
        }
        else {
            self->target -= val;
        }
    }
}

void ztimer_mock_jump(ztimer_mock_t *self, uint32_t target)
{
    self->now = target & self->mask;
    DEBUG("zmock_jump: now=0x%08" PRIx32 ", target=0x%08" PRIx32 " (%u)\n",
        self->now, self->target, self->armed);
    /* Do not touch target */
}

void ztimer_mock_fire(ztimer_mock_t *self)
{
    DEBUG("zmock_fire: now=0x%08" PRIx32 ", target=0x%08" PRIx32 " (%u)\n",
        self->now, self->target, self->armed);
    self->target = 0;
    self->armed = 0;
    /* Execute ztimer core interrupt handler */
    ztimer_handler(&self->super);
}

/* Implementations for the standard ztimer operations below */

static void ztimer_mock_op_set(ztimer_dev_t *dev, uint32_t val)
{
    ztimer_mock_t *self = (ztimer_mock_t*)dev;
    self->target = val & self->mask;
    self->armed = 1;
    DEBUG("zmock_set: now=0x%08" PRIx32 ", target=0x%08" PRIx32 " (%u)\n",
        self->now, self->target, self->armed);
}

static uint32_t ztimer_mock_op_now(ztimer_dev_t *dev)
{
    ztimer_mock_t *self = (ztimer_mock_t*)dev;
    DEBUG("zmock_now: now=0x%08" PRIx32 ", target=0x%08" PRIx32 " (%u)\n",
        self->now, self->target, self->armed);
    return self->now;
}

static void ztimer_mock_op_cancel(ztimer_dev_t *dev)
{
    ztimer_mock_t *self = (ztimer_mock_t*)dev;
    DEBUG("zmock_cancel: now=0x%08" PRIx32 ", target=0x%08" PRIx32 " (%u)\n",
        self->now, self->target, self->armed);
    self->armed = 0;
}

static const ztimer_ops_t ztimer_mock_ops = {
    .set    = ztimer_mock_op_set,
    .now    = ztimer_mock_op_now,
    .cancel = ztimer_mock_op_cancel,
};

void ztimer_mock_init(ztimer_mock_t *self, unsigned width)
{
    *self = (ztimer_mock_t){
        .mask = (~((uint32_t)0ul)) >> (32 - width),
        .super = { .ops = &ztimer_mock_ops, },
    };
    DEBUG("zmock_init: %p width=%u mask=0x%08" PRIx32 "\n", (void *)self, width, self->mask);
}
