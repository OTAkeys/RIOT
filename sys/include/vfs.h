/*
 * Copyright (C) 2016 Eistec AB
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @defgroup  sys_vfs Virtual File System (VFS) layer
 * @ingroup   sys
 * @brief     Provides an interface for accessing files and directories from different devices and file systems
 *
 * This layer is modeled as a mix between POSIX syscalls (e.g. open) and the
 * Linux VFS layer implementation, with major reductions in the feature set, in
 * order to fit the resource constrained platforms that RIOT targets.
 *
 * @todo VFS layer reference counting for open files and simultaneous access.
 *
 * @{
 * @file
 * @brief   VFS layer API specifications
 * @author  Joakim Nohlgård <joakim.nohlgard@eistec.se>
 */

#ifndef VFS_H_
#define VFS_H_

#include <stdint.h>
#include <sys/stat.h> /* for struct stat */
#include <sys/types.h> /* for off_t etc. */

#include "kernel_types.h"
#include "atomic.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef VFS_MAX_OPEN_FILES
/**
 * @brief Maximum number of simultaneous open files
 */
#define VFS_MAX_OPEN_FILES (16)
#endif

#ifndef VFS_MAX_MOUNTS
/**
 * @brief Maximum number of simultaneous mounted file systems
 */
#define VFS_MAX_MOUNTS (4)
#endif

#ifndef VFS_MOUNT_POINT_LEN
/**
 * @brief Longest possible mount point name, including trailing null byte
 */
/* string       | required length
 * "/"          | 2
 * "/mnt"       | 5
 * "/home"      | 6
 * "/var/tmp"   | 9
 * "/mnt/cdrom" | 11
 */
#define VFS_MOUNT_POINT_LEN (8)
#endif

/**
 * @brief Used with vfs_bind to bind to any available fd number
 */
#define VFS_ANY_FD (-1)

/* Forward declarations */
struct vfs_file_ops;
/**
 * @brief struct @c vfs_file_ops typedef
 */
typedef struct vfs_file_ops vfs_file_ops_t;
struct vfs_file_system_ops;
/**
 * @brief struct @c vfs_file_system_ops typedef
 */
typedef struct vfs_file_system_ops vfs_file_system_ops_t;

/**
 * @brief A file system driver
 */
typedef struct {
    const vfs_file_ops_t *f_op;         /**< File operations table */
    const vfs_file_system_ops_t *fs_op; /**< File system operations table */
} vfs_file_system_t;

/**
 * @brief A mounted file system
 */
typedef struct {
    const vfs_file_system_t *fs;     /**< The file system driver for the mount point */
    char mount_point[VFS_MOUNT_POINT_LEN]; /**< Mount point, e.g. "/mnt/cdrom" */
    atomic_int_t open_files;   /**< Number of currently open files */
    void *private_data;        /**< File system driver private data, implementation defined */
} vfs_mount_t;

/**
 * @brief An open file
 *
 * Similar, but not equal, to struct file in Linux
 */
typedef struct {
    const vfs_file_ops_t *f_op;   /**< File operations table */
    vfs_mount_t *mp;        /**< Pointer to mount table entry */
    int flags;              /**< File flags */
    off_t pos;              /**< Current position in the file */
    kernel_pid_t pid;       /**< PID of the process that opened the file */
    void *private_data;     /**< File system driver private data, implementation defined */
} vfs_file_t;

/**
 * @brief Operations on open files
 *
 * Similar, but not equal, to struct file_operations in Linux
 */
struct vfs_file_ops {
    /**
     * @brief Close an open file
     *
     * This function must perform any necessary clean ups and flush any internal
     * buffers in the file system driver.
     *
     * If an error occurs, the file will still be considered closed by the VFS
     * layer. Therefore, the proper clean up must still be performed by the file
     * system driver before returning any error code.
     *
     * @note This implementation does not consider @c -EINTR a special return code,
     * the file is still considered closed.
     *
     * @param[in]  filp     pointer to open file
     *
     * @return 0 on success
     * @return <0 on error, the file is considered closed anyway
     */
    int (*close) (vfs_file_t *filp);

    /**
     * @brief Query/set options on an open file
     *
     * @param[in]  filp     pointer to open file
     * @param[in]  cmd      fcntl command, see man 3p fcntl
     * @param[in]  arg      argument to fcntl command, see man 3p fcntl
     *
     * @return 0 on success
     * @return <0 on error
     */
    int (*fcntl) (vfs_file_t *filp, int cmd, int arg);

