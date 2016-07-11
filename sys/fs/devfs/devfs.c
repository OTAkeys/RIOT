/*
 * Copyright (C) 2016 Eistec AB
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "fs/devfs.h"
#include "vfs.h"
#include "mutex.h"

#define ENABLE_DEBUG (1)
#include "debug.h"

/**
 * @internal
 * @brief DevFS list head
 *
 * DevFS operates as a singleton, the same files show up in all mounted instances.
 */
static devfs_t *_devfs_head;
/**
 * @internal
 * @brief mutex to protect the DevFS list from corruption
 */
static mutex_t _devfs_mutex = MUTEX_INIT;

/* No need for file system operations, no extra work to be done on
 * mount/umount. unlink is not permitted, use devfs_unregister instead */

/* File operations */
/* Only open is overloaded to allow searching for the correct device */
static int devfs_open(vfs_file_t *filp, const char *name, int flags, int mode, const char *abs_path);
static int devfs_fcntl(vfs_file_t *filp, int cmd, int arg);

static const vfs_file_ops_t devfs_file_ops = {
    .open  = devfs_open,
    .fcntl = devfs_fcntl,
};

const vfs_file_system_t devfs_file_system = {
    .f_op = &devfs_file_ops,
};

static int devfs_open(vfs_file_t *filp, const char *name, int flags, int mode, const char *abs_path)
{
    DEBUG("devfs_open: %p, \"%s\", 0x%x, %04o, \"%s\"\n", (void *)filp, name, flags, mode, abs_path);
    /* linear search through the device list */
    devfs_t *node = _devfs_head;
    while (node != NULL) {
        if (strcmp(node->path, name) == 0) {
            DEBUG("devfs_open: Found :)\n");
            /* Add private data from DevFS node */
            filp->private_data = node->private_data;
            /* Replace f_op with the operations provided by the device driver */
            filp->f_op = node->f_op;
            break;
        }
        node = node->next;
    }
    if (node == NULL) {
        DEBUG("devfs_open: Not found :(\n");
        return -ENOENT;
    }
    /* Chain the open() method for the specific device */
    if (filp->f_op->open != NULL) {
        return filp->f_op->open(filp, name, flags, mode, abs_path);
    }
    return 0;
}

static int devfs_fcntl(vfs_file_t *filp, int cmd, int arg)
{
    DEBUG("devfs_fcntl: %p, 0x%x, 0x%x\n", (void *)filp, cmd, arg);
    switch (cmd) {
        /* F_GETFL is handled directly by vfs_fcntl */
        case F_SETFL:
            DEBUG("devfs_fcntl: SETFL: %d\n", arg);
            filp->flags = arg;
            return filp->flags;
        default:
            return -EINVAL;
    }
}

int devfs_register(devfs_t *node)
{
    DEBUG("devfs_register: %p\n", (void *)node);
    if (node == NULL) {
        return -EINVAL;
    }
    DEBUG("devfs_register: \"%s\" -> (%p, %p)\n", node->path, (void *)node->f_op, node->private_data);
    if (node->path == NULL) {
        return -EINVAL;
    }
    if (node->f_op == NULL) {
        return -EINVAL;
    }
    mutex_lock(&_devfs_mutex);
    /* linear search through the device list */
    devfs_t *it = _devfs_head;
    while (it != NULL) {
        if (it == node) {
            /* Already registered */
            mutex_unlock(&_devfs_mutex);
            return -EEXIST;
        }
        if (strcmp(it->path, node->path) == 0) {
            /* Path already registered */
            mutex_unlock(&_devfs_mutex);
            return -EEXIST;
        }
        it = it->next;
    }
    /* insert node first in list */
    node->next = _devfs_head;
    _devfs_head = node;
    mutex_unlock(&_devfs_mutex);
    return 0;
}

int devfs_unregister(devfs_t *node)
{
    if (node == NULL) {
        return -EINVAL;
    }
    mutex_lock(&_devfs_mutex);
    if (_devfs_head == node) {
        /* node was list head */
        _devfs_head = node->next;
        mutex_unlock(&_devfs_mutex);
        return 0;
    }
    /* linear search in list */
    devfs_t *it = _devfs_head;
    while (it != NULL) {
        if (it->next == node) {
            it->next = node->next;
            mutex_unlock(&_devfs_mutex);
            return 0;
        }
        it = it->next;
    }
    mutex_unlock(&_devfs_mutex);
    return -ENOENT;
}
