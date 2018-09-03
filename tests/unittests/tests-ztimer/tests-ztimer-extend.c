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
 * @brief       Unittests for ztimer_extend
 *
 * @author      Joakim Nohlgård <joakim.nohlgard@eistec.se>
 */

#include "ztimer.h"
#include "ztimer/mock.h"
#include "ztimer/extend.h"

#include "embUnit/embUnit.h"

#include "tests-ztimer.h"

/**
 * @brief   Testing the counter of an 32 bit extended 8 bit counter
 */
static void test_ztimer_extend_now_rollover(void)
{
    ztimer_mock_t zmock;
    ztimer_extend_t zx;
    ztimer_dev_t *z = &zx.super;
    ztimer_dev_t *zm = &zmock.super;

    ztimer_mock_init(&zmock, 8);
    ztimer_extend_init(&zx, &zmock.super, 8);
    uint32_t now = ztimer_now(z);
    TEST_ASSERT_EQUAL_INT(0, now);
    ztimer_mock_advance(&zmock, 50);
    now = ztimer_now(z);
    TEST_ASSERT_EQUAL_INT(50, now);
    ztimer_mock_advance(&zmock, 50);
    now = ztimer_now(z);
    TEST_ASSERT_EQUAL_INT(100, now);
    ztimer_mock_advance(&zmock, 50);
    now = ztimer_now(zm);
    TEST_ASSERT_EQUAL_INT(150, now);
    now = ztimer_now(z);
    TEST_ASSERT_EQUAL_INT(150, now);
    ztimer_mock_advance(&zmock, 50);
    now = ztimer_now(zm);
    TEST_ASSERT_EQUAL_INT(200, now);
    now = ztimer_now(z);
    TEST_ASSERT_EQUAL_INT(200, now);
    ztimer_mock_advance(&zmock, 50);
    now = ztimer_now(z);
    TEST_ASSERT_EQUAL_INT(250, now);
    ztimer_mock_advance(&zmock, 50); /* -> rollover in lower clock */
    now = ztimer_now(zm);
    TEST_ASSERT_EQUAL_INT((300 - (1 << 8)), now);
    now = ztimer_now(z);
    TEST_ASSERT_EQUAL_INT(300, now);
    ztimer_mock_advance(&zmock, 50);
    now = ztimer_now(z);
    TEST_ASSERT_EQUAL_INT(350, now);
}

Test *tests_ztimer_extend_tests(void)
{
    EMB_UNIT_TESTFIXTURES(fixtures) {
        new_TestFixture(test_ztimer_extend_now_rollover),
    };

    EMB_UNIT_TESTCALLER(ztimer_tests, NULL, NULL, fixtures);

    return (Test *)&ztimer_tests;
}

/** @} */
