/*
 * Copyright (C) 2016 Eistec AB
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @{
 * @file
 * @brief   VFS layer implementation
 * @author  Joakim Nohlgård <joakim.nohlgard@eistec.se>
 */

/* TODO: Add locking/mutexes! */

#include <errno.h> /* for error codes */
#include <string.h> /* for strncmp */
#include <stddef.h> /* for NULL */
#include <sys/types.h> /* for off_t etc */
#include <fcntl.h> /* for O_ACCMODE, ..., fcntl */

#include "vfs.h"
#include "mutex.h"
#include "thread.h"
#include "kernel_types.h"

#define ENABLE_DEBUG (1)
#include "debug.h"
#if ENABLE_DEBUG
/* Since some of these functions are called by printf, we can't really call
 * printf from our functions or we end up in an infinite recursion. */
#include <unistd.h> /* for STDOUT_FILENO */
#define DEBUG_NOT_STDOUT(fd, ...) if (fd != STDOUT_FILENO) { DEBUG(__VA_ARGS__); }
#else
#define DEBUG_NOT_STDOUT(...)
#endif

/**
 * @internal
 * @brief Array of all currently open files
 *
 * This table maps POSIX fd numbers to vfs_file_t instances
 *
 * @attention STDIN, STDOUT, STDERR will use the three first items in this array.
 */
static vfs_file_t _vfs_open_files[VFS_MAX_OPEN_FILES];

/**
 * @internal
 * @brief Array of all currently mounted file systems
 *
 * This table is used to dispatch vfs_open calls to the appropriate file system
 * driver.
 */
static vfs_mount_t _vfs_mounts[VFS_MAX_MOUNTS];

/**
 * @internal
 * @brief Find an unused entry in the _vfs_open_files array and mark it as used
 *
 * If the @p fd argument is non-negative, the allocation fails if the
 * corresponding slot in the open files table is already occupied, no iteration
 * is done to find another free number in this case.
 *
 * If the @p fd argument is negative, the algorithm will iterate through the
 * open files table until it find an unused slot and return the number of that
 * slot.
 *
 * @param[in]  fd  Desired fd number, use VFS_ANY_FD for any free fd
 *
 * @return fd on success
 * @return <0 on error
 */
inline static int _allocate_fd(int fd);

/**
 * @internal
 * @brief Mark an allocated entry as unused in the _vfs_open_files array
 *
 * @param[in]  fd     fd to free
 */
inline static void _free_fd(int fd);

/**
 * @internal
 * @brief Initialize an entry in the _vfs_open_files array and mark it as used.
 *
 * @param[in]  fd           desired fd number, passed to _allocate_fd
 * @param[in]  f_op         pointer to file operations table
 * @param[in]  mountp       pointer to mount table entry, may be NULL
 * @param[in]  flags        file flags
 * @param[in]  private_data private_data initial value
 *
 * @return fd on success
 * @return <0 on error
 */
inline static int _init_fd(int fd, const vfs_file_ops_t *f_op, vfs_mount_t *mountp, int flags, void *private_data);

/**
 * @internal
 * @brief Find an unused entry in the _vfs_mounts array and mark it as used
 *
 * @p mount_point must be at most @c VFS_MOUNT_POINT_LEN-1 characters long
 *
 * @param[in]  mount_point  absolute path to mount point
 *
 * @return mount index on success
 * @return <0 on error
 */
inline static int _allocate_mount(const char *mount_point);

/**
 * @internal
 * @brief Mark an allocated entry as unused in the _vfs_mounts array
 *
 * @param[in]  md     md to free
 */
inline static void _free_mount(int md);

/**
 * @internal
 * @brief Find the file system associated with the file name @p name, and
 * increment the open_files counter
 *
 * A pointer to the mount point relative file name will be written to @p rel_path.
 *
 * @param[in]  name  absolute path to file
 * @param[out] rel_path  output pointer for relative path
 *
 * @return mount index on success
 * @return <0 on error
 */
inline static int _find_mount(const char *name, const char **rel_path);

/**
 * @internal
 * @brief Check that a given fd number is valid
 *
 * @param[in]  fd    fd to check
 *
 * @return 0 if the fd is valid
 * @return <0 if the fd is not valid
 */
