/*
 * Copyright (C) 2018 Eistec AB
 *
 * This file is subject to the terms and conditions of the GNU Lesser General
 * Public License v2.1. See the file LICENSE in the top level directory for more
 * details.
 */

/**
 * @ingroup     boards_mulle
 * @{
 *
 * @file
 * @brief       Board specific ztimer configuration for Eistec Mulle
 *
 * @author      Joakim Nohlgård <joakim.nohlgard@eistec.se>
 *
 * @}
 */

#include "ztimer.h"
#include "ztimer/convert.h"
#include "ztimer/extend.h"
#include "ztimer/periph.h"

#define ENABLE_DEBUG (0)
#include "debug.h"

static ztimer_periph_t _ztimer_pit;
static ztimer_periph_t _ztimer_lptmr;
static ztimer_convert_t _ztimer_lptmr_convert;
static ztimer_extend_t _ztimer_lptmr_extend;

ztimer_dev_t *const ZTIMER_USEC = (ztimer_dev_t*) &_ztimer_pit;
ztimer_dev_t *const ZTIMER_MSEC = (ztimer_dev_t*) &_ztimer_lptmr_extend;

void ztimer_board_init(void)
{
    ztimer_periph_init(&_ztimer_pit, TIMER_PIT_DEV(0), 1000000lu);
    _ztimer_pit.adjust = ztimer_diff(ZTIMER_USEC, 100);
    DEBUG("ztimer_board_init(): ZTIMER_US diff=%"PRIu32"\n", _ztimer_pit.adjust);

    ztimer_periph_init(&_ztimer_lptmr, TIMER_LPTMR_DEV(0), 32768lu);
    ztimer_convert_init(&_ztimer_lptmr_convert, (ztimer_dev_t*)&_ztimer_lptmr, 125, 4096);
    ztimer_extend_init(&_ztimer_lptmr_extend, (ztimer_dev_t*)&_ztimer_lptmr_convert, 22);
}
