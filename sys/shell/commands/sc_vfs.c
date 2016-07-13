/*
 * Copyright (C) 2016 Eistec AB
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     sys_shell_commands
 * @{
 *
 * @file
 * @brief       Shell commands for the VFS module
 *
 * @author      Joakim Nohlgård <joakim.nohlgard@eistec.se>
 *
 * @}
 */

#if MODULE_VFS
#include <stdint.h>
#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

#include "vfs.h"

static void _ls_usage(char **argv)
{
    printf("%s <path>\n", argv[0]);
    puts("list files in <path>");
}

static void _vfs_usage(char **argv)
{
    printf("%s <r|w> <path> [bytes] [offset]\n", argv[0]);
    puts("r: Read [bytes] bytes at [offset] in file <path>");
}

/* Macro used by _errno_string to expand errno labels to string and print it */
#define _case_snprintf_errno_name(x) \
    case x: \
        res = snprintf(buf, buflen, #x); \
        break

static int _errno_string(int err, char *buf, size_t buflen)
{
    int len = 0;
    int res;
    if (err < 0) {
        res = snprintf(buf, buflen, "-");
        if (res < 0) {
            return res;
        }
        if (res <= buflen) {
            buf += res;
            buflen -= res;
        }
        len += res;
        err = -err;
    }
    switch (err) {
        _case_snprintf_errno_name(EACCES);
        _case_snprintf_errno_name(ENOENT);
        _case_snprintf_errno_name(EINVAL);
        _case_snprintf_errno_name(EFAULT);
        _case_snprintf_errno_name(EROFS);
        _case_snprintf_errno_name(EIO);

        default:
            res = snprintf(buf, buflen, "%d", err);
            break;
    }
    if (res < 0) {
        return res;
    }
    len += res;
    return len;
}
#undef _case_snprintf_errno_name

int _vfs_handler(int argc, char **argv)
{
    if (argc < 3) {
        _vfs_usage(argv);
        return 1;
    }
    if (strcmp(argv[1], "r") != 0) {
        printf("Only read is currently supported");
        return 2;
    }
    uint8_t buf[16];
    size_t nbytes = sizeof(buf);
    off_t offset = 0;
    char *path = argv[2];
    if (argc > 3) {
        nbytes = atoi(argv[3]);
    }
    if (argc > 4) {
        offset = atoi(argv[4]);
    }

    vfs_normalize_path(path, path, strlen(path));

    int fd = vfs_open(path, O_RDONLY, 0);
    if (fd < 0) {
        _errno_string(fd, (char *)buf, sizeof(buf));
        printf("Error opening file \"%s\": %s\n", path, buf);
        return 3;
    }

    int res;
    res = vfs_lseek(fd, offset, SEEK_SET);
    if (res < 0) {
        _errno_string(res, (char *)buf, sizeof(buf));
        printf("Seek error: %s\n", buf);
        vfs_close(fd);
        return 4;
    }

    while (nbytes > 0) {
        size_t line_len = (nbytes < sizeof(buf) ? nbytes : sizeof(buf));
        res = vfs_read(fd, buf, line_len);
        if (res < 0) {
            _errno_string(res, (char *)buf, sizeof(buf));
            printf("Read error: %s\n", buf);
            vfs_close(fd);
            return 5;
        }
        else if (res > line_len) {
            printf("BUFFER OVERRUN! %d > %lu\n", res, (unsigned long)line_len);
            vfs_close(fd);
            return 6;
        }
        else if (res == 0) {
            /* EOF */
            printf("-- EOF --\n");
            break;
        }
        printf("%08lx:", (unsigned long)offset);
        for (size_t k = 0; k < res; ++k) {
            if ((k % 2) == 0) {
                putchar(' ');
            }
            printf("%02x", buf[k]);
        }
        putchar(' ');
        putchar(' ');
        for (size_t k = 0; k < res; ++k) {
            if (isprint(buf[k])) {
                putchar(buf[k]);
            }
            else {
                putchar('.');
            }
        }
        puts("");
        offset += res;
        nbytes -= res;
    }

    vfs_close(fd);
    return 0;
}

int _ls_handler(int argc, char **argv)
{
    if (argc < 2) {
        _ls_usage(argv);
        return 1;
    }
    vfs_DIR dir;
    uint8_t buf[16];
    int res = vfs_opendir(&dir, argv[1]);
    if (res < 0) {
        _errno_string(res, (char *)buf, sizeof(buf));
        printf("vfs_opendir error: %s\n", buf);
        return 1;
    }
    unsigned int nfiles = 0;

    while (1) {
        vfs_dirent_t entry;
        res = vfs_readdir(&dir, &entry);
        if (res < 0) {
            _errno_string(res, (char *)buf, sizeof(buf));
            printf("vfs_opendir error: %s\n", buf);
            if (res == -EAGAIN) {
                /* try again */
                continue;
            }
            return 1;
        }
        if (res == 0) {
            /* end of stream */
            break;
        }
        printf("%s\n", entry.d_name);
        ++nfiles;
    }
    printf("total %u files\n", nfiles);
    return 0;
}
#endif
