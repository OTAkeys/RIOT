/*
 * Copyright (C) 2018 Eistec AB
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @{
 *
 * @file
 * @brief       Unittests for ztimer
 *
 * @author      Joakim Nohlgård <joakim.nohlgard@eistec.se>
 */

#include "ztimer.h"
#include "ztimer/mock.h"

#include "embUnit/embUnit.h"

#include "tests-ztimer.h"

/**
 * @brief   Simple callback for counting alarms
 */
static void cb_incr(void *arg)
{
    uint32_t *ptr = arg;
    *ptr += 1;
}

/**
 * @brief   Testing 32 bit wide mock clock now functionality
 */
static void test_ztimer_mock_now32(void)
{
    ztimer_mock_t zmock;
    ztimer_dev_t *z = &zmock.super;

    /* Basic sanity test of the mock implementation */
    ztimer_mock_init(&zmock, 32);
    uint32_t now = ztimer_now(z);
    TEST_ASSERT_EQUAL_INT(0, now);

    ztimer_mock_advance(&zmock, 123);
    now = ztimer_now(z);
    TEST_ASSERT_EQUAL_INT(123, now);

    ztimer_mock_jump(&zmock, 0x10000000ul);
    now = ztimer_now(z);
    TEST_ASSERT_EQUAL_INT(0x10000000ul, now);

    ztimer_mock_advance(&zmock, 0x98765432ul);
    now = ztimer_now(z);
    TEST_ASSERT_EQUAL_INT(0xa8765432ul, now);
    ztimer_mock_advance(&zmock, 0x41234567ul);
    ztimer_mock_advance(&zmock, 0x40000000ul);
    now = ztimer_now(z);
    TEST_ASSERT_EQUAL_INT(0x29999999ul, now);
}

/**
 * @brief   Testing 16 bit wide mock clock now functionality
 */
static void test_ztimer_mock_now16(void)
{
    ztimer_mock_t zmock;
    ztimer_dev_t *z = &zmock.super;

    /* testing a 16 bit counter */
    ztimer_mock_init(&zmock, 16);
    uint32_t now = ztimer_now(z);
    TEST_ASSERT_EQUAL_INT(0, now);

    ztimer_mock_advance(&zmock, 123);
    now = ztimer_now(z);
    TEST_ASSERT_EQUAL_INT(123, now);

    ztimer_mock_advance(&zmock, 30000ul);
    now = ztimer_now(z);
    TEST_ASSERT_EQUAL_INT(30123ul, now);

    ztimer_mock_advance(&zmock, 0x10000ul);
    now = ztimer_now(z);
    TEST_ASSERT_EQUAL_INT(30123ul, now);
    ztimer_mock_advance(&zmock, 0x8000ul);
    now = ztimer_now(z);
    TEST_ASSERT_EQUAL_INT(62891ul, now);
}

/**
 * @brief   Testing 8 bit wide mock clock now functionality
 */
static void test_ztimer_mock_now8(void)
{
    ztimer_mock_t zmock;
    ztimer_dev_t *z = &zmock.super;

    /* testing a small counter */
    ztimer_mock_init(&zmock, 8);
    uint32_t now = ztimer_now(z);
    TEST_ASSERT_EQUAL_INT(0, now);

    ztimer_mock_advance(&zmock, 123);
    now = ztimer_now(z);
    TEST_ASSERT_EQUAL_INT(123, now);

    ztimer_mock_advance(&zmock, 0x100);
    now = ztimer_now(z);
    TEST_ASSERT_EQUAL_INT(123, now);

    ztimer_mock_advance(&zmock, 180);
    now = ztimer_now(z);
    TEST_ASSERT_EQUAL_INT(47, now);
}

/**
 * @brief   Testing 3 bit wide mock clock now functionality
 */
static void test_ztimer_mock_now3(void)
{
    ztimer_mock_t zmock;
    ztimer_dev_t *z = &zmock.super;

    /* testing a tiny counter */
    ztimer_mock_init(&zmock, 3);
    uint32_t now = ztimer_now(z);
    TEST_ASSERT_EQUAL_INT(0, now);

    ztimer_mock_advance(&zmock, 7);
    now = ztimer_now(z);
    TEST_ASSERT_EQUAL_INT(7, now);

    ztimer_mock_advance(&zmock, 8);
    now = ztimer_now(z);
    TEST_ASSERT_EQUAL_INT(7, now);

    ztimer_mock_advance(&zmock, 10);
    now = ztimer_now(z);
    TEST_ASSERT_EQUAL_INT(1, now);
}

/**
 * @brief   Testing 32 bit wide mock clock set functionality
 */
