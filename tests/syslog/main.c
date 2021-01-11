/*
 * Copyright (C) 2020 Continental
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup tests
 * @{
 *
 * @file
 * @brief Syslog test
 *
 * @author      Vincent Dupont <vincent.dupont@continental-its.com>
 *
 * @}
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "syslog.h"
#include "msg.h"
#include "shell.h"

#if CONFIG_SYSLOG_HAS_OPEN
static char app_name[16];
static syslog_entry_t *entry;

static int _open_handler(int argc, char **argv)
{
    if (argc != 3) {
        printf("Usage: %s <ident> <facility>\n", argv[0]);
        return 1;
    }
    strncpy(app_name, argv[1], sizeof(app_name));
    entry = openlog(app_name, 0, atoi(argv[2]));

    return 0;
}

static int _close_handler(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    closelog(entry);

    return 0;
}

static int _log_handler(int argc, char **argv)
{
    if (argc != 3) {
        printf("Usage: %s <level> <string>\n", argv[0]);
        return 1;
    }
    syslog(entry, atoi(argv[1]), "%s", argv[2]);

    return 0;
}
#endif /* CONFIG_SYSLOG_HAS_OPEN */

static int _logd_handler(int argc, char **argv)
{
    if (argc != 3) {
        printf("Usage: %s <level> <string>\n", argv[0]);
        return 1;
    }
    syslog(syslog_default_entry, atoi(argv[1]), "%s", argv[2]);

    return 0;
}

static const shell_command_t cmd[] = {
#if CONFIG_SYSLOG_HAS_OPEN
    {"open", "open a syslog entry", _open_handler},
    {"log", "send a log to syslog", _log_handler},
    {"close", "close a syslog entry", _close_handler},
#endif
    {"logd", "send a log to syslog (default entry)", _logd_handler},
    {NULL, NULL, NULL}
};

int main(void)
{
    syslog_init();
    syslog_set_hostname("hostname");

    char line_buf[SHELL_DEFAULT_BUFSIZE];
    shell_run(cmd, line_buf, sizeof(line_buf));

    return 0;
}