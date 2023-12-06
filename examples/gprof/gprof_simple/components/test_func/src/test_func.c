/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "cJSON.h"

cJSON *a_json;
cJSON *b_json;

static void test_func_real_0(void)
{
    for (int i = 0; i < 10; i++) {
        cJSON_Compare(a_json, b_json, 0);
    }
}

static void test_func_real_1(void)
{
    for (int i = 0; i < 10; i++) {
        cJSON_Compare(a_json, b_json, 1);
    }
}

static void test_func_level_4(void)
{
    for (int i = 0; i < 10; i++) {
        test_func_real_0();
    }
}

static void test_func_level_3(void)
{
    for (int i = 0; i < 10; i++) {
        test_func_level_4();
        test_func_real_1();
    }
}

static void test_func_level_2(void)
{
    for (int i = 0; i < 10; i++) {
        test_func_level_3();
        test_func_real_1();
    }
}

static void test_func_level_1(void)
{
    for (int i = 0; i < 10; i++) {
        test_func_level_2();
        test_func_real_1();
    }
}

static void test_func_level_0(void)
{
    for (int i = 0; i < 10; i++) {
        test_func_level_1();
        test_func_real_1();
    }
}

void test_func(void)
{
    a_json = cJSON_Parse("0.5E-100");
    b_json = cJSON_Parse("0.5E-101");

    test_func_level_0();
}
