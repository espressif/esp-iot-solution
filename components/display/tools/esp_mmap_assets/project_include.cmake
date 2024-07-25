# spiffs_create_partition_assets
#
# Create a spiffs image of the specified directory on the host during build and optionally
# have the created image flashed using `idf.py flash`
function(spiffs_create_partition_assets partition base_dir)
    set(options FLASH_IN_PROJECT)
    set(multi DEPENDS)
    cmake_parse_arguments(arg "${options}" "" "${multi}" "${ARGN}")

    # Try to install Pillow using pip
    idf_build_get_property(python PYTHON)
    execute_process(
        COMMAND ${python} -c "import PIL"
        RESULT_VARIABLE PIL_FOUND
        OUTPUT_QUIET
        ERROR_QUIET
    )

    if(PIL_FOUND EQUAL 0)
        message(STATUS "Pillow is installed.")
    else()
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

    get_filename_component(base_dir_full_path ${base_dir} ABSOLUTE)

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
            message(STATUS "Component dir: ${TARGET_COMPONENT_PATH}")
        endif()

        set(image_file ${CMAKE_BINARY_DIR}/mmap_build/${partition}.bin)
        set(MVMODEL_EXE ${TARGET_COMPONENT_PATH}/spiffs_assets_gen.py)

        set(MMAP_SUPPORT_SPNG "$<IF:$<STREQUAL:${CONFIG_MMAP_SUPPORT_SPNG},y>,ON,OFF>")
        set(MMAP_SUPPORT_SJPG "$<IF:$<STREQUAL:${CONFIG_MMAP_SUPPORT_SJPG},y>,ON,OFF>")

        if(NOT DEFINED CONFIG_MMAP_SPLIT_HEIGHT OR CONFIG_MMAP_SPLIT_HEIGHT STREQUAL "")
            set(CONFIG_MMAP_SPLIT_HEIGHT 0)  # Default value
        endif()

        add_custom_target(spiffs_${partition}_bin ALL
            COMMENT "Move and Pack assets..."
            COMMAND python ${MVMODEL_EXE}
            -d1 ${PROJECT_DIR}
            -d2 ${CMAKE_CURRENT_LIST_DIR}
            -d3 ${base_dir_full_path}
            -d4 ${size}
            -d5 ${image_file}
            -d6 ${MMAP_SUPPORT_SPNG}
            -d7 ${MMAP_SUPPORT_SJPG}
            -d8 ${CONFIG_MMAP_FILE_SUPPORT_FORMAT}
            -d9 ${CONFIG_MMAP_SPLIT_HEIGHT}
            DEPENDS ${arg_DEPENDS}
            VERBATIM)

        set_property(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" APPEND PROPERTY
            ADDITIONAL_CLEAN_FILES
            ${image_file})

        if(arg_FLASH_IN_PROJECT)
            esptool_py_flash_to_partition(flash "${partition}" "${image_file}")
            add_dependencies(flash spiffs_${partition}_bin)
        endif()
    else()
        set(message "Failed to create assets bin for partition '${partition}'. "
                    "Check project configuration if using the correct partition table file.")
        fail_at_build_time(spiffs_${partition}_bin "${message}")
    endif()
endfunction()
