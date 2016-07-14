/*
 * Copyright (C) 2016 OTA keys S.A.
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

#include <stdint.h>
#include <fcntl.h>
#include <errno.h>

#include "fs/spiffs_fs.h"

#include "kernel_defines.h"

#define ENABLE_DEBUG (0)
#include <debug.h>

static int spiffs_err_to_errno(s32_t err);

#if SPIFFS_HAL_CALLBACK_EXTRA
static int32_t _dev_read(struct spiffs_t *fs, u32_t addr, u32_t size, u8_t *dst)
{
    mtdi_dev_t *dev = (mtdi_dev_t *)fs->user_data;

    //DEBUG("spiffs: read: from addr 0x%x size 0x%x\n", addr, size);

    return dev->driver->mtdi_read(dev, dst, addr, size);
}

static int32_t _dev_write(struct spiffs_t *fs, u32_t addr, u32_t size, u8_t *src)
{
    mtdi_dev_t *dev = (mtdi_dev_t *)fs->user_data;

    DEBUG("spiffs: write: from addr 0x%x size 0x%x\n", addr, size);

    return dev->driver->mtdi_write(dev, src, addr, size);
}

static int32_t _dev_erase(struct spiffs_t *fs, u32_t addr, u32_t size)
{
    mtdi_dev_t *dev = (mtdi_dev_t *)fs->user_data;

    DEBUG("spiffs: erase: from addr 0x%x size 0x%x\n", addr, size);

    return dev->driver->mtdi_erase(dev, addr, size);
}
#else
#error "SPIFFS needs SPIFFS_HAL_CALLBACK_EXTRA"
#endif

void spiffs_lock(struct spiffs_t *fs)
{
    spiffs_desc_t *fs_desc = container_of(fs, spiffs_desc_t, fs);

    DEBUG("spiffs: lock: fs_desc 0x%p\n", (void*)fs_desc);
    mutex_lock(&fs_desc->lock);
}
void spiffs_unlock(struct spiffs_t *fs)
{
    spiffs_desc_t *fs_desc = container_of(fs, spiffs_desc_t, fs);

    DEBUG("spiffs: unlock: fs_desc 0x%p\n", (void*)fs_desc);
    mutex_unlock(&fs_desc->lock);
}

static int _mount(vfs_mount_t *mountp)
{
    spiffs_desc_t *fs_desc = mountp->private_data;
    mtdi_dev_t *dev = fs_desc->dev;
    fs_desc->fs.user_data = dev;

    DEBUG("spiffs: mount: private_data = 0x%p\n", mountp->private_data);

    unsigned long sect_size;
    unsigned long sect_count;
    unsigned long page_size;
    dev->driver->mtdi_ioctl(dev, MTDI_GET_SECTOR_SIZE, &sect_size);
    dev->driver->mtdi_ioctl(dev, MTDI_GET_SECTOR_COUNT, &sect_count);
    dev->driver->mtdi_ioctl(dev, MTDI_GET_PAGE_SIZE, &page_size);


    fs_desc->config.hal_read_f = _dev_read;
    fs_desc->config.hal_write_f = _dev_write;
    fs_desc->config.hal_erase_f = _dev_erase;
    fs_desc->config.phys_size = sect_size * sect_count;
    fs_desc->config.log_block_size = sect_size;
    fs_desc->config.log_page_size = page_size;
    fs_desc->config.phys_addr = 0;
    fs_desc->config.phys_erase_block = sect_size;

    if (dev->driver->mtdi_init) {
        dev->driver->mtdi_init(dev);
    }

    s32_t ret = SPIFFS_mount(&fs_desc->fs,
                               &fs_desc->config,
                               fs_desc->work,
                               fs_desc->fd_space,
                               SPIFFS_FS_FD_SPACE_SIZE,
                               fs_desc->cache,
                               SPIFFS_FS_CACHE_SIZE,
                               NULL);

    if (ret != 0) {
        DEBUG("spiffs: mount: ret %d\n", ret);
        switch (ret) {
        case SPIFFS_ERR_NOT_A_FS:
            DEBUG("spiffs: mount: formatting fs\n");
            ret = SPIFFS_format(&fs_desc->fs);
            DEBUG("spiffs: mount: format ret %d\n", ret);
            if (ret < 0) {
                return spiffs_err_to_errno(ret);
            }
            ret = SPIFFS_mount(&fs_desc->fs,
                               &fs_desc->config,
                               fs_desc->work,
                               fs_desc->fd_space,
                               SPIFFS_FS_FD_SPACE_SIZE,
                               fs_desc->cache,
                               SPIFFS_FS_CACHE_SIZE,
                               NULL);
            DEBUG("spiffs: mount: ret %d\n", ret);
            break;
        }
    }

    return spiffs_err_to_errno(ret);
}

static int _umount(vfs_mount_t *mountp)
{
    spiffs_desc_t *fs_desc = mountp->private_data;

    SPIFFS_unmount(&fs_desc->fs);

    return 0;
}

static int _unlink(vfs_mount_t *mountp, const char *name)
{
    spiffs_desc_t *fs_desc = mountp->private_data;

    return spiffs_err_to_errno(SPIFFS_remove(&fs_desc->fs, name));
}

static int _rename(vfs_mount_t *mountp, const char *from_path, const char *to_path)
{
    spiffs_desc_t *fs_desc = mountp->private_data;

    return spiffs_err_to_errno(SPIFFS_rename(&fs_desc->fs, from_path, to_path));
}

static int _open(vfs_file_t *filp, const char *name, int flags, mode_t mode, const char *abs_path)
{
    spiffs_desc_t *fs_desc = filp->mp->private_data;
    (void) abs_path;
    DEBUG("spiffs: open: private_data = 0x%p\n", filp->mp->private_data);

    spiffs_flags s_flags = SPIFFS_O_RDONLY;
    if (flags & O_APPEND) {
        s_flags |= SPIFFS_O_APPEND;
    }
    if (flags & O_TRUNC) {
        s_flags |= SPIFFS_O_TRUNC;
    }
    if (flags & O_CREAT) {
        s_flags |= SPIFFS_O_CREAT;
    }
    if (flags & O_WRONLY) {
        s_flags |= SPIFFS_O_WRONLY;
    }
    if (flags & O_RDWR) {
        s_flags |= SPIFFS_O_RDWR;
    }
#ifdef __O_DIRECT
    if (flags & __O_DIRECT) {
        s_flags |= SPIFFS_O_DIRECT;
    }
#endif
    if (flags & O_EXCL) {
        s_flags |= SPIFFS_O_EXCL;
    }

    DEBUG("spiffs: open: %s (abs_path: %s), flags: 0x%x, mode: %d\n", name, abs_path, s_flags, mode);

    s32_t ret = SPIFFS_open(&fs_desc->fs, name, s_flags, mode);
    if (ret >= 0) {
        filp->private_data.value = ret;
        return ret;
    }
    else {
        return spiffs_err_to_errno(ret);
    }
}

static int _close(vfs_file_t *filp)
{
    spiffs_desc_t *fs_desc = filp->mp->private_data;

    return spiffs_err_to_errno(SPIFFS_close(&fs_desc->fs, filp->private_data.value));
}

static ssize_t _write(vfs_file_t *filp, const void *src, size_t nbytes)
{
    spiffs_desc_t *fs_desc = filp->mp->private_data;

    return spiffs_err_to_errno(SPIFFS_write(&fs_desc->fs, filp->private_data.value, src, nbytes));
}

static ssize_t _read(vfs_file_t *filp, void *dest, size_t nbytes)
{
    spiffs_desc_t *fs_desc = filp->mp->private_data;

    return spiffs_err_to_errno(SPIFFS_read(&fs_desc->fs, filp->private_data.value, dest, nbytes));
}

static off_t _lseek(vfs_file_t *filp, off_t off, int whence)
{
    spiffs_desc_t *fs_desc = filp->mp->private_data;

    int s_whence = 0;
    if (whence == SEEK_SET) {
        s_whence = SPIFFS_SEEK_SET;
    }
    else if (whence == SEEK_CUR) {
        s_whence = SPIFFS_SEEK_CUR;
    }
    else if (whence == SEEK_END) {
        s_whence = SPIFFS_SEEK_END;
    }

    return spiffs_err_to_errno(SPIFFS_lseek(&fs_desc->fs, filp->private_data.value, off, s_whence));
}

static int _fstat(vfs_file_t *filp, struct stat *buf)
{
    spiffs_desc_t *fs_desc = filp->mp->private_data;
    spiffs_stat stat;
    s32_t ret;

    memset(buf, 0, sizeof(*buf));

    ret = SPIFFS_fstat(&fs_desc->fs, filp->private_data.value, &stat);

    if (ret < 0) {
        return ret;
    }
    //stat.name;
    buf->st_ino = stat.obj_id;
    //stat.pix;
    buf->st_size = stat.size;
    //stat.type;
    buf->st_mode = S_IFREG;

    return spiffs_err_to_errno(ret);
}

static int _opendir(vfs_DIR *dirp, const char *dirname, const char *abs_path)
{
    spiffs_desc_t *fs_desc = dirp->mp->private_data;
    spiffs_DIR d;
    (void) abs_path;

    if (VFS_DIR_BUFFER_SIZE < sizeof(spiffs_DIR)) {
        return -EOVERFLOW;
    }

    SPIFFS_opendir(&fs_desc->fs, dirname, &d);
    memcpy(dirp->private_data.buffer, &d, sizeof(d));

    return 0;
}

static int _readdir(vfs_DIR *dirp, vfs_dirent_t *entry)
{
    spiffs_DIR d;
    struct spiffs_dirent e;
    struct spiffs_dirent *ret;

    memcpy(&d, dirp->private_data.buffer, sizeof(d));

    ret = SPIFFS_readdir(&d, &e);
    memcpy(dirp->private_data.buffer, &d, sizeof(d));
    if (ret == NULL) {
        s32_t err = SPIFFS_errno(d.fs);
        if (err != SPIFFS_OK && err > SPIFFS_ERR_INTERNAL) {
            DEBUG("spiffs: readdir: err=%d\n", err);
            return -EIO;
        }
    }

    if (ret) {
        entry->d_ino = e.obj_id;
        strncpy(entry->d_name, (char*) e.name, VFS_NAME_MAX);
        return 1;
    }
    else {
        return 0;
    }
}

static int _closedir(vfs_DIR *dirp)
{
    spiffs_DIR d;

    memcpy(&d, dirp->private_data.buffer, sizeof(d));

    return spiffs_err_to_errno(SPIFFS_closedir(&d));
}

static int spiffs_err_to_errno (s32_t err)
{
    if (err >= 0) {
        return (int) err;
    }

    DEBUG("spiffs: error=%d\n", err);

    switch (err) {
    case SPIFFS_OK:
        return 0;
    case SPIFFS_ERR_NOT_MOUNTED:
        return -EINVAL;
    case SPIFFS_ERR_FULL:
        return -ENOSPC;
    case SPIFFS_ERR_NOT_FOUND:
        return -ENOENT;
    case SPIFFS_ERR_END_OF_OBJECT:
        return 0;
    case SPIFFS_ERR_DELETED:
        return -ENOENT;
    case SPIFFS_ERR_MOUNTED:
        return -EBUSY;
    case SPIFFS_ERR_ERASE_FAIL:
        return -EIO;
    case SPIFFS_ERR_MAGIC_NOT_POSSIBLE:
        return -ENOSPC;
    case SPIFFS_ERR_NO_DELETED_BLOCKS:
        return 0;
    case SPIFFS_ERR_FILE_EXISTS:
        return -EEXIST;
    case SPIFFS_ERR_NOT_A_FILE:
        return -ENOENT;
    case SPIFFS_ERR_RO_NOT_IMPL:
        return -EROFS;
    case SPIFFS_ERR_RO_ABORTED_OPERATION:
        return -SPIFFS_ERR_RO_ABORTED_OPERATION;
    case SPIFFS_ERR_PROBE_TOO_FEW_BLOCKS:
        return -ENOSPC;
    case SPIFFS_ERR_PROBE_NOT_A_FS:
        return -ENODEV;
    case SPIFFS_ERR_NAME_TOO_LONG:
        return -ENAMETOOLONG;
    case SPIFFS_ERR_NOT_FINALIZED:
        return -ENODEV;
    case SPIFFS_ERR_NOT_INDEX:
        return -ENODEV;
    case SPIFFS_ERR_OUT_OF_FILE_DESCS:
        return -ENFILE;
    case SPIFFS_ERR_FILE_CLOSED:
        return -ENOENT;
    case SPIFFS_ERR_FILE_DELETED:
        return -ENOENT;
    case SPIFFS_ERR_BAD_DESCRIPTOR:
        return -EBADF;
    case SPIFFS_ERR_IS_INDEX:
        return -ENOENT;
    case SPIFFS_ERR_IS_FREE:
        return -ENOENT;
    case SPIFFS_ERR_INDEX_SPAN_MISMATCH:
        return -EIO;
    case SPIFFS_ERR_DATA_SPAN_MISMATCH:
        return -EIO;
    case SPIFFS_ERR_INDEX_REF_FREE:
        return -EIO;
    case SPIFFS_ERR_INDEX_REF_LU:
        return -EIO;
    case SPIFFS_ERR_INDEX_REF_INVALID:
        return -EIO;
    case SPIFFS_ERR_INDEX_FREE:
        return -EIO;
    case SPIFFS_ERR_INDEX_LU:
        return -EIO;
    case SPIFFS_ERR_INDEX_INVALID:
        return -EIO;
    case SPIFFS_ERR_NOT_WRITABLE:
        return -EACCES;
    case SPIFFS_ERR_NOT_READABLE:
        return -EACCES;
    case SPIFFS_ERR_CONFLICTING_NAME:
        return -EEXIST;
    case SPIFFS_ERR_NOT_CONFIGURED:
        return -ENODEV;
    case SPIFFS_ERR_NOT_A_FS:
        return -ENODEV;
    }

    return (int) err;
}

static const vfs_file_system_ops_t spiffs_fs_ops = {
    .mount = _mount,
    .umount = _umount,
    .unlink = _unlink,
    .rename = _rename,
};

static const vfs_file_ops_t spiffs_file_ops = {
    .open = _open,
    .close = _close,
    .read = _read,
    .write = _write,
    .lseek = _lseek,
    .fstat = _fstat,
};

static const vfs_dir_ops_t spiffs_dir_ops = {
    .opendir = _opendir,
    .readdir = _readdir,
    .closedir = _closedir,
};

const vfs_file_system_t spiffs_file_system = {
    .fs_op = &spiffs_fs_ops,
    .f_op = &spiffs_file_ops,
    .d_op = &spiffs_dir_ops,
};
