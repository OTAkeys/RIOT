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
    TEST_ASSERT_EQUAL_STRING(&buf[0], &r_buf[0]);

    res = vfs_close(fd);
    TEST_ASSERT(res == 0);

    fd = vfs_open("/test.txt", O_RDONLY, 0);
    TEST_ASSERT(fd >= 0);

    res = vfs_read(fd, r_buf, sizeof(r_buf));
    TEST_ASSERT(res == sizeof(buf));
    TEST_ASSERT_EQUAL_STRING(&buf[0], &r_buf[0]);

    res = vfs_close(fd);
    TEST_ASSERT(res == 0);

    res = vfs_umount(mp);
    TEST_ASSERT(res >= 0);
}

static void tests_spiffs_write2(void)
{
    char buf[1024];
    /*char r_buf[sizeof(buf) + 2];*/

    for (int i = 0; i < 1024; i++) {
        buf[i] = 'A' + (i % 30);
    }

    int mp = vfs_mount(&spiffs_file_system, "/", &spiffs_desc);
    TEST_ASSERT(mp >= 0);

    int res;
    for (int j = 0; j < 20; j++) {
        int fd = vfs_open("/test.txt", O_CREAT | O_RDWR, 0);
        TEST_ASSERT(fd >= 0);

        for  (int i = 0; i < 1000; i++) {
            res = vfs_write(fd, buf, sizeof(buf));
            TEST_ASSERT_EQUAL_INT(sizeof(buf), res);
        }

        res = vfs_lseek(fd, 0, SEEK_SET);
        TEST_ASSERT(res == 0);

        res = vfs_close(fd);
        TEST_ASSERT(res == 0);

        res = vfs_unlink("/test.txt");
        TEST_ASSERT(res == 0);
    }

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

static void tests_spiffs_readdir(void)
{
    const char buf0[] = "TESTSTRING";
    const char buf1[] = "TESTTESTSTRING";
    const char buf2[] = "TESTSTRINGSTRING";

    int mp = vfs_mount(&spiffs_file_system, "/", &spiffs_desc);
    TEST_ASSERT(mp >= 0);

    int fd0 = vfs_open("/test0.txt", O_CREAT | O_RDWR, 0);
    TEST_ASSERT(fd0 >= 0);

    int fd1 = vfs_open("/test1.txt", O_CREAT | O_RDWR, 0);
    TEST_ASSERT(fd1 >= 0);

    int fd2 = vfs_open("/a/test2.txt", O_CREAT | O_RDWR, 0);
    TEST_ASSERT(fd2 >= 0);

    int res = vfs_write(fd0, buf0, sizeof(buf0));
    TEST_ASSERT(res == sizeof(buf0));

    res = vfs_write(fd1, buf1, sizeof(buf1));
    TEST_ASSERT(res == sizeof(buf1));

    res = vfs_write(fd2, buf2, sizeof(buf2));
    TEST_ASSERT(res == sizeof(buf2));

    res = vfs_close(fd0);
    TEST_ASSERT(res == 0);

    res = vfs_close(fd1);
    TEST_ASSERT(res == 0);

    res = vfs_close(fd2);
    TEST_ASSERT(res == 0);

    vfs_DIR dirp;
    res = vfs_opendir(&dirp, "/");

    vfs_dirent_t entry;
    res = vfs_readdir(&dirp, &entry);
    TEST_ASSERT(res == 1);
    TEST_ASSERT_EQUAL_STRING("test0.txt", &(entry.d_name[0]));

    res = vfs_readdir(&dirp, &entry);
    TEST_ASSERT(res == 1);
    TEST_ASSERT_EQUAL_STRING("test1.txt", &(entry.d_name[0]));

    res = vfs_readdir(&dirp, &entry);
    TEST_ASSERT(res == 1);
    TEST_ASSERT_EQUAL_STRING("a/test2.txt", &(entry.d_name[0]));

    res = vfs_readdir(&dirp, &entry);
    TEST_ASSERT(res == 0);

    res = vfs_closedir(&dirp);
    TEST_ASSERT(res == 0);

    res = vfs_unlink("/test0.txt");
    TEST_ASSERT(res == 0);

    res = vfs_unlink("/test1.txt");
    TEST_ASSERT(res == 0);

    res = vfs_unlink("/a/test2.txt");
    TEST_ASSERT(res == 0);

    res = vfs_umount(mp);
    TEST_ASSERT(res >= 0);
}

static void tests_spiffs_rename(void)
{
    const char buf[] = "TESTSTRING";
    char r_buf[2 * sizeof(buf)];

    int mp = vfs_mount(&spiffs_file_system, "/", &spiffs_desc);
    TEST_ASSERT(mp >= 0);

    int fd = vfs_open("/test.txt", O_CREAT | O_RDWR, 0);
    TEST_ASSERT(fd >= 0);

    int res = vfs_write(fd, buf, sizeof(buf));
    TEST_ASSERT(res == sizeof(buf));

    res = vfs_lseek(fd, 0, SEEK_SET);
    TEST_ASSERT(res == 0);

    res = vfs_read(fd, r_buf, sizeof(r_buf));
    TEST_ASSERT(res == sizeof(buf));
    TEST_ASSERT_EQUAL_STRING(&buf[0], &r_buf[0]);

    res = vfs_close(fd);
    TEST_ASSERT(res == 0);

    res = vfs_rename("/test.txt", "/test1.txt");
    TEST_ASSERT(res == 0);

    fd = vfs_open("/test.txt", O_RDONLY, 0);
    TEST_ASSERT(fd < 0);

    fd = vfs_open("/test1.txt", O_RDONLY, 0);
    TEST_ASSERT(fd >= 0);

    res = vfs_lseek(fd, 0, SEEK_SET);
    TEST_ASSERT(res == 0);

    res = vfs_read(fd, r_buf, sizeof(r_buf));
    TEST_ASSERT(res == sizeof(buf));
    TEST_ASSERT_EQUAL_STRING(&buf[0], &r_buf[0]);

    res = vfs_close(fd);
    TEST_ASSERT(res == 0);

    res = vfs_unlink("/test1.txt");
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
        new_TestFixture(tests_spiffs_readdir),
        new_TestFixture(tests_spiffs_rename),
        new_TestFixture(tests_spiffs_write2),
    };

    EMB_UNIT_TESTCALLER(spiffs_tests, NULL, NULL, fixtures);

    return (Test *)&spiffs_tests;
}

void tests_spiffs(void)
{
    TESTS_RUN(tests_spiffs_tests());
}
/** @} */
