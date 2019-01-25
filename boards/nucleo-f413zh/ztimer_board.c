/*
 * Copyright (C) 2018 SKF AB
 *
 * This file is subject to the terms and conditions of the GNU Lesser General
 * Public License v2.1. See the file LICENSE in the top level directory for more
 * details.
 */

/**
 * @ingroup     boards_frdm-kw41z
 * @{
 *
 * @file
 * @brief       Board specific ztimer configuration for the FRDM-KW41Z development board
 *
 * @author      Joakim Nohlgård <joakim.nohlgard@eistec.se>
 *
 * @}
 */

#include "ztimer.h"
#include "ztimer/rtt.h"
#include "ztimer/periph.h"

#define ENABLE_DEBUG (0)
#include "debug.h"

static ztimer_periph_t _ztimer_periph;
static ztimer_rtt_t _ztimer_rtt;

ztimer_dev_t *const ZTIMER_USEC = (ztimer_dev_t*) &_ztimer_rtt;

void ztimer_board_init(void)
{
    ztimer_periph_init(&_ztimer_periph, 0, 1000000lu);
    ztimer_rtt_init(&_ztimer_rtt);
}
