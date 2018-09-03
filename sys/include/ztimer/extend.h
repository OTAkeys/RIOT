/*
 * Copyright (C) 2018 Eistec AB
 * Copyright (C) 2018 Kaspar Schleiser <kaspar@schleiser.de>
 *
 * This file is subject to the terms and conditions of the GNU Lesser General
 * Public License v2.1. See the file LICENSE in the top level directory for more
 * details.
 */

/**
 * @defgroup  sys_ztimer_extend ztimer_extend counter width extender
 * @ingroup   sys_ztimer
 * @brief     Extends a counter to a 32 bit virtual counter.
 *
 * Provides a monotonic virtual 32 bit clock by wrapping another clock, which
 * can be of any smaller width, using checkpointed interval partitioning.
 *
 * ### Theory of operation
 *
 * The underlying clock range is split into multiple equal length intervals
 * called partitions. The underlying alarm target is never scheduled further
 * into the future than the length of one partition. This behaviour prevents the
 * ambiguity in how the software should interpret the counter values in relation
 * to setting alarm targets.
 *
 * An internal state is used to keep track of the 32 bit virtual alarm target,
 * some internal flags, and the 32 bit offset from the underlying clock.
 *
 * #### Checkpointing
 *
 * A checkpoint is updated every time the underlying counter is read by the library.
 * The virtual clock offset is updated whenever the underlying counter transitions
 * into a new partition.
 *
 * #### Long timeouts
 *
 * When an alarm target is requested which is further into the future than the
 * entire width of the underlying timer, the wrapper will set successive
 * partition length alarms on the underlying clock until the target is within
 * reach of the underlying timer.
 *
 * #### Race conditions
 *
 * The checkpoint function may be run from multiple threads, and is therefore
 * sensitive to data races. The implementation uses atomic operations to ensure
 * that threads do not interfere with each other. This solution may cause
 * additional calls to the @ref ztimer_ops_t::now "now" method on the underlying
 * timer because of retries when a data race is detected.
 *
 * @{
 * @file
 * @brief   ztimer_extend interface definitions
 * @author  Joakim Nohlgård <joakim.nohlgard@eistec.se>
 */
#ifndef ZTIMER_EXTEND_H
#define ZTIMER_EXTEND_H
#include <stdint.h>

#include "ztimer.h"

/**
 * @brief   ztimer counter width extender clock wrapper class
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
    ztimer_t lower_alarm_entry;
    /**
     * @brief   Partition transition alarm entry for underlying clock
     */
    ztimer_t lower_overflow_entry;
    /**
     * @brief   Offset from underlying timer to virtual long counter
     */
    uint32_t origin;
    /**
     * @brief   Maximum settable timeout for the lower level timer
     *
     * @ref ztimer_extend_init will set this to the correct value based on the
     * lower counter width.
     */
    uint32_t lower_max;
    /**
     * @brief   Bit mask for the counter bits inside the partition
     *
     * @ref ztimer_extend_init will set this to the correct value based on the
     * lower counter width.
     */
    uint32_t partition_mask;
} ztimer_extend_t;

/**
 * @brief   @ref ztimer_extend_t constructor
 *
 * @param[in]   self        pointer to instance being initialized
 * @param[in]   lower       pointer to underlying clock
 * @param[in]   lower_bits  number of bits in the underlying counter
 */
void ztimer_extend_init(ztimer_extend_t *self, ztimer_dev_t *lower, unsigned lower_bits);

#endif /* ZTIMER_EXTEND_H */
/** @} */