inline static int _fd_is_valid(int fd);

static mutex_t _mount_mutex = MUTEX_INIT;
static mutex_t _open_mutex = MUTEX_INIT;

int vfs_close(int fd)
{
    DEBUG("vfs_close: %d\n", fd);
    int res = _fd_is_valid(fd);
    if (res < 0) {
        return res;
    }
    vfs_file_t *filp = &_vfs_open_files[fd];
    if (filp->f_op->close != NULL) {
        /* We will invalidate the fd regardless of the outcome of the file
         * system driver close() call below */
        res = filp->f_op->close(filp);
    }
    _free_fd(fd);
    return res;
}

int vfs_fcntl(int fd, int cmd, int arg)
{
    DEBUG("vfs_fcntl: %d, %d, %d\n", fd, cmd, arg);
    int res = _fd_is_valid(fd);
    if (res < 0) {
        return res;
    }
    vfs_file_t *filp = &_vfs_open_files[fd];
    /* The default fcntl implementation below only allows querying flags,
     * any other command requires insight into the file system driver */
    switch (cmd) {
        case F_GETFL:
            /* Get file flags */
            DEBUG("vfs_fcntl: GETFL: %d\n", filp->flags);
            return filp->flags;
        default:
            break;
    }
    /* pass on to file system driver */
    if (filp->f_op->fcntl != NULL) {
        return filp->f_op->fcntl(filp, cmd, arg);
    }
    return -EINVAL;
}

int vfs_fstat(int fd, struct stat *buf)
{
    DEBUG_NOT_STDOUT(fd, "vfs_fstat: %d, %p\n", fd, (void *)buf);
    if (buf == NULL) {
        return -EFAULT;
    }
    int res = _fd_is_valid(fd);
    if (res < 0) {
        return res;
    }
    vfs_file_t *filp = &_vfs_open_files[fd];
    if (filp->f_op->fstat == NULL) {
        /* driver does not implement fstat() */
        return -EINVAL;
    }
    if (filp->mp) {
        /* compute which md number the file belongs to, to use as a default for st_dev */
        buf->st_dev = (int)(filp->mp - &_vfs_mounts[0]);
    }
    return filp->f_op->fstat(filp, buf);
}


off_t vfs_lseek(int fd, off_t off, int whence)
{
    DEBUG("vfs_lseek: %d, %ld, %d\n", fd, (long)off, whence);
    int res = _fd_is_valid(fd);
    if (res < 0) {
        return res;
    }
    vfs_file_t *filp = &_vfs_open_files[fd];
    if (filp->f_op->lseek == NULL) {
        /* driver does not implement lseek() */
        /* default seek functionality is naive */
        switch (whence) {
            case SEEK_SET:
                break;
            case SEEK_CUR:
                off += filp->pos;
                break;
            case SEEK_END:
                /* we could use fstat here, but most file system drivers will
                 * likely already implement lseek in a more efficient fashion */
                return -EINVAL;
            default:
                return -EINVAL;
        }
        if (off < 0) {
            /* the resulting file offset would be negative */
            return -EINVAL;
        }
        filp->pos = off;

        return off;
    }
    return filp->f_op->lseek(filp, off, whence);
}


int vfs_open(const char *name, int flags, mode_t mode)
{
    DEBUG("vfs_open: \"%s\", 0x%x, %04lo\n", name, flags, (long unsigned int)mode);
    if (name == NULL) {
        return -EINVAL;
    }
    const char *rel_path;
    int md = _find_mount(name, &rel_path);
    /* _find_mount implicitly increments the open_files count on success */
    if (md < 0) {
        /* No mount point maps to the requested file name */
        DEBUG("vfs_open: no matching mount\n");
        return md;
    }
    vfs_mount_t *mountp = &_vfs_mounts[md];
    mutex_lock(&_open_mutex);
    int fd = _init_fd(VFS_ANY_FD, mountp->fs->f_op, mountp, flags, NULL);
    mutex_unlock(&_open_mutex);
    if (fd < 0) {
        DEBUG("vfs_open: _init_fd: ERR %d!\n", fd);
        /* remember to decrement the open_files count */
        atomic_dec(&mountp->open_files);
        return fd;
    }
    vfs_file_t *filp = &_vfs_open_files[fd];
    if (filp->f_op->open != NULL) {
        int res = filp->f_op->open(filp, rel_path, flags, mode, name);
        if (res < 0) {
            /* something went wrong during open */
            DEBUG("vfs_open: open: ERR %d!\n", res);
            /* clean up */
            _free_fd(fd);
            return res;
        }
    }
    DEBUG("vfs_open: opened %d\n", fd);
    return fd;
}

