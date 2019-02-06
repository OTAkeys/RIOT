/*
 * Copyright (C) 2018 SKF AB
 *
 * This file is subject to the terms and conditions of the GNU Lesser General
 * Public License v2.1. See the file LICENSE in the top level directory for more
 * details.
 */

/**
 * @ingroup     boards_nucleo413zh
 * @{
 *
 * @file
 * @brief       Board specific ztimer configuration for the Nucleo STM32F413ZH development board
 *
 * @author      Javier FILEIV <javier.fileiv@gmail.com>
 *
 * @}
 */

#include "ztimer.h"
#include "ztimer/rtt.h"
#include "ztimer/extend.h"

#define ENABLE_DEBUG (0)
#include "debug.h"

static ztimer_rtt_t _ztimer_rtt;
static ztimer_extend_t _ztimer_lptmr_extend;

ztimer_dev_t *const ZTIMER_USEC = (ztimer_dev_t*) &_ztimer_rtt;
ztimer_dev_t *const ZTIMER_MSEC = (ztimer_dev_t*) &_ztimer_lptmr_extend;

void ztimer_board_init(void)
{
    ztimer_rtt_init(&_ztimer_rtt);
//    ztimer_extend_init(&_ztimer_lptmr_extend, (ztimer_dev_t*)&_ztimer_rtt, 16);
}
