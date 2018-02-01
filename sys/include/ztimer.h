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
 * @brief       Provides a flexible timer API
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
typedef struct ztimer_instance ztimer_dev_t;

/**
 * @brief   Minimum information for each timer
 */
struct ztimer_base {
    ztimer_base_t *next;        /**< next timer in list             */
    uint32_t offset;            /**< offset from last timer in list */
};

/**
 * @brief   ztimer structure
 */
typedef struct {
    ztimer_base_t base;         /**< timer list entry               */
    void(*callback)(void*arg);  /**< timer callback function ptr    */
    void *arg;                  /**< timer callback argument        */
} ztimer_t;

/**
 * @brief   ztimer backend method structure
 */
typedef struct {
    void(*set)(ztimer_dev_t *ztimer, uint32_t val);
    uint32_t(*now)(ztimer_dev_t *ztimer);
    void(*cancel)(ztimer_dev_t *ztimer);
    void(*trigger)(ztimer_dev_t *instance, ztimer_base_t *timer);
} ztimer_ops_t;

/**
 * @brief   ztimer instance (backend) structure
 */
struct ztimer_instance {
    ztimer_base_t list;         /**< list of active timers          */
    const ztimer_ops_t *ops;    /**< ptr to methods structure       */
};

void ztimer_handler(ztimer_dev_t *ztimer);

/* User API */
void ztimer_set(ztimer_dev_t *ztimer, ztimer_t *entry, uint32_t val);
void ztimer_remove(ztimer_dev_t *ztimer, ztimer_t *entry);

uint64_t ztimer_now64(void);
void ztimer_set_msg(ztimer_dev_t *instance, ztimer_t *timer, uint32_t offset, msg_t *msg, kernel_pid_t target_pid);
int ztimer_msg_receive_timeout(ztimer_dev_t *instance, msg_t *msg, uint32_t timeout);

static inline uint32_t ztimer_now(ztimer_dev_t *ztimer)
{
    return ztimer->ops->now(ztimer);
}

void ztimer_periodic_wakeup(ztimer_dev_t *ztimer, uint32_t *last_wakeup, uint32_t period);
void ztimer_sleep(ztimer_dev_t *ztimer, uint32_t duration);

uint32_t ztimer_diff(ztimer_dev_t *ztimer, uint32_t base);

void ztimer_board_init(void);

/* default ztimer instances */
extern ztimer_dev_t *const ZTIMER_USEC;
extern ztimer_dev_t *const ZTIMER_MSEC;

#endif /* ZTIMER_H */
