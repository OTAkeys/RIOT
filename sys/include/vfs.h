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

#ifndef VFS_DIR_BUFFER_SIZE
/**
 * @brief Size of buffer space in vfs_DIR
 *
 * This space is needed to avoid dynamic memory allocations for some file
 * systems where a single pointer is not enough space for its directory stream
 * state, e.g. SPIFFS.
 *
 * Guidelines:
 *
 * SPIFFS requires a sizeof(spiffs_DIR) (6-16 bytes, depending on target
 * platform and configuration) buffer for its DIR struct.
 *
 * @attention File system developers: If your file system requires a buffer for
 * DIR streams that is larger than a single pointer or @c int variable, ensure
 * that you have a preprocessor check in your header file (so that it is
 * impossible to attempt to mount the file system without running into a
 * compiler error):
 *
 * @attention @code
 * #if VFS_DIR_BUFFER_SIZE < (sizeof(my_DIR))
 * #error VFS_DIR_BUFFER_SIZE is too small, at least 123 bytes is required
 * #endif
 * @endcode
 *
 * @attention Put the check in the public header file (.h), do not put the check in the
 * implementation (.c) file.
 */
#define VFS_DIR_BUFFER_SIZE (12)
#endif

#ifndef VFS_NAME_MAX
/**
 * @brief Maximum length of the name in a @c vfs_dirent_t
 *
 * Similar to the POSIX macro NAME_MAX
 */
#define VFS_NAME_MAX (32)
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
struct vfs_dir_ops;
/**
 * @brief struct @c vfs_dir_ops typedef
 */
typedef struct vfs_dir_ops vfs_dir_ops_t;
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
    const vfs_dir_ops_t *d_op;          /**< Directory operations table */
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
    const vfs_file_ops_t *f_op; /**< File operations table */
    vfs_mount_t *mp;            /**< Pointer to mount table entry */
    int flags;                  /**< File flags */
    off_t pos;                  /**< Current position in the file */
    kernel_pid_t pid;           /**< PID of the process that opened the file */
    union {
        void *ptr;              /**< pointer to private data */
        int value;              /**< alternatively, you can use private_data as an int */
    } private_data;             /**< File system driver private data, implementation defined */
} vfs_file_t;

/**
 * @brief A directory entry
 *
 * Used by opendir, readdir, closedir
 *
 * @attention This structure should be treated as an opaque blob and must not be
 * modified by user code. The contents should only be used by file system drivers.
 */
typedef struct {
    const vfs_dir_ops_t *d_op; /**< Directory operations table */
    vfs_mount_t *mp;           /**< Pointer to mount table entry */
    union {
        void *ptr;             /**< pointer to private data */
        int value;             /**< alternatively, you can use private_data as an int */
        uint8_t buffer[VFS_DIR_BUFFER_SIZE]; /**< Buffer space, in case a single pointer is not enough */
    } private_data;            /**< File system driver private data, implementation defined */
} vfs_DIR;

/**
 * @brief A directory entry
 *
 * Used to hold the output from readdir
 *
 * @note size, modification time, and other information is part of the file
 * status, not the directory entry.
 */
typedef struct {
    ino_t d_ino; /**< file serial number, unique for the file system ("inode" in Linux) */
    char  d_name[VFS_NAME_MAX]; /**< file name, relative to its containing directory */
} vfs_dirent_t;

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
     * @c filp->private_data.ptr will be initialized to NULL, @c filp->pos will
     * be set to 0.
     *
     * @note @p name is an absolute path inside the file system, @p abs_path is
     * the path to the file in the VFS, example:
     * @p abs_path = "/mnt/hd/foo/bar", @p name = "/foo/bar"
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
 * @brief Operations on open directories
 */
struct vfs_dir_ops {
    /**
     * @brief Open a directory for reading with readdir
     *
     * @param[in]  dirp     pointer to open directory
     * @param[in]  name     null-terminated name of the dir to open, relative to the file system root, including a leading slash
     * @param[in]  abs_path null-terminated name of the dir to open, relative to the VFS root ("/")
     *
     * @return 0 on success
     * @return <0 on error
     */
    int (*opendir) (vfs_DIR *dirp, const char *dirname, const char *abs_path);

