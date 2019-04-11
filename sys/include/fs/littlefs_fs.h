/*
 * Copyright (C) 2017 OTA keys S.A.
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @defgroup    sys_littlefs  littlefs integration
 * @ingroup     pkg_littlefs
 * @brief       RIOT integration of littlefs
 *
 * @{
 *
 * @file
 * @brief       littlefs integration with vfs
 *
 * @author      Vincent Dupont <vincent@otakeys.com>
 */

#ifndef FS_LITTLEFS_FS_H
#define FS_LITTLEFS_FS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "vfs.h"
#include "lfs.h"
#include "mtd.h"
#include "mutex.h"

/**
 * @name    littlefs configuration
 * @{
 */
#ifndef LITTLEFS_LOOKAHEAD_SIZE
/** Default lookahead size */
#define LITTLEFS_LOOKAHEAD_SIZE     (128 / 8)
#endif

#ifndef LITTLEFS_READ_BUFFER_SIZE
/** Read buffer size, if 0, dynamic allocation is used.
 * If set, it must be read size (mtd page size is used internally as read size) */
#define LITTLEFS_READ_BUFFER_SIZE   (0)
#endif

#ifndef LITTLEFS_PROG_BUFFER_SIZE
/** Prog buffer size, if 0, dynamic allocation is used.
 * If set, it must be program size */
#define LITTLEFS_PROG_BUFFER_SIZE   (0)
#endif

#ifndef LITTLEFS_CACHE_SIZE
/** Cache size, if 0, prog_size is used */
#define LITTLEFS_CACHE_SIZE         LITTLEFS_READ_BUFFER_SIZE
#endif

#if LITTLEFS_READ_BUFFER_SIZE != LITTLEFS_PROG_BUFFER_SIZE
#error "LITTLEFS_READ_BUFFER_SIZE != LITTLEFS_PROG_BUFFER_SIZE"
#endif
#if LITTLEFS_READ_BUFFER_SIZE && (LITTLEFS_READ_BUFFER_SIZE != LITTLEFS_CACHE_SIZE)
#error "LITTLEFS_CACHE_SIZE != LITTLEFS_READ_BUFFER_SIZE"
#endif

#ifndef LITTLEFS_BLOCK_CYCLES
/** Number of erase cycles before we should move data to another block.
 * May be zero, in which case no block-level wear-leveling is performed. */
#define LITTLEFS_BLOCK_CYCLES       (100)
#endif
/** @} */

/**
 * @brief   littlefs descriptor for vfs integration
 */
typedef struct {
    lfs_t fs;                   /**< littlefs descriptor */
    struct lfs_config config;   /**< littlefs config */
    mtd_dev_t *dev;             /**< mtd device to use */
    mutex_t lock;               /**< mutex */
    /** first block number to use,
     * total number of block is defined in @p config.
     * if set to 0, the total number of sectors from the mtd is used */
    uint32_t base_addr;
#if LITTLEFS_READ_BUFFER_SIZE || DOXYGEN
    /** read buffer to use internally if LITTLEFS_READ_BUFFER_SIZE is set */
    uint8_t read_buf[LITTLEFS_READ_BUFFER_SIZE];
#endif
#if LITTLEFS_PROG_BUFFER_SIZE || DOXYGEN
    /** prog buffer to use internally if LITTLEFS_PROG_BUFFER_SIZE is set */
    uint8_t prog_buf[LITTLEFS_PROG_BUFFER_SIZE];
#endif
    /** lookahead buffer to use internally */
    __attribute__((aligned(8))) uint64_t lookahead_buf[LITTLEFS_LOOKAHEAD_SIZE];
} littlefs_desc_t;

/** The littlefs vfs driver */
extern const vfs_file_system_t littlefs_file_system;

#ifdef __cplusplus
}
#endif

#endif /* FS_LITTLEFS_FS_H */
/** @} */
