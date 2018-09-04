/*
 * Copyright (C) 2018 Kaspar Schleiser <kaspar@schleiser.de>
 * Copyright (C) 2018 Eistec AB
 *
 * This file is subject to the terms and conditions of the GNU Lesser General
 * Public License v2.1. See the file LICENSE in the top level directory for more
 * details.
 */

/**
 * @defgroup  sys_ztimer_convert ztimer_convert frequency conversion layer
 * @ingroup   sys_ztimer
 * @brief     Translates between clock tick rates
 *
 * Translates the ticks of an underlying clock into virtual ticks at a different
 * frequency.
 *
 * @{
 * @file
 * @brief   ztimer_convert interface definitions
 *
 * @author  Kaspar Schleiser <kaspar@schleiser.de>
 * @author  Joakim Nohlgård <joakim.nohlgard@eistec.se>
 */
#ifndef ZTIMER_CONVERT_H
#define ZTIMER_CONVERT_H

#include <stdint.h>
#include "ztimer.h"
#include "frac.h"

/**
 * @brief   ztimer_convert frequency conversion layer class
 */
typedef struct {
    /**
     * @brief   Superclass instance
     */
    ztimer_dev_t super;
    /**
     * @brief   pointer to underlying clock
     */
    ztimer_dev_t *lower;
    /**
     * @brief   Target alarm entry for underlying clock
     */
    ztimer_t lower_entry;
    /**
     * @brief   Frequency conversion scaling constant from lower to self
     */
    frac_t scale_now;
    /**
     * @brief   Frequency conversion scaling constant from self to lower
     */
    frac_t scale_set;
    /**
     * @brief   base count in this counter
     */
    uint32_t origin_self;
    /**
     * @brief   base count in lower counter
     */
    uint32_t origin_lower;
} ztimer_convert_t;

/**
 * @brief   @ref ztimer_convert_t constructor
 *
 * @param[in]   self        pointer to instance being initialized
 * @param[in]   lower       pointer to underlying clock
 * @param[in]   freq_self   desired frequency of this clock
 * @param[in]   freq_lower  frequency of the underlying clock
 */
void ztimer_convert_init(ztimer_convert_t *self, ztimer_dev_t *lower, uint32_t freq_self, uint32_t freq_lower);

/**
 * @brief   Change the scaling without affecting the current count
 *
 * @param[in]   self        pointer to instance being initialized
 * @param[in]   freq_self   desired frequency of this clock
 * @param[in]   freq_lower  frequency of the underlying clock
 */
void ztimer_convert_change_rate(ztimer_convert_t *self, uint32_t freq_self, uint32_t freq_lower);

#endif /* ZTIMER_CONVERT_H */
/** @} */
