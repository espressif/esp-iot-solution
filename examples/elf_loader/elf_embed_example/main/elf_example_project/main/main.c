/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <unistd.h>

#define ITERATIONS 10

static const char buffer[] = "hello world test %d\n";
static int s_cnt;

int try_test(void)
{
    for (int i = 0; i < ITERATIONS; i++) {
        printf(buffer, s_cnt++);
        sleep(1);
    }

    return 0;
}

int main(int argc, char *argv[])
{
    try_test();

    return 0;
}
