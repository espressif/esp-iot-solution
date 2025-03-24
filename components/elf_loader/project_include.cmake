set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR} ${CMAKE_MODULE_PATH})

# elf_embed_binary
#
# Create a c file corresponding to ELF and embed into the application.
function(elf_embed_binary app_name app_path output_bin)
    idf_build_get_property(idf_path IDF_PATH)
    idf_build_get_property(idf_target IDF_TARGET)
    idf_build_get_property(build_dir BUILD_DIR)

    # Clean and set up external project paths
    file(REMOVE ${app_path}/sdkconfig)
    set(elf_app_dir "${build_dir}/elf_${app_name}")
    set(elf_file "${elf_app_dir}/${app_name}.app.elf")
    set(elf_output_bin "${build_dir}/${output_bin}.bin")

    # Add external project (to generate ELF)
    externalproject_add(${app_name}
        SOURCE_DIR ${app_path}
        INSTALL_COMMAND ""
        CMAKE_ARGS
            -DIDF_PATH=${idf_path}
            -DIDF_TARGET=${idf_target}
            -DSDKCONFIG=${elf_app_dir}/sdkconfig
        BINARY_DIR ${elf_app_dir}
        BUILD_BYPRODUCTS ${elf_file}
        BUILD_ALWAYS 1
    )

    add_custom_command(
        OUTPUT ${elf_output_bin}
        COMMAND ${CMAKE_COMMAND} -E copy
                "${elf_file}"
                "${elf_output_bin}"
        DEPENDS ${app_name}
    )

    target_add_binary_data(${COMPONENT_LIB} ${elf_output_bin} BINARY)
endfunction()
