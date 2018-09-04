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
 * @brief       Unittests for ztimer_convert
 *
 * @author      Joakim Nohlgård <joakim.nohlgard@eistec.se>
 */

#include "ztimer.h"
#include "ztimer/mock.h"
#include "ztimer/convert.h"

#include "embUnit/embUnit.h"

#include "tests-ztimer.h"

/**
 * @brief   Basic checks for ztimer_convert
 */
static void test_ztimer_convert_now(void)
{
    ztimer_mock_t zmock;
    ztimer_convert_t zc;
    ztimer_dev_t *z = &zc.super;

    ztimer_mock_init(&zmock, 32);
    ztimer_convert_init(&zc, &zmock.super, 123, 456);
    uint32_t now = ztimer_now(z);
    TEST_ASSERT_EQUAL_INT(0, now);
    ztimer_mock_advance(&zmock, 455);
    now = ztimer_now(z);
    TEST_ASSERT_EQUAL_INT(122, now);
    ztimer_mock_advance(&zmock, 1);
    now = ztimer_now(z);
    TEST_ASSERT_EQUAL_INT(123, now);
    ztimer_mock_advance(&zmock, 456000);
    now = ztimer_now(z);
    TEST_ASSERT_EQUAL_INT(123123, now);
}

Test *tests_ztimer_convert_tests(void)
{
    EMB_UNIT_TESTFIXTURES(fixtures) {
        new_TestFixture(test_ztimer_convert_now),
    };

    EMB_UNIT_TESTCALLER(ztimer_tests, NULL, NULL, fixtures);

    return (Test *)&ztimer_tests;
}

/** @} */
