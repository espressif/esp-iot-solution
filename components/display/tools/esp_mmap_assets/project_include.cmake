function(_esp_mmap_assets_ensure_python_module python_exe pip_package import_module)
    execute_process(
        COMMAND ${python_exe} -c "import importlib; importlib.import_module('${import_module}')"
        RESULT_VARIABLE module_found
        OUTPUT_QUIET
        ERROR_QUIET
    )

    if(NOT module_found EQUAL 0)
        message(STATUS "${pip_package} not found. Attempting to install it using pip...")

        execute_process(
            COMMAND ${python_exe} -m pip install -U ${pip_package}
            RESULT_VARIABLE result
            OUTPUT_VARIABLE output
            ERROR_VARIABLE error
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_STRIP_TRAILING_WHITESPACE
        )

        if(result)
            message(FATAL_ERROR "Failed to install ${pip_package} using pip. Please install it manually.\nError: ${error}")
        else()
            message(STATUS "${pip_package} successfully installed.")
        endif()
    endif()
endfunction()

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
                       MMAP_FILE_ALIGNMENT
                       MMAP_SPLIT_HEIGHT
                       MMAP_RAW_FILE_FORMAT
                       MMAP_RAW_COLOR_FORMAT
                       IMPORT_INC_PATH
                       COPY_PREBUILT_BIN)

    # Define multi-value arguments
    set(multi DEPENDS)

    # Parse the arguments passed to the function
    cmake_parse_arguments(assets
                          "${options}"
                          "${one_value_args}"
                          "${multi}"
                          "${ARGN}")

    set(copy_prebuilt_bin OFF)
    if(DEFINED assets_COPY_PREBUILT_BIN AND NOT assets_COPY_PREBUILT_BIN STREQUAL "")
        set(copy_prebuilt_bin ON)
    endif()
    idf_build_get_property(python_exe PYTHON)

    string(TOLOWER "${assets_MMAP_SUPPORT_SJPG}" support_sjpg)
    string(TOLOWER "${assets_MMAP_SUPPORT_SPNG}" support_spng)
    string(TOLOWER "${assets_MMAP_SUPPORT_QOI}" support_qoi)
    string(TOLOWER "${assets_MMAP_SUPPORT_SQOI}" support_sqoi)
    string(TOLOWER "${assets_MMAP_SUPPORT_PJPG}" support_pjpg)
    string(TOLOWER "${assets_MMAP_SUPPORT_RAW}" support_raw)
    string(TOLOWER "${assets_MMAP_RAW_DITHER}" support_raw_dither)
    string(TOLOWER "${assets_MMAP_RAW_BGR_MODE}" support_raw_bgr)

    # Check if COPY_PREBUILT_BIN is enabled (has a path provided)
    if(copy_prebuilt_bin)
        if(NOT EXISTS "${assets_COPY_PREBUILT_BIN}")
            message(FATAL_ERROR "COPY_PREBUILT_BIN file not found: ${assets_COPY_PREBUILT_BIN}")
        endif()
        message(STATUS "Copy pre-built bin file: ${assets_COPY_PREBUILT_BIN}")
    endif()

    # Skip format and conversion validations for copy mode
    if(NOT copy_prebuilt_bin)
        if(NOT DEFINED assets_MMAP_FILE_SUPPORT_FORMAT OR assets_MMAP_FILE_SUPPORT_FORMAT STREQUAL "")
            message(FATAL_ERROR "MMAP_FILE_SUPPORT_FORMAT is empty. Please input the file suffixes you want (e.g .png, .jpg).")
        endif()

        if(assets_MMAP_SUPPORT_QOI AND (assets_MMAP_SUPPORT_SJPG OR assets_MMAP_SUPPORT_SPNG))
            message(FATAL_ERROR "MMAP_SUPPORT_QOI depends on !MMAP_SUPPORT_SJPG && !MMAP_SUPPORT_SPNG.")
        endif()

        if(assets_MMAP_SUPPORT_SQOI AND NOT assets_MMAP_SUPPORT_QOI)
            message(FATAL_ERROR "MMAP_SUPPORT_SQOI depends on MMAP_SUPPORT_QOI.")
        endif()

        if( (assets_MMAP_SUPPORT_SJPG OR assets_MMAP_SUPPORT_SPNG OR assets_MMAP_SUPPORT_SQOI) AND
            (NOT DEFINED assets_MMAP_SPLIT_HEIGHT OR assets_MMAP_SPLIT_HEIGHT LESS 1) )
            message(FATAL_ERROR "MMAP_SPLIT_HEIGHT must be defined and its value >= 1 when MMAP_SUPPORT_SJPG, MMAP_SUPPORT_SPNG, or MMAP_SUPPORT_SQOI is enabled.")
        endif()

        if(DEFINED assets_MMAP_SPLIT_HEIGHT)
            if(NOT (assets_MMAP_SUPPORT_SJPG OR assets_MMAP_SUPPORT_SPNG OR assets_MMAP_SUPPORT_SQOI))
                message(FATAL_ERROR "MMAP_SPLIT_HEIGHT depends on MMAP_SUPPORT_SJPG || MMAP_SUPPORT_SPNG || MMAP_SUPPORT_SQOI.")
            endif()
        endif()

        if(assets_MMAP_SUPPORT_RAW AND (assets_MMAP_SUPPORT_SJPG OR assets_MMAP_SUPPORT_SPNG OR assets_MMAP_SUPPORT_QOI OR assets_MMAP_SUPPORT_SQOI OR assets_MMAP_SUPPORT_PJPG))
            message(FATAL_ERROR "MMAP_SUPPORT_RAW and MMAP_SUPPORT_SJPG/MMAP_SUPPORT_SPNG/MMAP_SUPPORT_QOI/MMAP_SUPPORT_SQOI/MMAP_SUPPORT_PJPG cannot be enabled at the same time.")
        endif()

        # Install Python dependencies required by enabled converters.
        string(FIND ",${assets_MMAP_FILE_SUPPORT_FORMAT}," ".jpg" support_jpg_pos)
        string(FIND ",${assets_MMAP_FILE_SUPPORT_FORMAT}," ".jpeg" support_jpeg_pos)
        string(FIND ",${assets_MMAP_FILE_SUPPORT_FORMAT}," ".png" support_png_pos)
        if(assets_MMAP_SUPPORT_SJPG OR assets_MMAP_SUPPORT_SPNG OR assets_MMAP_SUPPORT_QOI OR
           assets_MMAP_SUPPORT_SQOI OR assets_MMAP_SUPPORT_PJPG OR assets_MMAP_SUPPORT_RAW OR
           NOT support_jpg_pos EQUAL -1 OR NOT support_jpeg_pos EQUAL -1 OR NOT support_png_pos EQUAL -1)
            _esp_mmap_assets_ensure_python_module(${python_exe} "Pillow" "PIL")
        endif()

        if(assets_MMAP_SUPPORT_QOI OR assets_MMAP_SUPPORT_SQOI OR assets_MMAP_SUPPORT_PJPG)
            _esp_mmap_assets_ensure_python_module(${python_exe} "numpy" "numpy")
        endif()

        if(assets_MMAP_SUPPORT_QOI OR assets_MMAP_SUPPORT_SQOI)
            _esp_mmap_assets_ensure_python_module(${python_exe} "qoi-conv" "qoi-conv.qoi")
        endif()

        if(assets_MMAP_SUPPORT_RAW)
            _esp_mmap_assets_ensure_python_module(${python_exe} "packaging" "packaging")
        endif()
    endif()

    get_filename_component(base_dir_full_path ${base_dir} ABSOLUTE)
    get_filename_component(base_dir_name "${base_dir_full_path}" NAME)
    if(NOT copy_prebuilt_bin)
        file(GLOB_RECURSE asset_dependencies CONFIGURE_DEPENDS "${base_dir_full_path}/*")
    endif()

    partition_table_get_partition_info(partition_size "--partition-name ${partition}" "size")
    partition_table_get_partition_info(partition_offset "--partition-name ${partition}" "offset")

    if("${partition_size}" AND "${partition_offset}")

        set(target_component "")
        set(target_component_path "")

        idf_build_get_property(build_components BUILD_COMPONENTS)
        foreach(component ${build_components})
            if(component MATCHES "esp_mmap_assets" OR component MATCHES "espressif__esp_mmap_assets")
                set(target_component ${component})
                break()
            endif()
        endforeach()

        if(target_component STREQUAL "")
            message(FATAL_ERROR "Component 'esp_mmap_assets' not found.")
        else()
            idf_component_get_property(target_component_path ${target_component} COMPONENT_DIR)
        endif()

        get_filename_component(py_tool_dir "${target_component_path}/py_tool" ABSOLUTE)

        set(image_file ${CMAKE_BINARY_DIR}/mmap_build/${base_dir_name}/${partition}/${partition}.bin)
        set(assets_gen_script ${py_tool_dir}/spiffs_assets_gen.py)

        if(assets_MMAP_SUPPORT_RAW AND NOT copy_prebuilt_bin)
            foreach(component ${build_components})
                if(component MATCHES "^lvgl$" OR component MATCHES "^lvgl__lvgl$")
                    set(lvgl_name ${component})
                    if(component STREQUAL "lvgl")
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

        if(NOT assets_MMAP_SPLIT_HEIGHT)
            set(assets_MMAP_SPLIT_HEIGHT 0) # Default value
        endif()
        if(NOT DEFINED assets_MMAP_FILE_ALIGNMENT OR assets_MMAP_FILE_ALIGNMENT STREQUAL "")
            set(assets_MMAP_FILE_ALIGNMENT 0)
        endif()

        # Handle IMPORT_INC_PATH parameter
        if(DEFINED assets_IMPORT_INC_PATH)
            set(import_include_path ${assets_IMPORT_INC_PATH})
        else()
            set(import_include_path ${CMAKE_CURRENT_LIST_DIR})
        endif()

        set(app_bin_path "${CMAKE_BINARY_DIR}/${CMAKE_PROJECT_NAME}.bin")
        if(copy_prebuilt_bin)
            set(source_bin_path "${assets_COPY_PREBUILT_BIN}")
        else()
            set(source_bin_path "")
        endif()

        set(config_dir "${CMAKE_BINARY_DIR}/mmap_build/${base_dir_name}")
        file(MAKE_DIRECTORY "${config_dir}")
        set(config_file_path "${config_dir}/${partition}.json")
        set(pjpg_processor_path "${py_tool_dir}/png_processor.py")

        configure_file(
            "${target_component_path}/config_template.json.in"
            "${config_file_path}"
            @ONLY
        )

        if(copy_prebuilt_bin)
            add_custom_command(
                OUTPUT "${image_file}"
                COMMENT "Copy prebuilt binary"
                COMMAND ${python_exe} ${assets_gen_script} --config "${config_file_path}" --copy
                DEPENDS "${assets_COPY_PREBUILT_BIN}" "${config_file_path}" "${assets_gen_script}" ${assets_DEPENDS}
                VERBATIM)
        else()
            add_custom_command(
                OUTPUT "${image_file}"
                COMMENT "Build assets binary"
                COMMAND ${python_exe} ${assets_gen_script} --config "${config_file_path}" --build
                DEPENDS "${config_file_path}" "${assets_gen_script}" ${asset_dependencies} ${assets_DEPENDS}
                VERBATIM)
        endif()

        add_custom_target(assets_${partition}_bin ALL DEPENDS "${image_file}")

        if(assets_FLASH_APPEND_APP)
            add_custom_target(assets_${partition}_merge_bin ALL
            COMMENT "Merge binary files"
            COMMAND ${python_exe} ${assets_gen_script} --config "${config_file_path}" --merge
            COMMAND ${CMAKE_COMMAND} -E rm -f "${CMAKE_BINARY_DIR}/.bin_timestamp" # Remove the timestamp file to force re-run
            DEPENDS assets_${partition}_bin app
            VERBATIM)
        endif()

        if(assets_FLASH_IN_PROJECT)
            set(assets_target "assets_${partition}_bin")

            if(assets_FLASH_APPEND_APP)
                set(assets_target "assets_${partition}_merge_bin")
                add_dependencies(app-flash ${assets_target})
            else()
                esptool_py_flash_to_partition(flash "${partition}" "${image_file}")
            endif()

            add_dependencies(flash ${assets_target})
        endif()

    else()
        set(error_message "Failed to create assets bin for partition '${partition}'. "
                          "Check project configuration if using the correct partition table file.")
        fail_at_build_time(assets_${partition}_bin "${error_message}")
    endif()
endfunction()
