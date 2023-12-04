# @brief Link designated functions from SRAM to Flash to save SRAM
if(CONFIG_GPROF_ENABLE)
    message(STATUS "GProf is enabled, the following components will be profiling: ${CONFIG_GPROF_COMPONENTS}")

    idf_build_get_property(link_depends __LINK_DEPENDS)

    set(CONFIG_GPROF_COMPONENTS ${CONFIG_GPROF_COMPONENTS})
	list(JOIN CONFIG_GPROF_COMPONENTS " " GPROF_COMPONENTS_STRING)
    foreach (c ${CONFIG_GPROF_COMPONENTS})
        target_compile_options(__idf_${c} PRIVATE -pg)
    endforeach()

    set(gprof_script "${CMAKE_CURRENT_LIST_DIR}/scripts/gprof.py")

    set(link_path "${CMAKE_BINARY_DIR}/esp-idf/esp_system/ld")
    set(link_src_file "${link_path}/sections.ld")
    set(link_dst_file "${link_path}/customer_sections.ld")

    set(relinker_opts relink --input       ${link_src_file}
                             --output      ${link_dst_file}
                             --components  ${GPROF_COMPONENTS_STRING})

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
    add_dependencies(${project_elf} customer_sections)

    set(gprof_file "${CMAKE_BINARY_DIR}/${CMAKE_PROJECT_NAME}.gprof.out")

	list(JOIN ESPTOOLPY " " ESPTOOLPY_STR)
    set(gprof_opts dump --esptool       ${ESPTOOLPY_STR}
                        --partition_csv ${PARTITION_CSV_PATH}
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
