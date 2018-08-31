/*
 * Copyright (C) 2018 Kaspar Schleiser <kaspar@schleiser.de>
 *
 * This file is subject to the terms and conditions of the GNU Lesser General
 * Public License v2.1. See the file LICENSE in the top level directory for more
 * details.
 */

#include "ztimer.h"

typedef struct {
    ztimer_dev_t super;
    ztimer_dev_t *parent;
    ztimer_t parent_entry;
    uint16_t mul;
    uint16_t div;
} ztimer_convert_t;

void ztimer_convert_init(ztimer_convert_t *ztimer_convert, ztimer_dev_t *parent, unsigned mul, unsigned div);
