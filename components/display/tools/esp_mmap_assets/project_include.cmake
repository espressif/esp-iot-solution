# spiffs_create_partition_assets
#
# Create a spiffs image of the specified directory on the host during build and optionally
# have the created image flashed using `idf.py flash`
function(spiffs_create_partition_assets partition base_dir)
    set(options FLASH_IN_PROJECT)
    set(multi DEPENDS)
    cmake_parse_arguments(arg "${options}" "" "${multi}" "${ARGN}")

    execute_process(
        COMMAND pip install Pillow
        RESULT_VARIABLE pip_result
        OUTPUT_VARIABLE pip_output
    )

    if(NOT pip_result EQUAL 0)
        message(FATAL_ERROR "Failed to execute 'python -c \"import Pillow; print(Pillow.__version__)\"'")
    endif()

    get_filename_component(base_dir_full_path ${base_dir} ABSOLUTE)

    partition_table_get_partition_info(size "--partition-name ${partition}" "size")
    partition_table_get_partition_info(offset "--partition-name ${partition}" "offset")

    if("${size}" AND "${offset}")

        idf_component_get_property(COMPONENT_PATH esp_mmap_assets COMPONENT_DIR)
        if(COMPONENT_PATH)
            message(STATUS "Component esp_mmap_assets dir: ${COMPONENT_PATH}")
            set(image_file ${CMAKE_BINARY_DIR}/mmap_build/${partition}.bin)
            set(MVMODEL_EXE ${COMPONENT_PATH}/spiffs_assets_gen.py)
        else()
            message(FATAL_ERROR "Component esp_mmap_assets not found")
        endif()

        set(MMAP_SUPPORT_SPNG "$<IF:$<STREQUAL:${CONFIG_MMAP_SUPPORT_SPNG},y>,ON,OFF>")
        set(MMAP_SUPPORT_SJPG "$<IF:$<STREQUAL:${CONFIG_MMAP_SUPPORT_SJPG},y>,ON,OFF>")

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