    /**
     * @brief Get status of an open file
     *
     * @param[in]  filp     pointer to open file
     * @param[out] buf      pointer to stat struct to fill
     *
     * @return 0 on success
     * @return <0 on error
     */
    int (*fstat) (vfs_file_t *filp, struct stat *buf);

    /**
     * @brief Seek to position in file
     *
     * @p whence determines the function of the seek and should be set to one of
     * the following values:
     *
     *  - @c SEEK_SET: Seek to absolute offset @p off
     *  - @c SEEK_CUR: Seek to current location + @p off
     *  - @c SEEK_END: Seek to end of file + @p off
     *
     * @param[in]  filp     pointer to open file
     * @param[in]  off      seek offset
     * @param[in]  whence   determines the seek method, see detailed description
     *
     * @return the new seek location in the file on success
     * @return <0 on error
     */
    off_t (*lseek) (vfs_file_t *filp, off_t off, int whence);

    /**
     * @brief Attempt to open a file in the file system at rel_path
     *
     * A file system driver should perform the necessary checks for file
     * existence etc in this function.
     *
     * The VFS layer will initialize the contents of @p *filp so that
     * @c filp->f_op points to the mounted file system's @c vfs_file_ops_t.
     * @c filp->private_data will be initialized to NULL, @c filp->pos will be set to 0.
     *
     * @note @p name is an absolute path inside the file system, @p abs_path is
     * the path to the file in the VFS, example: @p abs_path = "/mnt/hd/foo/bar", @p name = "/foo/bar"
     *
     * @note @p name and @p abs_path may point to different locations within the
     * same const char array and the strings may overlap
     *
     * @param[in]  filp     pointer to open file
     * @param[in]  name     null-terminated name of the file to open, relative to the file system root, including a leading slash
     * @param[in]  flags    flags for opening, see man 2 open, man 3p open
     * @param[in]  mode     mode for creating a new file, see man 2 open, man 3p open
     * @param[in]  abs_path null-terminated name of the file to open, relative to the VFS root ("/")
     *
     * @return 0 on success
     * @return <0 on error
     */
    int (*open) (vfs_file_t *filp, const char *name, int flags, int mode, const char *abs_path);

    /**
     * @brief Read bytes from an open file
     *
     * @param[in]  filp     pointer to open file
     * @param[in]  dest     pointer to destination buffer
     * @param[in]  nbytes   maximum number of bytes to read
     *
     * @return number of bytes read on success
     * @return <0 on error
     */
    ssize_t (*read) (vfs_file_t *filp, void *dest, size_t nbytes);

    /**
     * @brief Write bytes to an open file
     *
     * @param[in]  filp     pointer to open file
     * @param[in]  src      pointer to source buffer
     * @param[in]  nbytes   maximum number of bytes to write
     *
     * @return number of bytes written on success
     * @return <0 on error
     */
    ssize_t (*write) (vfs_file_t *filp, const void *src, size_t nbytes);
};

/**
 * @brief Operations on open files
 *
 * Similar, but not equal, to struct file_operations in Linux
 */
struct vfs_file_system_ops {
    /**
     * @brief Perform any extra processing needed after mounting a file system
     *
     * If this call returns an error, the whole vfs_mount call will signal a
     * failure.
     *
     * All fields of @p mountp will be initialized by vfs_mount beforehand,
     * @c private_data will be initialized to NULL.
     *
     * @param[in]  mountp  The file system mount being mounted
     *
     * @return 0 on success
     * @return <0 on error
     */
    int (*mount) (vfs_mount_t *mountp);

    /**
     * @brief Perform the necessary clean up for unmounting a file system
     *
     * @param[in]  mountp  The file system mount being unmounted
     *
     * @return 0 on success
     * @return <0 on error
     */
    int (*umount) (vfs_mount_t *mountp);

    /**
     * @brief Unlink (delete) a file from the file system
     *
     * @param[in]  mountp  The file system mount to operate on
     * @param[in]  name    The name of the file to delete
     *
     * @return 0 on success
     * @return <0 on error
     */
    int (*unlink) (vfs_mount_t *mountp, const char *name);
};

/**
 * @brief Close an open file
 *
 * @param[in]  fd    fd number to close
 *
 * @return 0 on success
 * @return <0 on error
 */
int vfs_close(int fd);

/**
 * @brief Query/set options on an open file
 *
 * @param[in]  fd    fd number to operate on
 * @param[in]  cmd   fcntl command, see man 3p fcntl
 * @param[in]  arg   argument to fcntl command, see man 3p fcntl
 *
 * @return 0 on success
 * @return <0 on error
 */