ssize_t vfs_read(int fd, void *dest, size_t count)
{
    DEBUG("vfs_read: %d, %p, %lu\n", fd, dest, (unsigned long)count);
    if (dest == NULL) {
        return -EFAULT;
    }
    int res = _fd_is_valid(fd);
    if (res < 0) {
        return res;
    }
    vfs_file_t *filp = &_vfs_open_files[fd];
    if (((filp->flags & O_ACCMODE) != O_RDONLY) & ((filp->flags & O_ACCMODE) != O_RDWR)) {
        /* File not open for reading */
        return -EBADF;
    }
    if (filp->f_op->read == NULL) {
        /* driver does not implement read() */
        return -EINVAL;
    }
    return filp->f_op->read(filp, dest, count);
}


ssize_t vfs_write(int fd, const void *src, size_t count)
{
    DEBUG_NOT_STDOUT(fd, "vfs_write: %d, %p, %lu\n", fd, src, (unsigned long)count);
    if (src == NULL) {
        return -EFAULT;
    }
    int res = _fd_is_valid(fd);
    if (res < 0) {
        return res;
    }
    vfs_file_t *filp = &_vfs_open_files[fd];
    if (((filp->flags & O_ACCMODE) != O_WRONLY) & ((filp->flags & O_ACCMODE) != O_RDWR)) {
        /* File not open for writing */
        return -EBADF;
    }
    if (filp->f_op->write == NULL) {
        /* driver does not implement write() */
        return -EINVAL;
    }
    return filp->f_op->write(filp, src, count);
}

int vfs_opendir(vfs_DIR *dirp, const char *dirname)
{
    DEBUG("vfs_opendir: %p, \"%s\"\n", (void *)dirp, dirname);
    if (dirp == NULL) {
        return -EINVAL;
    }
    if (dirname == NULL) {
        return -EINVAL;
    }
    const char *rel_path;
    int md = _find_mount(dirname, &rel_path);
    if (rel_path[0] == '\0') {
        /* if the trailing slash is missing we will get an empty string back, to
         * be consistent against the file system drivers we give the relative
         * path "/" instead */
        rel_path = "/";
    }
    /* _find_mount implicitly increments the open_files count on success */
    if (md < 0) {
        /* No mount point maps to the requested file name */
        DEBUG("vfs_open: no matching mount\n");
        return md;
    }
    vfs_mount_t *mountp = &_vfs_mounts[md];
    if (mountp->fs->d_op == NULL) {
        /* file system driver does not support directories */
        return -EINVAL;
    }
    /* initialize dirp */
    memset(dirp, 0, sizeof(*dirp));
    dirp->mp = mountp;
    dirp->d_op = mountp->fs->d_op;
    if (dirp->d_op->opendir != NULL) {
        int res = dirp->d_op->opendir(dirp, rel_path, dirname);
        if (res < 0) {
            /* remember to decrement the open_files count */
            atomic_dec(&mountp->open_files);
            return res;
        }
    }
    return 0;
}

int vfs_readdir(vfs_DIR *dirp, vfs_dirent_t *entry)
{
    DEBUG("vfs_readdir: %p, %p\n", (void *)dirp, (void *)entry);
    if (dirp == NULL) {
        return -EINVAL;
    }
    if (entry == NULL) {
        return -EINVAL;
    }
    int res = -EINVAL;
    if (dirp->d_op != NULL) {
        if (dirp->d_op->readdir != NULL) {
            res = dirp->d_op->readdir(dirp, entry);
        }
    }
    return res;
}

