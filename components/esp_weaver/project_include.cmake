# ESP Weaver - Auto-applied via project_include.cmake
#
# This file is automatically included by ESP-IDF during the project() call
# when esp_weaver component is found. It applies necessary patches to ESP-IDF.

# Find Python interpreter (required for patch script)
find_package(Python3 COMPONENTS Interpreter QUIET)

if(NOT Python3_FOUND)
    message(WARNING "[ESP-Weaver] Python3 not found, patches cannot be applied")
    return()
endif()

# Determine the component root directory (where this file lives)
get_filename_component(_ESP_WEAVER_DIR "${CMAKE_CURRENT_LIST_DIR}" ABSOLUTE)

# ============================================================================
# Apply ESP-IDF Patches
# ============================================================================
# Note: Although this runs during project(), IDF patches typically modify source
# files which are compiled later, so this timing should work for most patches.

if(DEFINED ENV{IDF_PATH} AND EXISTS "${_ESP_WEAVER_DIR}/tools/apply_patches.py")
    message(STATUS "[ESP-Weaver] Applying ESP-IDF patches...")
    execute_process(
        COMMAND ${Python3_EXECUTABLE} "${_ESP_WEAVER_DIR}/tools/apply_patches.py"
                --patches-dir "${_ESP_WEAVER_DIR}/patches"
                --idf-path "$ENV{IDF_PATH}"
        WORKING_DIRECTORY "${_ESP_WEAVER_DIR}"
        RESULT_VARIABLE _IDF_PATCH_RESULT
        OUTPUT_VARIABLE _IDF_PATCH_OUTPUT
        ERROR_VARIABLE _IDF_PATCH_ERROR
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_STRIP_TRAILING_WHITESPACE
    )
    if(NOT _IDF_PATCH_RESULT EQUAL 0)
        message(WARNING "[ESP-Weaver] Some ESP-IDF patches may have failed (exit code: ${_IDF_PATCH_RESULT})\n"
            "stdout: ${_IDF_PATCH_OUTPUT}\n"
            "stderr: ${_IDF_PATCH_ERROR}")
    else()
        message(STATUS "[ESP-Weaver] ESP-IDF patches applied successfully")
    endif()
endif()
