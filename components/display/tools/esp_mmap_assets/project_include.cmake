# spiffs_create_partition_assets
#
# Create a spiffs image of the specified directory on the host during build and optionally
# have the created image flashed using `idf.py flash`
function(spiffs_create_partition_assets partition base_dir)
    # Define option flags (BOOL)
    set(options FLASH_IN_PROJECT
                FLASH_APPEND_APP
                MMAP_SUPPORT_SJPG
                MMAP_SUPPORT_SPNG
                MMAP_SUPPORT_QOI
                MMAP_SUPPORT_SQOI
                MMAP_SUPPORT_PJPG
                MMAP_SUPPORT_RAW
                MMAP_RAW_DITHER
                MMAP_RAW_BGR_MODE)

    # Define one-value arguments (STRING and INT)
    set(one_value_args MMAP_FILE_SUPPORT_FORMAT
                       MMAP_SPLIT_HEIGHT
                       MMAP_RAW_FILE_FORMAT
                       MMAP_RAW_COLOR_FORMAT
                       IMPORT_INC_PATH
                       COPY_PREBUILT_BIN)

    # Define multi-value arguments
    set(multi DEPENDS)

    # Parse the arguments passed to the function
    cmake_parse_arguments(arg
                          "${options}"
                          "${one_value_args}"
                          "${multi}"
                          "${ARGN}")

    # Check if COPY_PREBUILT_BIN is enabled (has a path provided)
    if(DEFINED arg_COPY_PREBUILT_BIN AND NOT arg_COPY_PREBUILT_BIN STREQUAL "")
        if(NOT EXISTS "${arg_COPY_PREBUILT_BIN}")
            message(FATAL_ERROR "COPY_PREBUILT_BIN file not found: ${arg_COPY_PREBUILT_BIN}")
        endif()
        message(STATUS "Copy pre-built bin file: ${arg_COPY_PREBUILT_BIN}")
    endif()

    # Skip format and conversion validations for copy mode
    if(NOT (DEFINED arg_COPY_PREBUILT_BIN AND NOT arg_COPY_PREBUILT_BIN STREQUAL ""))
        if(NOT DEFINED arg_MMAP_FILE_SUPPORT_FORMAT OR arg_MMAP_FILE_SUPPORT_FORMAT STREQUAL "")
            message(FATAL_ERROR "MMAP_FILE_SUPPORT_FORMAT is empty. Please input the file suffixes you want (e.g .png, .jpg).")
        endif()

        if(arg_MMAP_SUPPORT_QOI AND (arg_MMAP_SUPPORT_SJPG OR arg_MMAP_SUPPORT_SPNG))
            message(FATAL_ERROR "MMAP_SUPPORT_QOI depends on !MMAP_SUPPORT_SJPG && !MMAP_SUPPORT_SPNG.")
        endif()

        if(arg_MMAP_SUPPORT_SQOI AND NOT arg_MMAP_SUPPORT_QOI)
            message(FATAL_ERROR "MMAP_SUPPORT_SQOI depends on MMAP_SUPPORT_QOI.")
        endif()

        if( (arg_MMAP_SUPPORT_SJPG OR arg_MMAP_SUPPORT_SPNG OR arg_MMAP_SUPPORT_SQOI) AND
            (NOT DEFINED arg_MMAP_SPLIT_HEIGHT OR arg_MMAP_SPLIT_HEIGHT LESS 1) )
            message(FATAL_ERROR "MMAP_SPLIT_HEIGHT must be defined and its value >= 1 when MMAP_SUPPORT_SJPG, MMAP_SUPPORT_SPNG, or MMAP_SUPPORT_SQOI is enabled.")
        endif()

        if(DEFINED arg_MMAP_SPLIT_HEIGHT)
            if(NOT (arg_MMAP_SUPPORT_SJPG OR arg_MMAP_SUPPORT_SPNG OR arg_MMAP_SUPPORT_SQOI))
                message(FATAL_ERROR "MMAP_SPLIT_HEIGHT depends on MMAP_SUPPORT_SJPG || MMAP_SUPPORT_SPNG || MMAP_SUPPORT_SQOI.")
            endif()
        endif()

        if(arg_MMAP_SUPPORT_RAW AND (arg_MMAP_SUPPORT_SJPG OR arg_MMAP_SUPPORT_SPNG OR arg_MMAP_SUPPORT_QOI OR arg_MMAP_SUPPORT_SQOI OR arg_MMAP_SUPPORT_PJPG))
            message(FATAL_ERROR "MMAP_SUPPORT_RAW and MMAP_SUPPORT_SJPG/MMAP_SUPPORT_SPNG/MMAP_SUPPORT_QOI/MMAP_SUPPORT_SQOI/MMAP_SUPPORT_PJPG cannot be enabled at the same time.")
        endif()

        # Try to install Pillow using pip
        idf_build_get_property(python PYTHON)
        execute_process(
            COMMAND ${python} -c "import PIL"
            RESULT_VARIABLE PIL_FOUND
            OUTPUT_QUIET
            ERROR_QUIET
        )

        if(NOT PIL_FOUND EQUAL 0)
            message(STATUS "Pillow not found. Attempting to install it using pip...")

            execute_process(
                COMMAND ${python} -m pip install -U Pillow
                RESULT_VARIABLE result
                OUTPUT_VARIABLE output
                ERROR_VARIABLE error
                OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_STRIP_TRAILING_WHITESPACE
            )

            if(result)
                message(FATAL_ERROR "Failed to install Pillow using pip. Please install it manually.\nError: ${error}")
            else()
                message(STATUS "Pillow successfully installed.")
            endif()
        endif()

        execute_process(
            COMMAND ${python} -c "import numpy"
            RESULT_VARIABLE NUMPY_FOUND
            OUTPUT_QUIET
            ERROR_QUIET
        )

        if(NOT NUMPY_FOUND EQUAL 0)
            message(STATUS "NumPy not found. Attempting to install it using pip...")

            execute_process(
                COMMAND ${python} -m pip install -U numpy
                RESULT_VARIABLE result
                OUTPUT_VARIABLE output
                ERROR_VARIABLE error
                OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_STRIP_TRAILING_WHITESPACE
            )

            if(result)
                message(FATAL_ERROR "Failed to install NumPy using pip. Please install it manually.\nError: ${error}")
            else()
                message(STATUS "NumPy successfully installed.")
            endif()
        endif()

        # Try to install qoi-conv using pip
        execute_process(
            COMMAND ${python} -c "import importlib; importlib.import_module('qoi-conv')"
            RESULT_VARIABLE PIL_FOUND
            OUTPUT_QUIET
            ERROR_QUIET
        )

        if(NOT PIL_FOUND EQUAL 0)
            message(STATUS "qoi-conv not found. Attempting to install it using pip...")

            execute_process(
                COMMAND ${python} -m pip install -U qoi-conv
                RESULT_VARIABLE result
                OUTPUT_VARIABLE output
                ERROR_VARIABLE error
                OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_STRIP_TRAILING_WHITESPACE
            )

            if(result)
                message(FATAL_ERROR "Failed to install qoi-conv using pip. Please install it manually.\nError: ${error}")
            else()
                message(STATUS "qoi-conv successfully installed.")
            endif()
        endif()
    endif()

    get_filename_component(base_dir_full_path ${base_dir} ABSOLUTE)
    get_filename_component(base_dir_name "${base_dir_full_path}" NAME)

    partition_table_get_partition_info(size "--partition-name ${partition}" "size")
    partition_table_get_partition_info(offset "--partition-name ${partition}" "offset")

    if("${size}" AND "${offset}")

        set(TARGET_COMPONENT "")
        set(TARGET_COMPONENT_PATH "")

        idf_build_get_property(build_components BUILD_COMPONENTS)
        foreach(COMPONENT ${build_components})
            if(COMPONENT MATCHES "esp_mmap_assets" OR COMPONENT MATCHES "espressif__esp_mmap_assets")
                set(TARGET_COMPONENT ${COMPONENT})
                break()
            endif()
        endforeach()

        if(TARGET_COMPONENT STREQUAL "")
            message(FATAL_ERROR "Component 'esp_mmap_assets' not found.")
        else()
            idf_component_get_property(TARGET_COMPONENT_PATH ${TARGET_COMPONENT} COMPONENT_DIR)
        endif()

        get_filename_component(PY_TOOL_DIR "${TARGET_COMPONENT_PATH}/py_tool" ABSOLUTE)

        set(image_file ${CMAKE_BINARY_DIR}/mmap_build/${base_dir_name}/${partition}/${partition}.bin)
        set(MVMODEL_EXE ${PY_TOOL_DIR}/spiffs_assets_gen.py)

        if(arg_MMAP_SUPPORT_RAW AND NOT (DEFINED arg_COPY_PREBUILT_BIN AND NOT arg_COPY_PREBUILT_BIN STREQUAL ""))
            foreach(COMPONENT ${build_components})
            if(COMPONENT MATCHES "^lvgl$" OR COMPONENT MATCHES "^lvgl__lvgl$")
                set(lvgl_name ${COMPONENT})
                if(COMPONENT STREQUAL "lvgl")
                    set(lvgl_ver $ENV{LVGL_VERSION})
                else()
                    idf_component_get_property(lvgl_ver ${lvgl_name} COMPONENT_VERSION)
                endif()
                break()
            endif()
            endforeach()

            if("${lvgl_ver}" STREQUAL "")
                message("Could not determine LVGL version, assuming v8.x")
                set(lvgl_ver "8.0.0")
            endif()
            message(STATUS "LVGL version: ${lvgl_ver}")
        endif()

        if(NOT arg_MMAP_SPLIT_HEIGHT)
            set(arg_MMAP_SPLIT_HEIGHT 0) # Default value
        endif()

        # Handle IMPORT_INC_PATH parameter
        if(DEFINED arg_IMPORT_INC_PATH)
            set(import_include_path ${arg_IMPORT_INC_PATH})
        else()
            set(import_include_path ${CMAKE_CURRENT_LIST_DIR})
        endif()

        string(TOLOWER "${arg_MMAP_SUPPORT_SJPG}" support_sjpg)
        string(TOLOWER "${arg_MMAP_SUPPORT_SPNG}" support_spng)
        string(TOLOWER "${arg_MMAP_SUPPORT_QOI}" support_qoi)
        string(TOLOWER "${arg_MMAP_SUPPORT_SQOI}" support_sqoi)
        string(TOLOWER "${arg_MMAP_SUPPORT_PJPG}" support_pjpg)
        string(TOLOWER "${arg_MMAP_SUPPORT_RAW}" support_raw)
        string(TOLOWER "${arg_MMAP_RAW_DITHER}" support_raw_dither)
        string(TOLOWER "${arg_MMAP_RAW_BGR_MODE}" support_raw_bgr)

        set(app_bin_path "${CMAKE_BINARY_DIR}/${CMAKE_PROJECT_NAME}.bin")
        if(DEFINED arg_COPY_PREBUILT_BIN AND NOT arg_COPY_PREBUILT_BIN STREQUAL "")
            set(source_bin_path "${arg_COPY_PREBUILT_BIN}")
        else()
            set(source_bin_path "")
        endif()

        set(CONFIG_DIR "${CMAKE_BINARY_DIR}/mmap_build/${base_dir_name}")
        file(MAKE_DIRECTORY "${CONFIG_DIR}")
        set(CONFIG_FILE_PATH "${CONFIG_DIR}/${partition}.json")
        set(PJPG_PROCESSOR_PATH "${PY_TOOL_DIR}/png_processor.py")
        set(partition ${partition})

        configure_file(
            "${TARGET_COMPONENT_PATH}/config_template.json.in"
            "${CONFIG_FILE_PATH}"
            @ONLY
        )

        if(DEFINED arg_COPY_PREBUILT_BIN AND NOT arg_COPY_PREBUILT_BIN STREQUAL "")
            add_custom_target(assets_${partition}_bin ALL
                COMMENT "Copy prebuilt binary"
                COMMAND ${python} ${MVMODEL_EXE} --config "${CONFIG_FILE_PATH}" --copy
                DEPENDS ${arg_DEPENDS}
                VERBATIM)
        else()
            add_custom_target(assets_${partition}_bin ALL
                COMMENT "Build assets binary"
                COMMAND ${python} ${MVMODEL_EXE} --config "${CONFIG_FILE_PATH}" --build
                DEPENDS ${arg_DEPENDS}
                VERBATIM)
        endif()

        if(arg_FLASH_APPEND_APP)
            add_custom_target(assets_${partition}_merge_bin ALL
            COMMENT "Merge binary files"
            COMMAND ${python} ${MVMODEL_EXE} --config "${CONFIG_FILE_PATH}" --merge
            COMMAND ${CMAKE_COMMAND} -E rm "${build_dir}/.bin_timestamp" # Remove the timestamp file to force re-run
            DEPENDS assets_${partition}_bin app
            VERBATIM)
        endif()

        if(arg_FLASH_IN_PROJECT)
            set(assets_target "assets_${partition}_bin")

            if(arg_FLASH_APPEND_APP)
                set(assets_target "assets_${partition}_merge_bin")
                add_dependencies(app-flash ${assets_target})
            else()
                esptool_py_flash_to_partition(flash "${partition}" "${image_file}")
            endif()

            add_dependencies(flash ${assets_target})
        endif()

    else()
        set(message "Failed to create assets bin for partition '${partition}'. "
                    "Check project configuration if using the correct partition table file.")
        fail_at_build_time(assets_${partition}_bin "${message}")
    endif()
endfunction()
