/*
 * Copyright (C) 2016 OTA keys S.A.
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @{
 *
 * @file
 */
#include "fs/spiffs_fs.h"
#include "vfs.h"
#include "mtdi.h"
#include "native_mtdi.h"

#include <fcntl.h>

#include "embUnit/embUnit.h"

#include "tests-spiffs.h"

static mtdi_dev_t _dev = {
    .driver = &native_flash_driver,
};

static struct spiffs_desc spiffs_desc = {
    .lock = MUTEX_INIT,
    .dev = &_dev,
};

static void test_spiffs_mount_umount(void)
{
    int res = vfs_mount(&spiffs_file_system, "/", &spiffs_desc);
    TEST_ASSERT(res >= 0);

    res = vfs_umount(res);
    TEST_ASSERT(res >= 0);
}

static void tests_spiffs_open_close(void)
{
    int mp = vfs_mount(&spiffs_file_system, "/", &spiffs_desc);
    TEST_ASSERT(mp >= 0);

    int res = vfs_open("/test.txt", O_CREAT | O_RDWR, 0);
    TEST_ASSERT(res >= 0);

    res = vfs_close(res);
    TEST_ASSERT(res == 0);

    res = vfs_umount(mp);
    TEST_ASSERT(res >= 0);
}

static void tests_spiffs_write(void)
{
    const char buf[] = "TESTSTRING";
    char r_buf[2 * sizeof(buf)];

    int mp = vfs_mount(&spiffs_file_system, "/", &spiffs_desc);
    TEST_ASSERT(mp >= 0);

    int fd = vfs_open("/test.txt", O_CREAT | O_RDWR, 0);
    TEST_ASSERT(fd >= 0);

    int res = vfs_write(fd, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_INT(sizeof(buf), res);

    res = vfs_lseek(fd, 0, SEEK_SET);
    TEST_ASSERT(res == 0);

    res = vfs_read(fd, r_buf, sizeof(r_buf));
    TEST_ASSERT(res == sizeof(buf));
    //TEST_ASSERT_EQUAL_STRING(buf, r_buf);
    printf("read buf: %s\n", r_buf);

    res = vfs_close(fd);
    TEST_ASSERT(res == 0);

    res = vfs_umount(mp);
    TEST_ASSERT(res >= 0);
}

static void tests_spiffs_unlink(void)
{
    const char buf[] = "TESTSTRING";

    int mp = vfs_mount(&spiffs_file_system, "/", &spiffs_desc);
    TEST_ASSERT(mp >= 0);

    int fd = vfs_open("/test.txt", O_CREAT | O_RDWR, 0);
    TEST_ASSERT(fd >= 0);

    int res = vfs_write(fd, buf, sizeof(buf));
    TEST_ASSERT(res == sizeof(buf));

    res = vfs_close(fd);
    TEST_ASSERT(res == 0);

    res = vfs_unlink("/test.txt");
    TEST_ASSERT(res == 0);

    res = vfs_umount(mp);
    TEST_ASSERT(res >= 0);
}

Test *tests_spiffs_tests(void)
{
    EMB_UNIT_TESTFIXTURES(fixtures) {
        new_TestFixture(test_spiffs_mount_umount),
        new_TestFixture(tests_spiffs_open_close),
        new_TestFixture(tests_spiffs_write),
        new_TestFixture(tests_spiffs_unlink),
    };

    EMB_UNIT_TESTCALLER(spiffs_tests, NULL, NULL, fixtures);

    return (Test *)&spiffs_tests;
}

void tests_spiffs(void)
{
    TESTS_RUN(tests_spiffs_tests());
}
/** @} */
