/*
 * Copyright (C) 2018 Kaspar Schleiser <kaspar@schleiser.de>
 *
 * This file is subject to the terms and conditions of the GNU Lesser General
 * Public License v2.1. See the file LICENSE in the top level directory for more
 * details.
 */

#include "ztimer.h"
#include "periph/timer.h"

typedef struct {
    ztimer_dev_t super;
    tim_t dev;
    uint32_t adjust;
} ztimer_periph_t;

void ztimer_periph_init(ztimer_periph_t *ztimer, tim_t dev, unsigned long freq);
