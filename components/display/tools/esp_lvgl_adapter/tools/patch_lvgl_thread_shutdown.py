# SPDX-FileCopyrightText: 2026 youkorr
# SPDX-License-Identifier: MIT
"""Keep LVGL thread-return ownership and delete only after remote execution stops."""
import argparse
import os
from pathlib import Path
import re
import tempfile

RETURN_MARKER = 'esp_lvgl_adapter: owner deletes completed threads'
DELETE_MARKER = 'esp_lvgl_adapter: stop remote execution before deleting'
RETURN_OLD = '''    pxThread->pvStartRoutine((void *)pxThread->pTaskArg);

    vTaskDelete(NULL);'''
RETURN_NEW = '''    pxThread->pvStartRoutine((void *)pxThread->pTaskArg);

#if defined(ESP_PLATFORM) && defined(ESP_LVGL_ADAPTER_THREAD_OWNER_DELETE)
    /* esp_lvgl_adapter: owner deletes completed threads. Keep the TCB and
     * caps-allocated stack valid until lv_thread_delete() releases them. */
    for(;;) {
        vTaskSuspend(NULL);
    }
#else
    vTaskDelete(NULL);
#endif'''
RETURN_LEGACY_PLATFORM = RETURN_NEW.replace(
    '#if defined(ESP_PLATFORM) && defined(ESP_LVGL_ADAPTER_THREAD_OWNER_DELETE)',
    '#ifdef ESP_PLATFORM', 1)
DELETE_GUARD = '''#if defined(ESP_PLATFORM) && defined(ESP_LVGL_ADAPTER_THREAD_OWNER_DELETE)
    /* esp_lvgl_adapter: stop remote execution before deleting. A callback
     * returning on another core can still call vTaskSuspend(NULL). If it
     * does so after vTaskDelete queued its TCB for idle cleanup, it moves
     * that TCB out of the termination list and leaves an orphan task.
     * Suspending first prevents new scheduling; waiting for all current
     * handles to change ensures that the last self-suspend has finished. */
    if(pxThread->xTaskHandle == NULL || pxThread->xTaskHandle == xTaskGetCurrentTaskHandle())
        return LV_RESULT_INVALID;
    vTaskSuspend(pxThread->xTaskHandle);
#if configNUMBER_OF_CORES > 1
    for(;;) {
        BaseType_t running = pdFALSE;
        for(BaseType_t core = 0; core < configNUMBER_OF_CORES; core++) {
            if(xTaskGetCurrentTaskHandleForCore(core) == pxThread->xTaskHandle) {
                running = pdTRUE;
                break;
            }
        }
        if(!running) break;
        taskYIELD();
    }
#endif
#endif
'''
DELETE_FINISH = '''#if defined(ESP_PLATFORM) && defined(ESP_LVGL_ADAPTER_THREAD_OWNER_DELETE)
    pxThread->xTaskHandle = NULL;
#endif

'''
INCLUDE = '''#if defined(ESP_PLATFORM) && defined(ESP_LVGL_ADAPTER_THREAD_OWNER_DELETE)
    #include <freertos/idf_additions.h>
#endif
'''


def patch(path):
    original = path.read_text(encoding='utf-8')
    source = original
    if RETURN_NEW in source:
        if source.count(RETURN_NEW) != 1 or source.count(RETURN_MARKER) != 1:
            raise ValueError('Duplicate LVGL FreeRTOS callback-return ownership hook')
    elif RETURN_LEGACY_PLATFORM in source:
        # Earlier test builds used the same marker but guarded the callback
        # only with ESP_PLATFORM. Upgrade that exact form so disabling the
        # adapter restores upstream self-deletion instead of orphaning a task.
        if source.count(RETURN_LEGACY_PLATFORM) != 1 or source.count(RETURN_MARKER) != 1:
            raise ValueError('Duplicate legacy LVGL FreeRTOS callback-return hook')
        source = source.replace(RETURN_LEGACY_PLATFORM, RETURN_NEW, 1)
    elif RETURN_MARKER in source:
        raise ValueError('Unrecognized or partial LVGL FreeRTOS callback-return ownership hook')
    else:
        if source.count(RETURN_OLD) != 1:
            raise ValueError('Unrecognized LVGL FreeRTOS callback-return layout')
        source = source.replace(RETURN_OLD, RETURN_NEW, 1)
    match = re.search(r'lv_result_t lv_thread_delete\(lv_thread_t \* pxThread\)\n\{\n.*?\n\}', source, re.S)
    if not match:
        raise ValueError('Unrecognized LVGL FreeRTOS thread-delete layout')
    body = match.group(0)
    if not re.search(r'vTaskDelete(?:WithCaps)?\(pxThread->xTaskHandle\);', body):
        raise ValueError('LVGL thread deletion does not use a recognized FreeRTOS API')
    if body.count('    return LV_RESULT_OK;') != 1:
        raise ValueError('Unrecognized LVGL thread-delete return path')
    if DELETE_MARKER in source:
        if source.count(DELETE_MARKER) != 1 or body.count(DELETE_GUARD) != 1 or body.count(DELETE_FINISH) != 1:
            raise ValueError('Unrecognized or partial LVGL FreeRTOS thread-delete ownership hook')
    else:
        if 'ESP_LVGL_ADAPTER_THREAD_OWNER_DELETE' in body:
            raise ValueError('Partial LVGL FreeRTOS thread-delete ownership guard')
        body = body.replace('{\n', '{\n' + DELETE_GUARD, 1)
        body = body.replace('    return LV_RESULT_OK;', DELETE_FINISH + '    return LV_RESULT_OK;', 1)
        source = source[:match.start()] + body + source[match.end():]
    if '#include <freertos/idf_additions.h>' not in source:
        anchor = '#if LV_USE_OS == LV_OS_FREERTOS\n'
        if source.count(anchor) != 1:
            raise ValueError('Unrecognized LVGL FreeRTOS source guard')
        # Use angle brackets: the independent stack-caps hook recognizes its
        # own quoted include, and must not mistake this one for a partial hook.
        source = source.replace(anchor, anchor + '\n' + INCLUDE, 1)
    elif source.count(INCLUDE) != 1:
        raise ValueError('Unrecognized LVGL FreeRTOS ownership include guard')
    if source == original:
        return 'already patched'
    # Managed component sources can be hard-linked to the shared cache.
    # Replace this entry instead of modifying every linked source in place.
    mode = path.stat().st_mode & 0o777
    with tempfile.NamedTemporaryFile('w', encoding='utf-8', dir=path.parent, delete=False) as output:
        temporary = Path(output.name)
        output.write(source)
    try:
        temporary.chmod(mode)
        os.replace(temporary, path)
    finally:
        temporary.unlink(missing_ok=True)
    return 'patched'


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('lv_freertos', type=Path)
    args = parser.parse_args()
    print(patch(args.lv_freertos))
