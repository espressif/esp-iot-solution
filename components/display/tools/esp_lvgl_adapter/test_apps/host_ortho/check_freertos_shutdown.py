#!/usr/bin/env python3
"""Compile real LVGL shutdown functions against a deterministic FreeRTOS model.

Only temporary copies of LVGL are patched. The scheduler model reproduces the
ESP-IDF SMP list race: deleting a remote current task queues it for termination,
but its final self-suspend can move its TCB onto the suspended list before idle
reclaims it. A forced interleaving exercises that window without timing or sleeps.
This verifies the adapter patch and its stack-caps hook, not the FreeRTOS kernel.

Example from the repository:
    python3 check_freertos_shutdown.py --lvgl /path/to/lvgl
"""
from __future__ import annotations

import argparse
import hashlib
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import types


MODEL = r'''
#include <setjmp.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef int BaseType_t;
typedef int lv_result_t;
enum { pdFALSE = 0, pdTRUE = 1, LV_RESULT_OK = 0, LV_RESULT_INVALID = 1 };
enum list_state { READY, BLOCKED, SUSPENDED, TERMINATION, FREED };
typedef struct task {
    enum list_state list;
    size_t bytes;
    unsigned reclaimed;
} task;
typedef task * TaskHandle_t;
typedef struct {
    void (*pvStartRoutine)(void *);
    void *pTaskArg;
    TaskHandle_t xTaskHandle;
} lv_thread_t;

static task owner, target;
static TaskHandle_t current[configNUMBER_OF_CORES];
static lv_thread_t thread;
static int active_core, remote_core, eviction_after;
static int yields, core_queries, owner_suspends, self_suspends;
static int callback_calls, normal_deletes, caps_deletes, queued_deletes;
static int immediate_deletes, cases;
static bool callback_pending, in_wrapper;
static jmp_buf callback_exit;

#define CHECK(cond) do { if(!(cond)) { \
    fprintf(stderr, "FAIL at line %d: %s\n", __LINE__, #cond); exit(1); \
} } while(0)

static void prvRunThread(void *);
static void run_callback(void);
static void vTaskSuspend(TaskHandle_t);
static void vTaskDelete(TaskHandle_t);
static void vTaskDeleteWithCaps(TaskHandle_t);
static TaskHandle_t xTaskGetCurrentTaskHandle(void);
static TaskHandle_t xTaskGetCurrentTaskHandleForCore(BaseType_t);
static void model_yield(void);
#define taskYIELD() model_yield()

@DELETE@

@WRAPPER@

static bool running(TaskHandle_t handle)
{
    for(int i = 0; i < configNUMBER_OF_CORES; i++)
        if(current[i] == handle) return true;
    return false;
}

static void reclaim(TaskHandle_t handle)
{
    CHECK(handle->list != FREED);
    CHECK(handle->reclaimed == 0);
    handle->list = FREED;
    handle->reclaimed++;
    handle->bytes = 0;
}

static void idle_cleanup(void)
{
    if(target.list == TERMINATION && !running(&target)) reclaim(&target);
}

static TaskHandle_t xTaskGetCurrentTaskHandle(void)
{
    CHECK(active_core >= 0 && active_core < configNUMBER_OF_CORES);
    return current[active_core];
}

static TaskHandle_t xTaskGetCurrentTaskHandleForCore(BaseType_t core)
{
    CHECK(core >= 0 && core < configNUMBER_OF_CORES);
    core_queries++;
    return current[core];
}

static void vTaskSuspend(TaskHandle_t handle)
{
    bool self = handle == NULL;
    if(self) handle = xTaskGetCurrentTaskHandle();
    CHECK(handle != NULL && handle->list != FREED);
    /* FreeRTOS removes xStateListItem from its present list before inserting
     * it into xSuspendedTaskList. This can rescue a queued termination TCB. */
    handle->list = SUSPENDED;
    if(self) {
        self_suspends++;
        CHECK(in_wrapper);
        longjmp(callback_exit, 1);
    }
    CHECK(handle == &target);
    owner_suspends++;
    /* The remote CPU can remain in the callback until a later context switch.
     * Suspending a task therefore does not immediately clear its current slot. */
}

static void delete_task(TaskHandle_t handle, bool caps)
{
    if(handle == NULL) handle = xTaskGetCurrentTaskHandle();
    CHECK(handle != NULL && handle->list != FREED);
    if(caps) caps_deletes++;
    else normal_deletes++;
    if(running(handle)) {
        bool self = handle == xTaskGetCurrentTaskHandle();
        handle->list = TERMINATION;
        queued_deletes++;
        /* Forced race: the other CPU returns from its callback only after
         * vTaskDelete has queued this still-current TCB for idle cleanup. */
        if(!self && callback_pending) run_callback();
        if(self) {
            current[active_core] = NULL;
            CHECK(in_wrapper);
            longjmp(callback_exit, 1);
        }
    }
    else {
        immediate_deletes++;
        reclaim(handle);
    }
}

static void vTaskDelete(TaskHandle_t handle) { delete_task(handle, false); }
static void vTaskDeleteWithCaps(TaskHandle_t handle) { delete_task(handle, true); }

static void callback(void *arg)
{
    CHECK(arg == &target);
    callback_calls++;
}

static void run_callback(void)
{
    int previous_core = active_core;
    CHECK(!in_wrapper);
    CHECK(current[remote_core] == &target);
    active_core = remote_core;
    callback_pending = false;
    in_wrapper = true;
    if(setjmp(callback_exit) == 0) {
        prvRunThread(&thread);
        CHECK(false); /* A FreeRTOS task wrapper must never return. */
    }
    in_wrapper = false;
    active_core = previous_core;
}

static void model_yield(void)
{
    CHECK(active_core == 0);
    yields++;
    CHECK(yields < 100); /* A broken wait must fail instead of hanging. */
    if(callback_pending) run_callback();
    if(yields >= eviction_after) {
        for(int i = 1; i < configNUMBER_OF_CORES; i++)
            if(current[i] == &target) current[i] = NULL;
    }
}

static void reset(int running_core, int delay)
{
    owner = (task){ READY, 0, 0 };
    target = (task){ BLOCKED, 33128, 0 };
    memset(current, 0, sizeof(current));
    current[0] = &owner;
    active_core = 0;
    remote_core = running_core < 0 ? 0 : running_core;
    eviction_after = delay;
    yields = core_queries = owner_suspends = self_suspends = 0;
    callback_calls = normal_deletes = caps_deletes = queued_deletes = 0;
    immediate_deletes = 0;
    callback_pending = in_wrapper = false;
    thread = (lv_thread_t){ callback, &target, &target };
    if(running_core >= 0) {
        current[running_core] = &target;
        target.list = READY;
        callback_pending = true;
    }
}

static void require_reclaimed(void)
{
    idle_cleanup();
    if(target.list == SUSPENDED && target.bytes == 33128 &&
       target.reclaimed == 0 && queued_deletes > 0 && self_suspends > 0) {
        fprintf(stderr, "ORPHAN: 33128 bytes and 1 task remain on suspended list; "
                "termination list is empty\n");
        exit(1);
    }
    CHECK(target.list == FREED);
    CHECK(target.reclaimed == 1 && target.bytes == 0);
    CHECK(owner.reclaimed == 0);
}

static void require_delete_api(void)
{
#if defined(ESP_PLATFORM) && defined(LV_FREERTOS_TASK_STACK_ALLOC_CAPS)
    CHECK(caps_deletes == 1 && normal_deletes == 0);
#else
    CHECK(normal_deletes == 1 && caps_deletes == 0);
#endif
}

#if defined(ESP_PLATFORM) && defined(ESP_LVGL_ADAPTER_THREAD_OWNER_DELETE)
static void test_blocked_and_double_delete(void)
{
    reset(-1, 0);
    CHECK(lv_thread_delete(&thread) == LV_RESULT_OK);
    require_reclaimed();
    CHECK(thread.xTaskHandle == NULL);
    CHECK(owner_suspends == 1 && yields == 0);
    CHECK(immediate_deletes == 1 && queued_deletes == 0);
    require_delete_api();
    CHECK(lv_thread_delete(&thread) == LV_RESULT_INVALID);
    CHECK(owner_suspends == 1 && target.reclaimed == 1);
    require_delete_api();
    cases += 2;
}

static void test_invalid_handles(void)
{
    reset(-1, 0);
    thread.xTaskHandle = NULL;
    CHECK(lv_thread_delete(&thread) == LV_RESULT_INVALID);
    CHECK(thread.xTaskHandle == NULL);
    CHECK(owner_suspends == 0 && normal_deletes == 0 && caps_deletes == 0);
    CHECK(target.bytes == 33128 && owner.reclaimed == 0);
    thread.xTaskHandle = &owner;
    CHECK(lv_thread_delete(&thread) == LV_RESULT_INVALID);
    CHECK(thread.xTaskHandle == &owner);
    CHECK(owner_suspends == 0 && normal_deletes == 0 && caps_deletes == 0);
    CHECK(owner.list == READY && owner.reclaimed == 0);
    cases += 2;
}

static void test_completed_callback(void)
{
    int core = configNUMBER_OF_CORES > 1 ? 1 : 0;
    reset(core, 1);
    run_callback();
    CHECK(target.list == SUSPENDED && target.bytes == 33128);
    CHECK(callback_calls == 1 && self_suspends == 1);
    CHECK(normal_deletes == 0 && caps_deletes == 0);
    current[core] = NULL;
    current[0] = &owner;
    CHECK(lv_thread_delete(&thread) == LV_RESULT_OK);
    require_reclaimed();
    CHECK(thread.xTaskHandle == NULL);
    require_delete_api();
    cases++;
}

static void test_remote(int core, int delay)
{
    reset(core, delay);
    CHECK(lv_thread_delete(&thread) == LV_RESULT_OK);
    /* Let the remote CPU settle and idle run, even with the wait mutated out.
     * A suspended orphan remains allocated after arbitrary idle opportunities. */
    current[core] = NULL;
    for(int i = 0; i < 10; i++) idle_cleanup();
    require_reclaimed();
    CHECK(thread.xTaskHandle == NULL);
    CHECK(callback_calls == 1 && self_suspends == 1);
    CHECK(yields >= delay && core_queries > 0);
    CHECK(owner_suspends == 1 && queued_deletes == 0);
    CHECK(immediate_deletes == 1);
    require_delete_api();
    cases++;
}
#else
static void test_unguarded(void)
{
    reset(-1, 0);
    CHECK(lv_thread_delete(&thread) == LV_RESULT_OK);
    require_reclaimed();
    CHECK(thread.xTaskHandle == &target); /* Preserve upstream guard-off API. */
    CHECK(owner_suspends == 0 && yields == 0);
    require_delete_api();
    reset(configNUMBER_OF_CORES > 1 ? 1 : 0, 1);
    run_callback();
    for(int i = 0; i < configNUMBER_OF_CORES; i++)
        if(current[i] == &target) current[i] = NULL;
    current[0] = &owner;
    require_reclaimed();
    CHECK(callback_calls == 1 && self_suspends == 0);
    CHECK(normal_deletes == 1 && caps_deletes == 0);
    CHECK(queued_deletes == 1);
    cases += 2;
}
#endif

int main(int argc, char **argv)
{
#if defined(ESP_PLATFORM) && defined(ESP_LVGL_ADAPTER_THREAD_OWNER_DELETE)
#if configNUMBER_OF_CORES > 1
    if(argc == 2 && strcmp(argv[1], "--race-only") == 0) {
        test_remote(1, 1);
        return 0;
    }
#else
    (void)argc; (void)argv;
#endif
    test_blocked_and_double_delete();
    test_invalid_handles();
    test_completed_callback();
#if configNUMBER_OF_CORES > 1
    for(int core = 1; core < configNUMBER_OF_CORES; core++) {
        test_remote(core, 1);
        test_remote(core, 5);
    }
#endif
#else
    (void)argc; (void)argv;
    test_unguarded();
#endif
    printf("PASS: %d scheduler scenarios\n", cases);
    return 0;
}
'''