static void test_ztimer_mock_set32(void)
{
    ztimer_mock_t zmock;
    ztimer_dev_t *z = &zmock.super;

    ztimer_mock_init(&zmock, 32);
    uint32_t now = ztimer_now(z);
    TEST_ASSERT_EQUAL_INT(0, now);

    uint32_t count = 0;
    ztimer_t alarm = { .callback = cb_incr, .arg = &count, };
    ztimer_set(z, &alarm, 1000);

    ztimer_mock_advance(&zmock,    1);    /* now =    1*/
    TEST_ASSERT_EQUAL_INT(0, count);
    ztimer_mock_advance(&zmock,  100);    /* now =  101 */
    TEST_ASSERT_EQUAL_INT(0, count);
    ztimer_mock_advance(&zmock,  898);    /* now =  999 */
    now = ztimer_now(z);
    TEST_ASSERT_EQUAL_INT(999, now);
    TEST_ASSERT_EQUAL_INT(0, count);
    ztimer_mock_advance(&zmock,    1);    /* now = 1000*/
    TEST_ASSERT_EQUAL_INT(1, count);
    ztimer_mock_advance(&zmock,    1);    /* now = 1001*/
    TEST_ASSERT_EQUAL_INT(1, count);
    ztimer_mock_advance(&zmock, 1000);    /* now = 2001*/
    TEST_ASSERT_EQUAL_INT(1, count);
    ztimer_set(z, &alarm, 3);
    ztimer_mock_advance(&zmock,  999);    /* now = 3000*/
    TEST_ASSERT_EQUAL_INT(2, count);
    ztimer_set(z, &alarm, 4000001000ul);
    ztimer_mock_advance(&zmock, 1000);    /* now = 4000*/
    TEST_ASSERT_EQUAL_INT(2, count);
    ztimer_mock_advance(&zmock, 4000000000ul); /* now = 4000004000*/
    now = ztimer_now(z);
    TEST_ASSERT_EQUAL_INT(4000004000ul, now);
    TEST_ASSERT_EQUAL_INT(3, count);
    ztimer_set(z, &alarm, 15);
    ztimer_mock_advance(&zmock,  14);
    ztimer_remove(z, &alarm);
    ztimer_mock_advance(&zmock, 1000);
    TEST_ASSERT_EQUAL_INT(3, count);
}

/**
 * @brief   Testing 16 bit wide mock clock set functionality
 */
static void test_ztimer_mock_set16(void)
{
    ztimer_mock_t zmock;
    ztimer_dev_t *z = &zmock.super;

    ztimer_mock_init(&zmock, 16);
    uint32_t now = ztimer_now(z);
    TEST_ASSERT_EQUAL_INT(0, now);

    uint32_t count = 0;
    ztimer_t alarm = { .callback = cb_incr, .arg = &count, };
    ztimer_set(z, &alarm, 1000);

    ztimer_mock_advance(&zmock,    1);       /* now =    1*/
    TEST_ASSERT_EQUAL_INT(0, count);
    ztimer_mock_advance(&zmock,  100);       /* now =  101 */
    TEST_ASSERT_EQUAL_INT(0, count);
    ztimer_mock_advance(&zmock,  898);       /* now =  999 */
    now = ztimer_now(z);
    TEST_ASSERT_EQUAL_INT(999, now);
    TEST_ASSERT_EQUAL_INT(0, count);
    ztimer_mock_advance(&zmock,    1);       /* now = 1000*/
    TEST_ASSERT_EQUAL_INT(1, count);
    ztimer_mock_advance(&zmock,    1);       /* now = 1001*/
    TEST_ASSERT_EQUAL_INT(1, count);
    ztimer_mock_advance(&zmock, 1000);       /* now = 2001*/
    TEST_ASSERT_EQUAL_INT(1, count);
    ztimer_set(z, &alarm, 0xfffful);
    ztimer_mock_advance(&zmock, 0x10000ul); /* now = 2001*/
    TEST_ASSERT_EQUAL_INT(2, count);
    ztimer_set(z, &alarm, 0xfffful);
    ztimer_mock_advance(&zmock, 0x10000000ul); /* now = 2001*/
    TEST_ASSERT_EQUAL_INT(3, count);
    ztimer_set(z, &alarm, 0x10001ul);
    ztimer_mock_advance(&zmock, 1);          /* now = 2002*/
    TEST_ASSERT_EQUAL_INT(4, count);
    now = ztimer_now(z);
    TEST_ASSERT_EQUAL_INT(2002, now);
}

Test *tests_ztimer_mock_tests(void)
{
    EMB_UNIT_TESTFIXTURES(fixtures) {
        new_TestFixture(test_ztimer_mock_now32),
        new_TestFixture(test_ztimer_mock_now16),
        new_TestFixture(test_ztimer_mock_now8),
        new_TestFixture(test_ztimer_mock_now3),
        new_TestFixture(test_ztimer_mock_set32),
        new_TestFixture(test_ztimer_mock_set16),
    };

    EMB_UNIT_TESTCALLER(ztimer_tests, NULL, NULL, fixtures);

    return (Test *)&ztimer_tests;
}

/** @} */
