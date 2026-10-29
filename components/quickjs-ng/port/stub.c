/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <sys/time.h>
#include <time.h>

#if defined(__GNUC__)
#define QJS_WEAK __attribute__((weak))
#else
#define QJS_WEAK
#endif

typedef void (*qjs_signal_handler_t)(int);

QJS_WEAK qjs_signal_handler_t signal(int signum, qjs_signal_handler_t handler)
{
    (void)signum;
    (void)handler;
    errno = ENOSYS;
    return SIG_ERR;
}

QJS_WEAK int nanosleep(const struct timespec *req, struct timespec *rem)
{
    (void)req;
    (void)rem;
    errno = ENOSYS;
    return -1;
}

QJS_WEAK int utimes(const char *path, const struct timeval times[2])
{
    (void)path;
    (void)times;
    errno = ENOSYS;
    return -1;
}

/* Dynamic loading stubs - return graceful errors instead of crashing.
 * QuickJS may probe for dlopen support; these stubs allow it to detect
 * absence of dynamic loading without terminating the firmware. */

static const char *const _qjs_dlerror_str = "Dynamic loading not supported on ESP";

QJS_WEAK void *dlopen(const char *filename, int mode)
{
    (void)filename;
    (void)mode;
    errno = ENOSYS;
    return NULL;
}

QJS_WEAK int dlclose(void *handle)
{
    (void)handle;
    errno = EINVAL;
    return -1;
}

QJS_WEAK char *dlerror(void)
{
    /* Return const string; caller should not modify/free. POSIX allows this
     * as implementation-defined behavior for non-thread-safe dlerror. */
    return (char *)_qjs_dlerror_str;
}

QJS_WEAK void *dlsym(void *handle, const char *symbol)
{
    (void)handle;
    (void)symbol;
    errno = ENOSYS;
    return NULL;
}

#undef QJS_WEAK