int vfs_closedir(vfs_DIR *dirp)
{
    DEBUG("vfs_closedir: %p\n", (void *)dirp);
    if (dirp == NULL) {
        return -EINVAL;
    }
    int res = 0;
    if (dirp->d_op != NULL) {
        if (dirp->d_op->closedir != NULL) {
            res = dirp->d_op->closedir(dirp);
        }
    }
    vfs_mount_t *mountp = dirp->mp;
    memset(dirp, 0, sizeof(*dirp));
    atomic_dec(&mountp->open_files);
    return res;
}

int vfs_mount(const vfs_file_system_t *fsp, const char *mount_point, void *private_data)
{
    DEBUG("vfs_mount: %p, \"%s\" (%p), %p\n", (void *)fsp, mount_point, (void *)mount_point, private_data);
    if (fsp == NULL) {
        return -EINVAL;
    }
    mutex_lock(&_mount_mutex);
    int md = _allocate_mount(mount_point);
    if (md < 0) {
        mutex_unlock(&_mount_mutex);
        return md;
    }
    vfs_mount_t *mountp = &_vfs_mounts[md];
    mountp->fs = fsp;
    mountp->private_data = private_data;
    if (mountp->fs->fs_op != NULL) {
        if (mountp->fs->fs_op->mount != NULL) {
            /* yes, a file system driver does not need to implement mount/umount */
            int res = mountp->fs->fs_op->mount(mountp);
            if (res < 0) {
                /* remember to free the allocated mount point */
                _free_mount(md);
                mutex_unlock(&_mount_mutex);
                return res;
            }
        }
    }
    mutex_unlock(&_mount_mutex);
    DEBUG("vfs_mount: mounted %d\n", md);
    return md;
}


int vfs_umount(int md)
{
    DEBUG("vfs_umount: %d\n", md);
    if ((unsigned int)md >= VFS_MAX_MOUNTS) {
        return -EBADF;
    }
    mutex_lock(&_mount_mutex);
    vfs_mount_t *mountp = &_vfs_mounts[md];
    if (mountp->mount_point[0] == '\0') {
        mutex_unlock(&_mount_mutex);
        return -EBADF;
    }
    DEBUG("vfs_umount: -> \"%s\" open=%d\n", mountp->mount_point, ATOMIC_VALUE(mountp->open_files));
    if (ATOMIC_VALUE(mountp->open_files) > 0) {
        mutex_unlock(&_mount_mutex);
        return -EBUSY;
    }
    if (mountp->fs->fs_op != NULL) {
        if (mountp->fs->fs_op->umount != NULL) {
            int res = mountp->fs->fs_op->umount(mountp);
            if (res < 0) {
                /* umount failed */
                mutex_unlock(&_mount_mutex);
                return res;
            }
        }
    }

    _free_mount(md);
    mutex_unlock(&_mount_mutex);
    return 0;
}

int vfs_rename(const char *from_path, const char *to_path)
{
    DEBUG("vfs_rename: \"%s\", \"%s\"\n", from_path, to_path);
    if ((from_path == NULL) || (to_path == NULL)) {
        return -EINVAL;
    }
    const char *rel_from;
    int md_from = _find_mount(from_path, &rel_from);
    /* _find_mount implicitly increments the open_files count on success */
    if (md_from < 0) {
        /* No mount point maps to the requested file name */
        DEBUG("vfs_rename: from: no matching mount\n");
        return md_from;
    }
    vfs_mount_t *mountp = &_vfs_mounts[md_from];
    if ((mountp->fs->fs_op == NULL) || (mountp->fs->fs_op->rename == NULL)) {
        /* rename not supported */
        DEBUG("vfs_rename: rename not supported by fs!\n");
        /* remember to decrement the open_files count */
        atomic_dec(&mountp->open_files);
        return -EPERM;
    }
    const char *rel_to;
    int md_to = _find_mount(to_path, &rel_to);
    /* _find_mount implicitly increments the open_files count on success */
    if (md_to < 0) {
        /* No mount point maps to the requested file name */
        DEBUG("vfs_rename: to: no matching mount\n");
        /* remember to decrement the open_files count */
        atomic_dec(&mountp->open_files);
        return md_to;
    }
    /* we decrement the open_files count once, since we're really only using one file */
    atomic_dec(&mountp->open_files);
    if (md_to != md_from) {
        /* The paths are on different file systems */
        DEBUG("vfs_rename: from_path and to_path are on different mounts\n");
        /* remember to decrement the open_files count */
        atomic_dec(&mountp->open_files);
        return -EXDEV;
    }
    int res = mountp->fs->fs_op->rename(mountp, rel_from, rel_to);
    DEBUG("vfs_rename: rename %d, \"%s\" -> \"%s\"", md_from, rel_from, rel_to);
    if (res < 0) {
        /* something went wrong during rename */
        DEBUG(": ERR %d!\n", res);
    }
    else {
        DEBUG("\n");
    }
    /* remember to decrement the open_files count */
    atomic_dec(&mountp->open_files);
    return res;
}