def extract_function(source: str, name: str) -> str:
    """Extract a function definition, skipping prototypes and comment braces."""
    match = re.search(
        rf'^(?:static void|lv_result_t) {name}\([^;]*?\)\s*\{{', source, re.M)
    if not match:
        raise AssertionError(f'Function definition missing: {name}')
    start = match.start()
    opening = source.index('{', start)
    token = re.compile(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|\{ | \}', re.S | re.X)
    depth = 0
    for item in token.finditer(source, opening):
        if item.group() == '{':
            depth += 1
        elif item.group() == '}':
            depth -= 1
            if depth == 0:
                return source[start:item.end()]
    raise AssertionError(f'Unterminated function: {name}')


def command(args: list[str], *, expect_orphan: bool = False) -> str:
    result = subprocess.run(args, text=True, capture_output=True, timeout=15)
    output = result.stdout + result.stderr
    if expect_orphan:
        if result.returncode == 0 or 'ORPHAN: 33128 bytes and 1 task' not in output:
            raise AssertionError(f'Regression was not detected: {args}\n{output}')
    elif result.returncode:
        raise AssertionError(f'Command failed ({result.returncode}): {args}\n{output}')
    return output.strip()


def load_patch(helper: Path):
    # Loading text avoids creating __pycache__ beside the production helper.
    module = types.ModuleType('shutdown_patch_under_test')
    exec(compile(helper.read_text(), str(helper), 'exec'), module.__dict__)
    return module.patch


def without_owner_guard(deletion: str) -> str:
    """Recover the original API call from an already-patched real function."""
    kept = []
    depth = 0
    for line in deletion.splitlines(keepends=True):
        if depth:
            if re.match(r'\s*#\s*(?:if|ifdef|ifndef)\b', line):
                depth += 1
            elif re.match(r'\s*#\s*endif\b', line):
                depth -= 1
        elif re.match(r'\s*#\s*if\b.*ESP_LVGL_ADAPTER_THREAD_OWNER_DELETE', line):
            depth = 1
        else:
            kept.append(line)
    if depth:
        raise AssertionError('Unbalanced owner-delete guard')
    return ''.join(kept)


def add_caps(source: str, cmake: str) -> str:
    """Use the actual production CMake stack-caps deletion hook verbatim."""
    match = re.search(r'set\(_lv_thread_delete_hook \[\[(.*?)\]\]\)', cmake, re.S)
    if not match:
        raise AssertionError('Production CMake stack-caps deletion hook missing')
    before = '    vTaskDelete(pxThread->xTaskHandle);'
    if source.count(before) != 1:
        raise AssertionError('Expected one upstream thread-delete API call')
    return source.replace(before, match.group(1), 1)


def run_suite(lvgl: Path, helper: Path, cmake_path: Path, compiler: str) -> int:
    source_path = lvgl if lvgl.is_file() else lvgl / 'src/osal/lv_freertos.c'
    original = source_path.read_text()
    cmake = cmake_path.read_text()
    patch = load_patch(helper)
    return_new = patch.__globals__['RETURN_NEW']
    return_old = patch.__globals__['RETURN_OLD']
    return_legacy = patch.__globals__['RETURN_LEGACY_PLATFORM']
    count = 0
    with tempfile.TemporaryDirectory(prefix='lvgl-shutdown-') as directory:
        temporary = Path(directory)
        for variant in ('plain', 'caps-before', 'caps-after', 'legacy-return'):
            file = temporary / f'{variant}-lv_freertos.c'
            before_patch = add_caps(original, cmake) if variant == 'caps-before' else original
            if variant == 'legacy-return':
                if return_new in before_patch:
                    before_patch = before_patch.replace(return_new, return_legacy, 1)
                elif return_old in before_patch:
                    before_patch = before_patch.replace(return_old, return_legacy, 1)
                elif return_legacy not in before_patch:
                    raise AssertionError('Cannot construct a recognized legacy callback-return hook')
            file.write_text(before_patch)
            if variant == 'legacy-return':
                linked_cache = temporary / 'linked-cache-lv_freertos.c'
                os.link(file, linked_cache)
            patch(file)
            patched = file.read_text()
            if variant == 'legacy-return':
                if linked_cache.read_text() != before_patch or file.stat().st_nlink != 1:
                    raise AssertionError('Legacy upgrade modified a hard-linked component cache')
            # A marker alone must not bless an unknown or incomplete guard.
            malformed = temporary / f'{variant}-partial-guard.c'
            malformed_source = patched.replace(
                return_new, return_new.replace('defined(ESP_LVGL_ADAPTER_THREAD_OWNER_DELETE)',
                                               'defined(UNKNOWN_OWNER_GUARD)'), 1)
            malformed.write_text(malformed_source)
            try:
                patch(malformed)
            except ValueError:
                if malformed.read_text() != malformed_source:
                    raise AssertionError('Rejected callback guard was modified')
            else:
                raise AssertionError('Unknown callback guard with a recognized marker was accepted')
            patch(file)
            if patched != file.read_text():
                raise AssertionError('Patch must be byte-for-byte idempotent')
            if variant == 'caps-after':
                patched = add_caps(patched, cmake)
                file.write_text(patched)
                patch(file)
                if patched != file.read_text():
                    raise AssertionError('Patch must preserve a subsequent caps hook')
            deletion = extract_function(patched, 'lv_thread_delete')
            wrapper = extract_function(patched, 'prvRunThread')
            # Compilation consumes actual production bodies, not model copies.
            source = MODEL.replace('@DELETE@', deletion).replace('@WRAPPER@', wrapper)
            definitions = ['-DESP_PLATFORM=1', '-DESP_LVGL_ADAPTER_THREAD_OWNER_DELETE=1']
            if variant.startswith('caps-'):
                definitions += ['-DLV_FREERTOS_TASK_STACK_ALLOC_CAPS=1']
            configs = [('smp2', definitions, 2), ('smp3', definitions, 3),
                       ('single-core', definitions, 1),
                       ('owner-guard-off', [d for d in definitions if 'OWNER_DELETE' not in d], 2),
                       ('platform-guard-off', [d for d in definitions if 'ESP_PLATFORM' not in d], 2)]
            for label, flags, cores in configs:
                c_file = temporary / f'{variant}-{label}.c'
                executable = c_file.with_suffix('')
                c_file.write_text(source)
                command([compiler, '-std=c11', '-Wall', '-Wextra', '-Werror',
                         '-Wno-unused-function', f'-DconfigNUMBER_OF_CORES={cores}',
                         *flags, str(c_file), '-o', str(executable)])
                command([str(executable)])
                count += 1

            # The targeted mutant retains owner suspend and every other guard;
            # skipping just the core-quiescence wait must recover the orphan.
            mutant, replacements = re.subn(r'if\s*\(\s*!running\s*\)\s*break;',
                                           'if((void)running, 1) break;', deletion)
            if replacements != 1:
                raise AssertionError('Could not uniquely mutate the quiescence wait')
            mutant_source = MODEL.replace('@DELETE@', mutant).replace('@WRAPPER@', wrapper)
            # This remains valid when the supplied LVGL tree was patched by a
            # prior CMake configure: strip only the added owner guard and keep
            # the actual backend's original normal/caps API selection.
            old_delete = without_owner_guard(deletion)
            old_source = MODEL.replace('@DELETE@', old_delete).replace('@WRAPPER@', wrapper)
            for label, regression in [('wait-mutant', mutant_source), ('old-delete', old_source)]:
                c_file = temporary / f'{variant}-{label}.c'
                executable = c_file.with_suffix('')
                c_file.write_text(regression)
                command([compiler, '-std=c11', '-Wall', '-Wextra', '-Werror',
                         '-Wno-unused-function', '-DconfigNUMBER_OF_CORES=2',
                         *definitions, str(c_file), '-o', str(executable)])
                command([str(executable), '--race-only'], expect_orphan=True)
                count += 1
            print(f'PASS {source_path}: {variant}: five configurations; '
                  'old deletion and wait mutant both reproduce the 33128-byte orphan')
    return count


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--lvgl', type=Path, action='append',
                        help='LVGL source root (or lv_freertos.c); repeat for several versions')
    parser.add_argument('--helper', type=Path, help='Production patch_lvgl_thread_shutdown.py')
    parser.add_argument('--cmake', type=Path, help='Adapter CMakeLists.txt with the caps hook')
    parser.add_argument('--cc', default='cc', help='Host C compiler (default: cc)')
    args = parser.parse_args()
    default_helper = Path(__file__).resolve().parents[2] / 'tools/patch_lvgl_thread_shutdown.py'
    helper = args.helper or default_helper
    if not helper.is_file():
        parser.error(f'Helper missing: {helper}; provide --helper')
    cmake = args.cmake or helper.parent.parent / 'CMakeLists.txt'
    if not cmake.is_file():
        parser.error(f'CMakeLists.txt missing: {cmake}; provide --cmake')
    if not args.lvgl:
        parser.error('Provide --lvgl /path/to/lvgl (may be repeated)')
    compiler = shutil.which(args.cc)
    if not compiler:
        parser.error(f'C compiler not found: {args.cc}')
    inputs = [helper, cmake] + [p if p.is_file() else p / 'src/osal/lv_freertos.c' for p in args.lvgl]
    digests = {p: hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs}
    count = sum(run_suite(lvgl, helper, cmake, compiler) for lvgl in args.lvgl)
    for path, before in digests.items():
        if hashlib.sha256(path.read_bytes()).hexdigest() != before:
            raise AssertionError(f'Production input changed during test: {path}')
    print(f'PASS: {count} compiled programs; production inputs unchanged; '
          'all expected-failure controls detected')


if __name__ == '__main__':
    main()