int vfs_fcntl(int fd, int cmd, int arg);

/**
 * @brief Get status of an open file
 *
 * @param[in]  fd       fd number obtained from vfs_open
 * @param[out] buf      pointer to stat struct to fill
 *
 * @return 0 on success
 * @return <0 on error
 */
int vfs_fstat(int fd, struct stat *buf);

/**
 * @brief Seek to position in file
 *
 * @p whence determines the function of the seek and should be set to one of
 * the following values:
 *
 *  - @c SEEK_SET: Seek to absolute offset @p off
 *  - @c SEEK_CUR: Seek to current location + @p off
 *  - @c SEEK_END: Seek to end of file + @p off
 *
 * @param[in]  fd       fd number obtained from vfs_open
 * @param[in]  off      seek offset
 * @param[in]  whence   determines the seek method, see detailed description
 *
 * @return the new seek location in the file on success
 * @return <0 on error
 */
off_t vfs_lseek(int fd, off_t off, int whence);

/**
 * @brief Open a file
 *
 * @param[in]  name    file name to open
 * @param[in]  flags   flags for opening, see man 3p open
 * @param[in]  mode    file mode
 *
 * @return fd number on success (>= 0)
 * @return <0 on error
 */
int vfs_open(const char *name, int flags, mode_t mode);

/**
 * @brief Read bytes from an open file
 *
 * @param[in]  fd       fd number obtained from vfs_open
 * @param[out] buf      output buffer to put the file contents in
 * @param[in]  count    maximum number of bytes to read
 *
 * @return number of bytes read on success
 * @return <0 on error
 */
ssize_t vfs_read(int fd, void *dest, size_t count);

/**
 * @brief Write bytes to an open file
 *
 * @param[in]  fd       fd number obtained from vfs_open
 * @param[in]  src      pointer to source buffer
 * @param[in]  nbytes   maximum number of bytes to write
 *
 * @return number of bytes written on success
 * @return <0 on error
 */
ssize_t vfs_write(int fd, const void *src, size_t count);

/**
 * @brief Mount a file system
 *
 * @note fsp will only be shallow copied. Therefore, do not reuse the same
 * @p fsp for multiple mounts unless the file system driver has no state
 * associated with each instance
 *
 * @param[in]  fsp           pointer to file system driver instance
 * @param[in]  mount_point   absolute path to mount point
 * @param[in]  private_data  the private_data member of vfs_mount_t will be initialized to this
 *
 * @return md number on success (>= 0)
 * @return <0 on error
 */
int vfs_mount(const vfs_file_system_t *fsp, const char *mount_point, void *private_data);

/**
 * @brief Unmount a mounted file system
 *
 * This will fail if there are any open files on the mounted file system
 *
 * @param[in]  md    md number of the file system to unmount
 *
 * @return 0 on success
 * @return <0 on error
 */
int vfs_umount(int md);

/**
 * @brief Unlink (delete) a file from a mounted file system
 *
 * @param[in]  name   name of file to delete
 *
 * @return 0 on success
 * @return <0 on error
 */
int vfs_unlink(const char *name);

/**
 * @brief Allocate a new file descriptor and give it file operations
 *
 * The new fd will be initialized with pointers to the given @p f_op file
 * operations table and @p private_data.
 *
 * This function can be used to give file-like functionality to devices, e.g. UART.
 *
 * @p private_data can be used for passing instance information to the file
 * operation handlers in @p f_op.
 *
 * @param[in]  fd            Desired fd number, use VFS_ANY_FD for any available fd
 * @param[in]  flags         not implemented yet
 * @param[in]  f_op          pointer to file operations table
 * @param[in]  private_data  opaque pointer to private data
 *
 * @return fd number on success (>= 0)
 * @return <0 on error
 */
int vfs_bind(int fd, int flags, vfs_file_ops_t *f_op, void *private_data);

/**
 * @brief Normalize a path
 *
 * Normalizing a path means to remove all relative components ("..", ".") and
 * any double slashes.
 *
 * @note @p buf is allowed to overlap @p path if @p &buf[0] <= @p &path[0]
 *
 * @attention @p path must be an absolute path (starting with @c / )
 *
 * @param[out] buf        buffer to store normalized path
 * @param[in]  path       path to normalize
 * @param[in]  buflen     available space in @p buf
 *
 * @return number of path components in the normalized path on success
 * @return <0 on error
 */
int vfs_normalize_path(char *buf, const char *path, size_t buflen);

#ifdef __cplusplus
}
#endif

#endif

/** @} */
