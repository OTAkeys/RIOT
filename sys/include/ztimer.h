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
 * The system is composed of ztimer clocks which can be chained to create
 * an abstract view of a hardware timer/counter device. Each ztimer clock acts
 * as a filter on the next clock in the chain. At the end of each ztimer chain
 * there is always some kind of counter device object.
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
 * @author      Joakim Nohlgård <joakim.nohlgard@eistec.se>
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
 * @brief   Minimum information for each alarm
 */
struct ztimer_base {
    ztimer_base_t *next;        /**< next alarm in list */
    uint32_t offset;            /**< offset from last alarm in list */
};

/**
 * @brief   ztimer structure
 *
 * This type represents an instance of an alarm, which is set on an
 * underlying clock object
 */
typedef struct {
    ztimer_base_t base;         /**< clock list entry */
    void(*callback)(void*arg);  /**< alarm callback function pointer */
    void *arg;                  /**< alarm callback argument */
} ztimer_t;

/**
 * @brief   ztimer backend method structure
 *
 * This table contains pointers to the virtual methods for a ztimer clock.
 *
 * These functions used by ztimer core to interact with the underlying clock.
 */
typedef struct {
    /**
     * @brief   Set a new timer target
     */
    void(*set)(ztimer_dev_t *ztimer, uint32_t val);

    /**
     * @brief   Get the current count of the timer
     */
    uint32_t(*now)(ztimer_dev_t *ztimer);

    /**
     * @brief   Cancel any set target
     */
    void(*cancel)(ztimer_dev_t *ztimer);

    /* TODO: What is the purpose of this method? */
    void(*trigger)(ztimer_dev_t *dev, ztimer_base_t *timer);
} ztimer_ops_t;

/**
 * @brief   ztimer device structure
 */
struct ztimer_dev {
    ztimer_base_t list;         /**< list of active timers */
    const ztimer_ops_t *ops;    /**< pointer to methods structure */
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

/* TODO what is this? */
uint64_t ztimer_now64(void);

/**
 * @brief   Post a message after a delay
 *
 * This function sets an alarm that will send a message @p offset ticks
 * from now.
 *
 * @note The memory pointed at by @p alarm and @p msg will not be copied, i.e.
 *       `*alarm` and `*msg` needs to remain valid until the alarm has occurred.
 *
 * @param[in]   ztimer          ztimer clock to operate on
 * @param[in]   alarm           pointer to alarm struct
 * @param[in]   offset          ticks from now
 * @param[in]   msg             pointer to msg that will be sent
 * @param[in]   target_pid      pid the message will be sent to
 */
void ztimer_set_msg(ztimer_dev_t *dev, ztimer_t *timer, uint32_t offset, msg_t *msg, kernel_pid_t target_pid);

/**
 * @brief receive a message blocking, with timeout
 *
 * @param[in]   ztimer          ztimer clock to operate on
 * @param[out]  msg             pointer to buffer which will be filled if a message is received
 * @param[in]   timeout         relative timeout, in @p ztimer time units
 *
 * @return  >=0 if a message was received
 * @return  <0 on timeout
 */
int ztimer_msg_receive_timeout(ztimer_dev_t *dev, msg_t *msg, uint32_t timeout);

/**
 * @brief   Get the current time from a clock
 *
 * @param[in]   ztimer          ztimer clock to operate on
 *
 * @return  Current count on the clock @p ztimer
 */
static inline uint32_t ztimer_now(ztimer_dev_t *ztimer)
{
    return ztimer->ops->now(ztimer);
}

/**
 * @brief Suspend the calling thread until the time (@p last_wakeup + @p period)
 *
 * This function can be used to create periodic wakeups.
 *
 * When the function returns, @p last_wakeup is set to
 * (@p last_wakeup + @p period).
 *
 * @c last_wakeup should be set to ztimer_now(@p ztimer) before first call of the
 * function.
 *
 * If the time (@p last_wakeup + @p period) has already passed, the function
 * sets @p last_wakeup to @p last_wakeup + @p period and returns immediately.
 *
 * @param[in]   ztimer          ztimer clock to operate on
 * @param[in]   last_wakeup     base time stamp for the wakeup
 * @param[in]   period          time in ticks that will be added to @p last_wakeup
 */
void ztimer_periodic_wakeup(ztimer_dev_t *ztimer, uint32_t *last_wakeup, uint32_t period);

/**
 * @brief   Put the calling thread to sleep for the specified number of ticks
 *
 * @param[in]   ztimer          ztimer clock to use
 * @param[in]   duration        duration of sleep, in @p ztimer time units
 */
void ztimer_sleep(ztimer_dev_t *ztimer, uint32_t duration);

/* TODO is this overhead calibration?? */
uint32_t ztimer_diff(ztimer_dev_t *ztimer, uint32_t base);

/**
 * @brief   Initialize the default ztimer configuration for the board
 */
void ztimer_board_init(void);

/* default ztimer virtual devices */
/**
 * @brief   Default ztimer microsecond clock
 */
extern ztimer_dev_t *const ZTIMER_USEC;

/**
 * @brief   Default ztimer millisecond clock
 */
extern ztimer_dev_t *const ZTIMER_MSEC;

#endif /* ZTIMER_H */
