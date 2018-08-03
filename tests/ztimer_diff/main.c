/*
 * Copyright (C) 2018 Kaspar Schleiser <kaspar@schleiser.de>
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     test
 * @{
 *
 * @file
 * @brief       ztimer diff test application
 *
 * @author      Kaspar Schleiser <kaspar@schleiser.de>
 *
 * @}
 */

#include <stdint.h>
#include <stdio.h>

#include "ztimer.h"
#include "ztimer/periph.h"

#define SAMPLES 1024

static ztimer_periph_t _ztimer_periph;

int main(void)
{
    ztimer_periph_init(&_ztimer_periph, 0, 1000000LU);

    uint32_t total = 0;

    uint16_t min = 0xFFFF;
    uint16_t max = 0;

    unsigned n = SAMPLES;
    while (n--) {
        unsigned diff = ztimer_diff((ztimer_dev_t *)&_ztimer_periph, 1000);
        total += diff;
        if (diff < min) {
            min = diff;
        }
        else if (diff > max) {
            max = diff;
        }
    }

    printf("min=%u max=%u avg=%u\n", min, max, (total / SAMPLES));

    return 0;
}