/* TODO: Share code between vfs_unlink, vfs_mkdir, vfs_rmdir since they are almost identical */

int vfs_unlink(const char *name)
{
    DEBUG("vfs_unlink: \"%s\"\n", name);
    if (name == NULL) {
        return -EINVAL;
    }
    const char *rel_path;
    int md = _find_mount(name, &rel_path);
    /* _find_mount implicitly increments the open_files count on success */
    if (md < 0) {
        /* No mount point maps to the requested file name */
        DEBUG("vfs_unlink: no matching mount\n");
        return md;
    }
    vfs_mount_t *mountp = &_vfs_mounts[md];
    if ((mountp->fs->fs_op == NULL) || (mountp->fs->fs_op->unlink == NULL)) {
        /* unlink not supported */
        DEBUG("vfs_unlink: unlink not supported by fs!\n");
        /* remember to decrement the open_files count */
        atomic_dec(&mountp->open_files);
        return -EPERM;
    }
    int res = mountp->fs->fs_op->unlink(mountp, rel_path);
    DEBUG("vfs_unlink: unlink %d, \"%s\"", md, rel_path);
    if (res < 0) {
        /* something went wrong during unlink */
        DEBUG(": ERR %d!\n", res);
    }
    else {
        DEBUG("\n");
    }
    /* remember to decrement the open_files count */
    atomic_dec(&mountp->open_files);
    return res;
}

int vfs_mkdir(const char *name, mode_t mode)
{
    DEBUG("vfs_mkdir: \"%s\", %04lo\n", name, (long unsigned int)mode);
    if (name == NULL) {
        return -EINVAL;
    }
    const char *rel_path;
    int md = _find_mount(name, &rel_path);
    /* _find_mount implicitly increments the open_files count on success */
    if (md < 0) {
        /* No mount point maps to the requested file name */
        DEBUG("vfs_mkdir: no matching mount\n");
        return md;
    }
    vfs_mount_t *mountp = &_vfs_mounts[md];
    if ((mountp->fs->fs_op == NULL) || (mountp->fs->fs_op->mkdir == NULL)) {
        /* mkdir not supported */
        DEBUG("vfs_mkdir: mkdir not supported by fs!\n");
        /* remember to decrement the open_files count */
        atomic_dec(&mountp->open_files);
        return -EPERM;
    }
    int res = mountp->fs->fs_op->mkdir(mountp, rel_path, mode);
    DEBUG("vfs_mkdir: mkdir %d, \"%s\"", md, rel_path);
    if (res < 0) {
        /* something went wrong during mkdir */
        DEBUG(": ERR %d!\n", res);
    }
    else {
        DEBUG("\n");
    }
    /* remember to decrement the open_files count */
    atomic_dec(&mountp->open_files);
    return res;
}

