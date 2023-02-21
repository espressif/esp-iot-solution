# @brief Link designated functions from SRAM to Flash to save SRAM
if(CONFIG_CU_RELINKER_ENABLE)
    message(STATUS "Relinker is enabled.")

    set(cfg_file_path ${PROJECT_DIR}/relinker)

    if(CONFIG_IDF_TARGET_ESP32C2)
        set(target "esp32c2")
    else()
        message(FATAL_ERROR "Other targets are not supported.")
    endif()

    set(cfg_file_path ${cfg_file_path}/${target})

    if(NOT EXISTS ${cfg_file_path})
        set(cfg_file_path ${CMAKE_CURRENT_LIST_DIR}/scripts/relinker/examples/${target})
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

    idf_build_get_property(link_depends __LINK_DEPENDS)

    add_custom_command(OUTPUT ${link_dst_file}
                        COMMAND ${python}    ${relinker_script}
                                --input      ${link_src_file}
                                --output     ${link_dst_file}
                                --library    ${library_file}
                                --object     ${object_file}
                                --function   ${function_file}
                                --sdkconfig  ${sdkconfig}
                                --objdump    ${cmake_objdump}
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
else()
    message(STATUS "Relinker isn't enabled.")
endif()
