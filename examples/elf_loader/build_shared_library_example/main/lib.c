/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <unistd.h>

char buffer1[] = "hello world %d\n";
char buffer2[] = "fibonacci(%d) test\n";

static int s_cnt;

int try_test(void)
{
    for (int i = 0; i < 10; i++) {
        printf(buffer1, s_cnt++);
        sleep(1);
    }

    return 0;
}

int fibonacci(int n)
{
    printf(buffer2, n);
    if (n < 0) {
        return -1;
    }
    if (n <= 1) {
        return n;
    }

    int a = 0, b = 1, c;
    for (int i = 2; i <= n; i++) {
        c = a + b;
        a = b;
        b = c;
    }

    return b;
}
