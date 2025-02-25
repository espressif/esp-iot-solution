# @brief Link designated functions from SRAM to Flash to save SRAM
if(CONFIG_CU_RELINKER_ENABLE)
    # project_elf variable is only in project.cmake
    if(NOT TARGET customer_sections AND DEFINED project_elf)
        message(STATUS "Relinker is enabled.")
        if(CONFIG_IDF_TARGET_ESP32C2)
            set(target "esp32c2")
        elseif(CONFIG_IDF_TARGET_ESP32C3)
            set(target "esp32c3")
        else()
            message(FATAL_ERROR "Other targets are not supported.")
        endif()

        set(idf_version "$ENV{IDF_VERSION}")
        string(REGEX MATCH "^([0-9]+\\.[0-9]+)" version_prefix "${idf_version}")

        if(CONFIG_CU_RELINKER_ENABLE_CUSTOMIZED_CONFIGURATION_FILES)
            idf_build_get_property(project_dir PROJECT_DIR)
            get_filename_component(cfg_file_path "${CONFIG_CU_RELINKER_CUSTOMIZED_CONFIGURATION_FILES_PATH}"
                                ABSOLUTE BASE_DIR "${project_dir}")

            if(NOT EXISTS "${cfg_file_path}")
                message(FATAL_ERROR "Relinker Configuration files path ${cfg_file_path} is not found.")
            endif()
        else()
            set(cfg_file_path ${PROJECT_DIR}/relinker/${target})
            if(NOT EXISTS ${cfg_file_path})
                if(version_prefix STREQUAL "5.0" OR version_prefix STREQUAL "5.3")
                    if(CONFIG_CU_RELINKER_LINK_SPECIFIC_FUNCTIONS_TO_IRAM)
                        set(cfg_file_path ${CMAKE_CURRENT_LIST_DIR}/scripts/relinker/examples/flash_suspend/${target}/${version_prefix})
                    else()
                        set(cfg_file_path ${CMAKE_CURRENT_LIST_DIR}/scripts/relinker/examples/iram_strip/${target}/${version_prefix})
                    endif()
                else()
                    message(FATAL_ERROR "There is no configuration file corresponding to esp-idf/v${version_prefix}, please make a request in github issue")
                endif()
            endif()
        endif()

        message(STATUS "Relinker configuration files: ${cfg_file_path}")

        set(library_file "${cfg_file_path}/library.csv")
        set(object_file "${cfg_file_path}/object.csv")
        set(function_file "${cfg_file_path}/function.csv")
        set(relinker_script "${CMAKE_CURRENT_LIST_DIR}/scripts/relinker/relinker.py")

        set(cmake_objdump "${CMAKE_OBJDUMP}")
        set(link_path "${CMAKE_BINARY_DIR}/esp-idf/esp_system/ld")
        set(link_src_file "${link_path}/sections.ld")
        set(link_dst_file "${link_path}/customer_sections.ld")

        set(relinker_opts --input     ${link_src_file}
                          --output    ${link_dst_file}
                          --library   ${library_file}
                          --object    ${object_file}
                          --function  ${function_file}
                          --sdkconfig ${sdkconfig}
                          --target    ${target}
                          --version   ${version_prefix}
                          --objdump   ${cmake_objdump})

        if(CONFIG_CU_RELINKER_ENABLE_PRINT_ERROR_INFO_WHEN_MISSING_FUNCTION)
            list(APPEND relinker_opts --missing_function_info)
        endif()

        if(CONFIG_CU_RELINKER_LINK_SPECIFIC_FUNCTIONS_TO_IRAM)
            list(APPEND relinker_opts --link_to_iram)
        endif()

        idf_build_get_property(link_depends __LINK_DEPENDS)

        add_custom_command(OUTPUT  ${link_dst_file}
                           COMMAND ${python} -B ${relinker_script}
                                   ${relinker_opts}
                           COMMAND ${CMAKE_COMMAND} -E copy
                                   ${link_dst_file}
                                   ${link_src_file}
                           COMMAND ${CMAKE_COMMAND} -E echo
                                   /*relinker*/ >>
                                   ${link_dst_file}
                           DEPENDS "${link_depends}"
                                   "${library_file}"
                                   "${object_file}"
                                   "${function_file}"
                           VERBATIM)

        add_custom_target(customer_sections DEPENDS ${link_dst_file})
        add_dependencies(${project_elf} customer_sections)
    endif()
else()
    message(STATUS "Relinker isn't enabled.")
endif()
