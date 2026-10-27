/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* A freestanding ELF C++ smoke test. Project ELF linking deliberately uses
 * -nostdlib, so this module carries its own DSO finalizer bookkeeping. The
 * host still needs to export other C++ runtime symbols if its module uses any.
 */
#include <stdint.h>

extern "C" {
    /* Host IDF C++ runtime defines this too; let the host own its strong symbol. */
    __attribute__((weak)) void *__dso_handle = &__dso_handle;
}

struct Finalizer {
    void (*function)(void *);
    void *argument;
};
static Finalizer finalizers[4];
static unsigned finalizer_count;

extern "C" int __cxa_atexit(void (*function)(void *), void *argument, void *dso)
{
    (void)dso;
    if (finalizer_count >= sizeof(finalizers) / sizeof(finalizers[0])) {
        return -1;
    }
    finalizers[finalizer_count++] = {function, argument};
    return 0;
}

static volatile int startup_stage;
static volatile int *test_observer;
static volatile unsigned startup_count;

__attribute__((constructor(101))) static void early_startup(void)
{
    unsigned previous_count = startup_count;
    startup_count = previous_count + 1;
    startup_stage = (previous_count == 0) ? 1 : -2;
}

struct StaticObject {
    StaticObject()
    {
        startup_stage = (startup_stage == 1) ? 42 : -1;
    }
    ~StaticObject()
    {
        startup_stage = 99;
        if (test_observer) {
            *test_observer = startup_stage;
        }
    }
};

static StaticObject global_object;

__attribute__((destructor)) static void late_shutdown(void)
{
    while (finalizer_count) {
        Finalizer entry = finalizers[--finalizer_count];
        entry.function(entry.argument);
    }
}

/* For the on-chip test, argv[0] points to a host-owned observer. This keeps
 * the fixture self-contained and makes post-unload destructor execution
 * observable without importing additional host C++ runtime functions. */
extern "C" int main(int argc, char *argv[])
{
    if (argc == 1 && argv && argv[0]) {
        test_observer = reinterpret_cast<volatile int *>(argv[0]);
        *test_observer = startup_stage;
    }
    return startup_stage == 42 ? 0 : -1;
}
