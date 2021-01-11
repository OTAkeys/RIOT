
#include <fcntl.h>
#include <errno.h>
#include <string.h>

#include "syslog.h"
#include "backend.h"
#include "vfs.h"
#include "mutex.h"

#define ENABLE_DEBUG    (1)
#include "debug.h"

#ifndef CONFIG_SYSLOG_FILE_ENTRY_NAME
#define CONFIG_SYSLOG_FILE_ENTRY_NAME       "log"
#endif

#ifndef CONFIG_SYSLOG_BACKEND_FILE_PATH
#define CONFIG_SYSLOG_BACKEND_FILE_PATH     "/log/sys"
#endif

#ifndef CONFIG_SYSLOG_BACKEND_ROT_SIZE
#define CONFIG_SYSLOG_BACKEND_ROT_SIZE      (256 * 1024)
#endif

#ifndef CONFIG_SYSLOG_BACKEND_NB_FILES
#define CONFIG_SYSLOG_BACKEND_NB_FILES      (32)
#endif

#define PATH_STRING(str) (sizeof(str) == 2 && str[0] == '/' ? "" : str)

typedef struct {
    const char *path;
    const char *prefix;
    size_t rotation_size;
    unsigned max_nb_files;
} syslog_file_params_t;

typedef struct {
    const syslog_file_params_t *params;
    size_t cur_size;
    unsigned nb_files;
} syslog_file_t;

static const syslog_file_params_t params = {
    .path = CONFIG_SYSLOG_BACKEND_FILE_PATH,
    .prefix = CONFIG_SYSLOG_FILE_ENTRY_NAME,
    .rotation_size = CONFIG_SYSLOG_BACKEND_ROT_SIZE,
    .max_nb_files = CONFIG_SYSLOG_BACKEND_NB_FILES,
};

static syslog_file_t _desc = {
    .params = &params,
};

static mutex_t rotation_lock = MUTEX_INIT;
static unsigned suspend_rotation;

static bool _needs_rotation(syslog_file_t *desc)
{
    if (desc->cur_size >= desc->params->rotation_size) {
        return true;
    }

    return false;
}

static void _rotate(syslog_file_t *desc)
{
    mutex_lock(&rotation_lock);
    if (suspend_rotation) {
        mutex_unlock(&rotation_lock);
        return;
    }
    if (desc->nb_files >= desc->params->max_nb_files) {
        /* remove oldest file */
        char name[VFS_NAME_MAX + 1];
        int ret = snprintf(name, sizeof(name), "%s/%s.%u", PATH_STRING(_desc.params->path),
                        _desc.params->prefix, desc->nb_files - 1);
        if (ret <= VFS_NAME_MAX) {
            DEBUG("syslog_file: rotation, removing %s\n", name);
            vfs_unlink(name);
            desc->nb_files--;
        }
    }
    for (unsigned i = desc->nb_files; i > 0; i--) {
        char old_name[VFS_NAME_MAX + 1];
        char new_name[VFS_NAME_MAX + 1];
        snprintf(old_name, sizeof(old_name), "%s/%s.%u", PATH_STRING(_desc.params->path),
                        _desc.params->prefix, i - 1);
        snprintf(new_name, sizeof(new_name), "%s/%s.%u", PATH_STRING(_desc.params->path),
                        _desc.params->prefix, i);
        DEBUG("syslog_file: rotation, moving %s -> %s\n", old_name, new_name);
        vfs_rename(old_name, new_name);
    }
    desc->nb_files++;
    desc->cur_size = 0;
    mutex_unlock(&rotation_lock);
}

static int _init(void)
{
    return 0;
}

static void _send(struct syslog_msg *msg)
{
    if (!_desc.nb_files) {
        return;
    }
    char name[VFS_NAME_MAX + 1];
    int ret = snprintf(name, sizeof(name), "%s/%s.0", PATH_STRING(_desc.params->path),
                    _desc.params->prefix);
    if (ret > VFS_NAME_MAX) {
        return;
    }
    int fd = vfs_open(name, O_RDWR | O_CREAT | O_APPEND, 0);
    if (fd < 0) {
        DEBUG("syslog_backend_file: unable to open file %s\n", name);
        return;
    }

    syslog_msg_get(msg);
    vfs_write(fd, msg->msg, msg->len);
    if (msg->len && msg->msg[msg->len - 1] != '\n') {
        vfs_write(fd, "\n", 1);
    }
    _desc.cur_size += msg->len;
    syslog_msg_put(msg);

    vfs_close(fd);

    if (_needs_rotation(&_desc)) {
        DEBUG("syslog_file: rotating\n");
        _rotate(&_desc);
    }
}

const syslog_backend_t file_be = {
    .init = _init,
    .send = _send,
};

int syslog_backend_file_start(void)
{
    DEBUG("syslog_backend_file_start: path=%s, prefix=%s\n", _desc.params->path, _desc.params->prefix);
    vfs_DIR dir;
    int ret = vfs_opendir(&dir, _desc.params->path);
    if (ret < 0) {
        ret = vfs_mkdir(_desc.params->path, 0);
        if (!ret) {
            ret = vfs_opendir(&dir, _desc.params->path);
        }
        if (ret) {
            DEBUG("syslog_backend_file_start: unable to open %s\n", _desc.params->path);
            return ret;
        }
    }

    int try_count = 3;
    while (1) {
        vfs_dirent_t entry;
        ret = vfs_readdir(&dir, &entry);
        if (ret < 0) {
            if (ret == -EAGAIN) {
                /* try again */
                if (try_count > 0) {
                    try_count--;
                    continue;
                }
            }
            break;
        }

        if (ret == 0) {
            /* end of stream */
            break;
        }

        if (strncmp(&entry.d_name[1], _desc.params->prefix, strlen(_desc.params->prefix)) == 0) {
            _desc.nb_files++;
        }
    }

    ret = vfs_closedir(&dir);

    DEBUG("syslog_backend_file_start: found %d files\n", _desc.nb_files);

    if (!_desc.nb_files) {
        /* if no file exists yet, update nb_files to 1 so logging can start */
        _desc.nb_files++;
    }

    return ret;
}

void syslog_backend_file_suspend_rotation(void)
{
    mutex_lock(&rotation_lock);
    suspend_rotation++;
    mutex_unlock(&rotation_lock);
}

void syslog_backend_file_resume_rotation(void)
{
    assert(suspend_rotation);

    mutex_lock(&rotation_lock);
    suspend_rotation--;
    mutex_unlock(&rotation_lock);
}