int vfs_rmdir(const char *name)
{
    DEBUG("vfs_rmdir: \"%s\"\n", name);
    if (name == NULL) {
        return -EINVAL;
    }
    const char *rel_path;
    int md = _find_mount(name, &rel_path);
    /* _find_mount implicitly increments the open_files count on success */
    if (md < 0) {
        /* No mount point maps to the requested file name */
        DEBUG("vfs_rmdir: no matching mount\n");
        return md;
    }
    vfs_mount_t *mountp = &_vfs_mounts[md];
    if ((mountp->fs->fs_op == NULL) || (mountp->fs->fs_op->rmdir == NULL)) {
        /* rmdir not supported */
        DEBUG("vfs_rmdir: rmdir not supported by fs!\n");
        /* remember to decrement the open_files count */
        atomic_dec(&mountp->open_files);
        return -EPERM;
    }
    int res = mountp->fs->fs_op->rmdir(mountp, rel_path);
    DEBUG("vfs_rmdir: rmdir %d, \"%s\"", md, rel_path);
    if (res < 0) {
        /* something went wrong during rmdir */
        DEBUG(": ERR %d!\n", res);
    }
    else {
        DEBUG("\n");
    }
    /* remember to decrement the open_files count */
    atomic_dec(&mountp->open_files);
    return res;
}

int vfs_bind(int fd, int flags, vfs_file_ops_t *f_op, void *private_data)
{
    DEBUG("vfs_bind: %d, %d, %p, %p\n", fd, flags, (void*)f_op, private_data);
    if (f_op == NULL) {
        return -EINVAL;
    }
    mutex_lock(&_open_mutex);
    fd = _init_fd(fd, f_op, NULL, flags, private_data);
    mutex_unlock(&_open_mutex);
    if (fd < 0) {
        DEBUG("vfs_bind: _init_fd: ERR %d!\n", fd);
        return fd;
    }
    DEBUG("vfs_bind: bound %d\n", fd);
    return fd;
}

int vfs_normalize_path(char *buf, const char *path, size_t buflen)
{
    DEBUG("vfs_normalize_path: %p, \"%s\" (%p), %lu\n", buf, path, path, (unsigned long)buflen);
    size_t len = 0;
    int npathcomp = 0;
    const char *path_end = path + strlen(path) + 1;
    if (len >= buflen) {
        return -ENAMETOOLONG;
    }

    while(path < path_end) {
        DEBUG("vfs_normalize_path: + %d \"%.*s\" <- \"%s\" (%p)\n", npathcomp, len, buf, path, path);
        if (path[0] == '\0') {
            break;
        }
        while (path[0] == '/') {
            /* skip extra slashes */
            ++path;
        }
        if (path[0] == '.') {
            ++path;
            if (path[0] == '/' || path[0] == '\0') {
                /* skip /./ components */
                DEBUG("vfs_normalize_path: skip .\n");
                continue;
            }
            if (path[0] == '.' && (path[1] == '/' || path[1] == '\0')) {
                DEBUG("vfs_normalize_path: reduce ../\n");
                if (len == 0) {
                    /* outside root */
                    return -EINVAL;
                }
                ++path;
                /* delete the last component of the path */
                while (len > 0 && buf[--len] != '/') {}
                --npathcomp;
                continue;
            }
        }
        buf[len++] = '/';
        if (len >= buflen) {
            return -ENAMETOOLONG;
        }
        if (path[0] == '\0') {
            /* trailing slash in original path, don't increment npathcomp */
            break;
        }
        ++npathcomp;
        /* copy the path component */
        while (len < buflen && path[0] != '/' && path[0] != '\0') {
            buf[len++] = path[0];
            ++path;
        }
        if (len >= buflen) {
            return -ENAMETOOLONG;
        }
    }
    /* special case for "/": (otherwise it will be zero) */
    if (len == 1) {
        npathcomp = 1;
    }
    buf[len] = '\0';
    DEBUG("vfs_normalize_path: = %d, \"%s\"\n", npathcomp, buf);
    return npathcomp;
}

inline static int _allocate_fd(int fd)
{
    if (fd < 0) {
        for (fd = 0; fd < VFS_MAX_OPEN_FILES; ++fd) {
            if (_vfs_open_files[fd].pid == KERNEL_PID_UNDEF) {
                break;
            }
        }
        if (fd >= VFS_MAX_OPEN_FILES) {
            /* The _vfs_open_files array is full */
            return -ENFILE;
        }
    }
    else if (_vfs_open_files[fd].pid != KERNEL_PID_UNDEF) {
        /* The desired fd is already in use */
        return -EEXIST;
    }
    kernel_pid_t pid = thread_getpid();
    if (pid == KERNEL_PID_UNDEF) {
        /* This happens when calling vfs_bind during boot, before threads have
         * been started. */
        pid = -1;
    }
    _vfs_open_files[fd].pid = pid;
    return fd;
}

