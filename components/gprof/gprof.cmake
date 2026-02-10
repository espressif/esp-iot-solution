# @brief Define support targets for GProf
if(CONFIG_GPROF_ENABLE)
    message(STATUS "GProf is enabled, the following components will be profiled: ${CONFIG_GPROF_COMPONENTS}")

    idf_build_get_property(link_depends __LINK_DEPENDS)

    set(CONFIG_GPROF_COMPONENTS ${CONFIG_GPROF_COMPONENTS})
	list(JOIN CONFIG_GPROF_COMPONENTS " " GPROF_COMPONENTS_STRING)
    foreach (component_name ${CONFIG_GPROF_COMPONENTS})
        idf_component_get_property(component_target ${component_name} COMPONENT_LIB)
        target_compile_options(${component_target} PRIVATE -pg)
    endforeach()

    set(gprof_script "${CMAKE_CURRENT_LIST_DIR}/scripts/gprof.py")

    set(link_path "${CMAKE_BINARY_DIR}/esp-idf/esp_system/ld")
    set(link_src_file "${link_path}/sections.ld")
    set(link_dst_file "${link_path}/customer_sections.ld")

    set(relinker_opts relink --input       ${link_src_file}
                             --output      ${link_dst_file}
                             --components  ${GPROF_COMPONENTS_STRING})

    idf_build_get_property(python PYTHON)

    add_custom_command(OUTPUT  ${link_dst_file}
                       COMMAND ${python} -B ${gprof_script}
                               ${relinker_opts}
                       COMMAND ${CMAKE_COMMAND} -E copy
                               ${link_dst_file}
                               ${link_src_file}
                       COMMAND ${CMAKE_COMMAND} -E echo
                                /*gprof*/ >>
                                ${link_dst_file}
                       DEPENDS "${link_depends}"
                       VERBATIM)

    add_custom_target(customer_sections DEPENDS ${link_dst_file})
    idf_build_get_property(${project_elf} EXECUTABLE)
    add_dependencies(${project_elf} customer_sections)

    set(gprof_file "${CMAKE_BINARY_DIR}/${CMAKE_PROJECT_NAME}.gprof.out")

    partition_table_get_partition_info(gprof_partition_offset "--partition-type data --partition-subtype 0x3a" "offset")
    if (NOT gprof_partition_offset)
        message(FATAL_ERROR "Gprof partition (type: data, subtype: 0x3a) is not found in the partition table")
    endif()

    list(APPEND gprof_opts
        dump
        --partition-offset ${gprof_partition_offset}
        --elf           ${project_elf}
        --output        ${gprof_file}
        --gcc           ${CMAKE_C_COMPILER})

    add_custom_target(gprof
        DEPENDS ${project_elf}
        COMMAND ${python} -B ${gprof_script} ${gprof_opts})

    add_custom_target(gprof-graphic
        DEPENDS ${project_elf}
        COMMAND ${python} -B ${gprof_script} ${gprof_opts} --graphic)
else()
    message(STATUS "GProf isn't enabled.")
endif()
