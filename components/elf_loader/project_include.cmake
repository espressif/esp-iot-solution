set(PY_PATH ${CMAKE_CURRENT_LIST_DIR})
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
    set(gen_dir "${build_dir}/generated/${app_name}")
    file(MAKE_DIRECTORY ${gen_dir})

    # Compile project files paths
    set(elf_app_dir "${CMAKE_CURRENT_BINARY_DIR}/elf_${app_name}")
    set(elf_file "${elf_app_dir}/${app_name}.app.elf")
    set(elf_output_bin "${build_dir}/${output_bin}.bin")

    # Script paths
    set(symbols_script "${PY_PATH}/tool/symbols.py")
    set(symbol_c_file "${gen_dir}/esp_${app_name}.c")

    # Add external project (to generate ELF)
    externalproject_add(${app_name}
        SOURCE_DIR ${app_path}
        INSTALL_COMMAND ""
        CMAKE_ARGS
            -DIDF_PATH=${idf_path}
            -DIDF_TARGET=${idf_target}
            -DSDKCONFIG=${app_path}/sdkconfig
        BINARY_DIR ${elf_app_dir}
        BUILD_BYPRODUCTS ${elf_file}
        BUILD_ALWAYS 1
        CMAKE_GENERATOR "Unix Makefiles"
        BUILD_COMMAND make elf
    )

    # Binary file generation
    add_custom_command(
        OUTPUT ${elf_output_bin}
        COMMAND ${CMAKE_COMMAND} -E copy ${elf_file} ${elf_output_bin}
        DEPENDS ${app_name}
        COMMENT "Generating BIN file"
    )

    # Symbol file generation
    add_custom_command(
        OUTPUT ${symbol_c_file}
        COMMAND ${Python_EXECUTABLE} ${symbols_script}
            --undefined
            --input ${elf_file}
            --type e
            --output ${symbol_c_file}
            --exclude elf_find_sym
        DEPENDS ${app_name}
        COMMENT "Generating ${app_name} symbol table"
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )

    # Combined build target
    add_custom_target(${app_name}_generate ALL
        DEPENDS ${symbol_c_file} ${elf_output_bin}
    )

    target_sources(${COMPONENT_LIB} PRIVATE ${symbol_c_file})

    add_dependencies(${COMPONENT_LIB} ${app_name}_generate)

    target_add_binary_data(${COMPONENT_LIB} ${elf_output_bin} BINARY)
endfunction()
