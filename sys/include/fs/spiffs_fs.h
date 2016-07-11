/*
 * Copyright (C) 2016 OTA keys S.A.
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

#ifndef SPIFFS_FS_H
#define SPIFFS_FS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "spiffs.h"
#include "spiffs_config.h"
#include "vfs.h"
#include "mtdi.h"
#include "mutex.h"

#ifndef SPIFFS_FS_CACHE_SIZE
#if SPIFFS_CACHE
#define SPIFFS_FS_CACHE_SIZE (512)
#else
#define SPIFFS_FS_CACHE_SIZE (0)
#endif
#endif
#ifndef SPIFFS_FS_WORK_SIZE
#define SPIFFS_FS_WORK_SIZE  (512)
#endif
#ifndef SPIFFS_FS_FD_SPACE_SIZE
#define SPIFFS_FS_FD_SPACE_SIZE (50)
#endif

/**
 * This contains anything needed to run an instance of SPIFFS
 */
typedef struct spiffs_desc {
    spiffs fs;                                  /**< The SPIFFS struct */
    uint8_t work[SPIFFS_FS_WORK_SIZE];          /**< SPIFFS work buffer */
    uint8_t fd_space[SPIFFS_FS_FD_SPACE_SIZE];  /**< SPIFFS file descriptor cache */
#if SPIFFS_CACHE
    uint8_t cache[SPIFFS_FS_CACHE_SIZE];        /**< SPIFFS cache */
#endif
    spiffs_config config;                       /**< SPIFFS config, filled at mount time depending
                                                 *  on the underlying mtdi_dev_t @p dev */
    mutex_t lock;                               /**< A lock for SPIFFS internal use */
    mtdi_dev_t *dev;                            /**< The underlying mtdi device, must be set by user */
} spiffs_desc_t;

/** The SPIFFS vfs driver, a pointer to a spiffs_desc_t must be provided as vfs_mountp::private_data */
extern const vfs_file_system_t spiffs_file_system;

#ifdef __cplusplus
}
#endif

#endif /* SPIFFS_FS_H */