inline static void _free_fd(int fd)
{
    DEBUG("_free_fd: %d, pid=%d\n", fd, _vfs_open_files[fd].pid);
    if (_vfs_open_files[fd].mp != NULL) {
        atomic_dec(&_vfs_open_files[fd].mp->open_files);
    }
    _vfs_open_files[fd].pid = KERNEL_PID_UNDEF;
}

inline static int _init_fd(int fd, const vfs_file_ops_t *f_op, vfs_mount_t *mountp, int flags, void *private_data)
{
    fd = _allocate_fd(fd);
    if (fd < 0) {
        return fd;
    }
    vfs_file_t *filp = &_vfs_open_files[fd];
    filp->mp = mountp;
    filp->f_op = f_op;
    filp->flags = flags;
    filp->pos = 0;
    filp->private_data.ptr = private_data;
    return fd;
}

inline static int _allocate_mount(const char *mount_point)
{
    for (int md = 0; md < VFS_MAX_OPEN_FILES; ++md) {
        if (_vfs_mounts[md].mount_point[0] == '\0') {
            vfs_mount_t *mountp = &_vfs_mounts[md];
            int res = vfs_normalize_path(mountp->mount_point, mount_point, VFS_MOUNT_POINT_LEN);
            if (res < 0) {
                /* something is wrong with the given mount point string */
                return res;
            }
            int len = strnlen(mountp->mount_point, VFS_MOUNT_POINT_LEN);
            if (len < 1) {
                /* something is wrong with the given mount point string */
                _free_mount(md);
                return -EINVAL;
            }
            if ((len > 1) && (mountp->mount_point[len - 1] == '/')) {
                /* remove trailing slash */
                mountp->mount_point[len - 1] = '\0';
            }
            ATOMIC_VALUE(_vfs_mounts[md].open_files) = 0;
            _vfs_mounts[md].private_data = NULL;
            _vfs_mounts[md].fs = NULL;
            return md;
        }
    }
    /* The _vfs_mounts array is full */
    return -ENOMEM;
}

inline static void _free_mount(int md)
{
    _vfs_mounts[md].mount_point[0] = '\0';
}

inline static int _find_mount(const char *name, const char **rel_path)
{
    size_t longest_match = 0;
    size_t name_len = strnlen(name, VFS_MOUNT_POINT_LEN - 1);
    int md;
    mutex_lock(&_mount_mutex);
    /* naive search for the longest matching prefix */
    for (int i = 0; i < VFS_MAX_MOUNTS; ++i) {
        size_t len = strnlen(_vfs_mounts[i].mount_point, VFS_MOUNT_POINT_LEN - 1);
        if (len < longest_match) {
            /* Already found a longer prefix */
            continue;
        }
        else if (len > name_len) {
            /* path name is shorter than the mount point name */
            continue;
        }
        else if ((len > 1) && (name[len] != '/') && (name[len] != '\0')) {
            /* name does not have a directory separator where mount point name ends */
            continue;
        }
        /* test whether mount_point is a prefix of name */
        if (strncmp(name, _vfs_mounts[i].mount_point, len) == 0) {
            longest_match = len;
            md = i;
        }
    }
    if (longest_match == 0) {
        /* not found */
        mutex_unlock(&_mount_mutex);
        return -ENOENT;
    }
    /* Increment open files counter for this mount */
    atomic_inc(&_vfs_mounts[md].open_files);
    mutex_unlock(&_mount_mutex);
    if (rel_path != NULL) {
        *rel_path = name + longest_match;
    }
    return md;
}

inline static int _fd_is_valid(int fd)
{
    if ((unsigned int)fd >= VFS_MAX_OPEN_FILES) {
        return -EBADF;
    }
    vfs_file_t *filp = &_vfs_open_files[fd];
    if (filp->pid == KERNEL_PID_UNDEF) {
        return -EBADF;
    }
    if (filp->f_op == NULL) {
        return -EBADF;
    }
    return 0;
}

/** @} */
