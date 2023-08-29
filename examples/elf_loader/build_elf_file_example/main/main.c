/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <unistd.h>

char buffer[] = "hello world %d\n";
static int s_cnt;

int try_test(void)
{
    for (int i = 0; i < 10; i++) {
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