    /**
     * @brief Read a single entry from the open directory dirp and advance the
     * read position by one
     *
     * @p entry will be populated with information about the next entry in the
     * directory stream @p dirp
     *
     * @param[in]  dirp     pointer to open directory
     * @param[out] entry    directory entry information
     *
     * @return 0 on success
     * @return <0 on error
     */
    int (*readdir) (vfs_DIR *dirp, vfs_dirent_t *entry);

    /**
     * @brief Close an open directory
     *
     * @param[in]  dirp     pointer to open directory
     *
     * @return 0 on success
     * @return <0 on error, the directory stream dirp should be considered invalid
     */
    int (*closedir) (vfs_DIR *dirp);
};

/**
 * @brief Operations on mounted file systems
 *
 * Similar, but not equal, to struct super_operations in Linux
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
     * @param[in]  mountp  file system mount being mounted
     *
     * @return 0 on success
     * @return <0 on error
     */
    int (*mount) (vfs_mount_t *mountp);

    /**
     * @brief Perform the necessary clean up for unmounting a file system
     *
     * @param[in]  mountp  file system mount being unmounted
     *
     * @return 0 on success
     * @return <0 on error
     */
    int (*umount) (vfs_mount_t *mountp);

    /**
     * @brief Unlink (delete) a file from the file system
     *
     * @param[in]  mountp  file system mount to operate on
     * @param[in]  name    name of the file to delete
     *
     * @return 0 on success
     * @return <0 on error
     */
    int (*unlink) (vfs_mount_t *mountp, const char *name);

    /**
     * @brief Create a directory on the file system
     *
     * @param[in]  mountp  file system mount to operate on
     * @param[in]  name    name of the directory to create
     * @param[in]  mode    file creation mode bits
     *
     * @return 0 on success
     * @return <0 on error
     */
    int (*mkdir) (vfs_mount_t *mountp, const char *name, mode_t mode);

    /**
     * @brief Remove a directory from the file system
     *
     * Only empty directories may be removed.
     *
     * @param[in]  mountp  file system mount to operate on
     * @param[in]  name    name of the directory to remove
     *
     * @return 0 on success
     * @return <0 on error
     */
    int (*rmdir) (vfs_mount_t *mountp, const char *name);
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
 * @brief Open a directory for reading with readdir
 *
 * The data in @c *dirp will be initialized by @c vfs_opendir
 *
 * @param[out] dirp     pointer to directory stream struct for storing the state
 * @param[in]  dirname  null-terminated name of the dir to open, absolute file system path
 *
 * @return 0 on success
 * @return <0 on error
 */
int vfs_opendir(vfs_DIR *dirp, const char *dirname);

/**
 * @brief Read a single entry from the open directory dirp and advance the
 * read position by one
 *
 * @p entry will be populated with information about the next entry in the
 * directory stream @p dirp
 *
 * @attention Calling vfs_readdir on an uninitialized @c vfs_DIR is forbidden
 * and may lead to file system corruption and random system failures.
 *
 * @param[in]  dirp     pointer to open directory
 * @param[out] entry    directory entry information
 *
 * @return 0 on success
 * @return <0 on error
 */
int vfs_readdir(vfs_DIR *dirp, vfs_dirent_t *entry);

/**
 * @brief Close an open directory
 *
 * @attention Calling vfs_closedir on an uninitialized @c vfs_DIR is forbidden
 * and may lead to file system corruption and random system failures.
 *
 * @param[in]  dirp     pointer to open directory
 *
 * @return 0 on success
 * @return <0 on error, the directory stream dirp should be considered invalid
 */
int vfs_closedir(vfs_DIR *dirp);

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
 * @brief Create a directory on the file system
 *
 * @param[in]  name    name of the directory to create
 * @param[in]  mode    file creation mode bits
 *
 * @return 0 on success
 * @return <0 on error
 */
int vfs_mkdir(const char *name, mode_t mode);

/**
 * @brief Remove a directory from the file system
 *
 * Only empty directories may be removed.
 *
 * @param[in]  name    name of the directory to remove
 *
 * @return 0 on success
 * @return <0 on error
 */
int vfs_rmdir(const char *name);

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
