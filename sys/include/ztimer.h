/*
 * Copyright (C) 2018 Kaspar Schleiser <kaspar@schleiser.de>
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @defgroup    sys_ztimer ztimer
 * @ingroup     sys
 * @brief       High level timer abstraction layer
 *
 * ztimer provides a high level abstraction of hardware timers for application
 * timing needs.
 *
 * The system is composed of ztimer objects which can be chained to create
 * an abstract view of a hardware timer. Each ztimer object acts as a filter on
 * the next object. At the end of each ztimer chain there is always some kind of
 * timer device object.
 *
 * TODO: Add block diagram figures
 *
 * Hardware interface submodules:
 *
 * - @ref ztimer_rtt_init "ztimer_rtt" interface for periph_rtt
 * - @ref ztimer_periph_init "ztimer_periph" interface for periph_timer
 *
 * Filter submodules:
 *
 * - @ref ztimer_extend_init "ztimer_extend" for hardware timer width extension
 * - @ref ztimer_convert_init "ztimer_convert" for frequency conversion
 * - @ref ztimer_dynfreq_init "ztimer_dynfreq" runtime adjustable frequency conversion
 * - @ref ztimer_utc_init "ztimer_utc" interface for clock synchronization libraries
 *
 * @{
 *
 * @file
 * @brief       ztimer API
 *
 * @author      Kaspar Schleiser <kaspar@schleiser.de>
 */

#ifndef ZTIMER_H
#define ZTIMER_H

#include <stdint.h>

#include "msg.h"

/**
 * @brief ztimer_base_t forward declaration
 */
typedef struct ztimer_base ztimer_base_t;

/**
 * @brief ztimer_dev_t forward declaration
 */
typedef struct ztimer_dev ztimer_dev_t;

/**
 * @brief   Minimum information for each timer
 */
struct ztimer_base {
    ztimer_base_t *next;        /**< next timer in list             */
    uint32_t offset;            /**< offset from last timer in list */
};

/**
 * @brief   ztimer structure
 *
 * This type represents an instance of an alarm, which is set on an
 * underlying clock object
 */
typedef struct {
    ztimer_base_t base;         /**< timer list entry               */
    void(*callback)(void*arg);  /**< timer callback function ptr    */
    void *arg;                  /**< timer callback argument        */
} ztimer_t;

/**
 * @brief   ztimer backend method structure
 *
 * This table contains pointers to the virtual methods for a ztimer clock
 *
 * These functions used by ztimer core to interact with the underlying clock.
 */
typedef struct {
    void(*set)(ztimer_dev_t *ztimer, uint32_t val);
    uint32_t(*now)(ztimer_dev_t *ztimer);
    void(*cancel)(ztimer_dev_t *ztimer);
    void(*trigger)(ztimer_dev_t *dev, ztimer_base_t *timer);
} ztimer_ops_t;

/**
 * @brief   ztimer device structure
 */
struct ztimer_dev {
    ztimer_base_t list;         /**< list of active timers          */
    const ztimer_ops_t *ops;    /**< ptr to methods structure       */
};

/**
 * @brief   main ztimer callback handler
 */
void ztimer_handler(ztimer_dev_t *ztimer);

/* User API */
/**
 * @brief   Set an alarm on a clock
 *
 * This will place @p entry in the alarm targets queue for @p ztimer.
 *
 * @note The memory pointed to by @p entry is not copied and must 
 *       remain in scope until the callback is fired or the alarm 
 *       is removed via @ref ztimer_remove
 *
 * @param[in]   ztimer      ztimer clock to operate on
 * @param[in]   entry       alarm entry to enqueue
 * @param[in]   val         alarm target
 */
void ztimer_set(ztimer_dev_t *ztimer, ztimer_t *entry, uint32_t val);

/**
 * @brief   Remove an alarm from a clock
 *
 * This will place @p entry in the timer targets queue for @p ztimer.
 * 
 * This function does nothing if @p entry is not found in the alarm queue of @p ztimer
 *
 * @param[in]   ztimer      ztimer clock to operate on
 * @param[in]   entry       alarm entry to enqueue
 * @param[in]   val         alarm target
 */
void ztimer_remove(ztimer_dev_t *ztimer, ztimer_t *entry);

uint64_t ztimer_now64(void);
void ztimer_set_msg(ztimer_dev_t *dev, ztimer_t *timer, uint32_t offset, msg_t *msg, kernel_pid_t target_pid);
int ztimer_msg_receive_timeout(ztimer_dev_t *dev, msg_t *msg, uint32_t timeout);

static inline uint32_t ztimer_now(ztimer_dev_t *ztimer)
{
    return ztimer->ops->now(ztimer);
}

void ztimer_periodic_wakeup(ztimer_dev_t *ztimer, uint32_t *last_wakeup, uint32_t period);
void ztimer_sleep(ztimer_dev_t *ztimer, uint32_t duration);

/* TODO is this overhead calibration?? */
uint32_t ztimer_diff(ztimer_dev_t *ztimer, uint32_t base);

void ztimer_board_init(void);

/* default ztimer virtual devices */
extern ztimer_dev_t *const ZTIMER_USEC;
extern ztimer_dev_t *const ZTIMER_MSEC;

#endif /* ZTIMER_H */
