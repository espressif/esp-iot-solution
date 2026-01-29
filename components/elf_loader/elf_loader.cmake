# The script is to generate ELF or shared library for application
#
# Cache this file's directory so macros invoked from other CMakeLists
# can still reference companion scripts in this folder.
set(ELF_LOADER_CMAKE_DIR "${CMAKE_CURRENT_LIST_DIR}")

# Trick to temporarily redefine project(). When functions are overridden in CMake, the originals can still be accessed
# using an underscore prefixed function of the same name. The following lines make sure that project  calls
# the original project(). See https://cmake.org/pipermail/cmake/2015-October/061751.html.
function(project_elf)
endfunction()

function(_project_elf)
endfunction()

macro(project_elf project_name)
    # Enable these options to remove unused symbols and reduce linked objects
    set(cflags -nostartfiles
               -nostdlib
               -fPIC
               -shared
               -e app_main
               -fdata-sections
               -ffunction-sections
               -Wl,--gc-sections
               -fvisibility=hidden)

    # Enable this options to remove unnecessary sections in
    list(APPEND cflags -Wl,--strip-all
                       -Wl,--strip-debug
                       -Wl,--strip-discarded)

    list(APPEND cflags -Dmain=app_main)

    idf_build_set_property(COMPILE_OPTIONS "${cflags}" APPEND)

    set(elf_app "${CMAKE_PROJECT_NAME}.app.elf")

    # Remove more unused sections
    string(REPLACE "-elf-gcc" "-elf-strip" CMAKE_STRIP ${CMAKE_C_COMPILER})
    set(strip_flags --strip-unneeded
                    --remove-section=.comment
                    --remove-section=.got.loc
                    --remove-section=.dynamic)

    if(CONFIG_IDF_TARGET_ARCH_XTENSA)
        list(APPEND strip_flags --remove-section=.xt.lit
                                --remove-section=.xt.prop
                                --remove-section=.xtensa.info)
    elseif(CONFIG_IDF_TARGET_ARCH_RISCV)
        list(APPEND strip_flags --remove-section=.riscv.attributes)
    endif()

    # Link input list of libraries to ELF
    list(PREPEND ELF_COMPONENTS "main")
    set(elf_libs "")
    set(elf_dependencies "")
    if(ELF_COMPONENTS)
        foreach(c ${ELF_COMPONENTS})
            list(APPEND elf_libs "esp-idf/${c}/lib${c}.a")
            if(${CMAKE_GENERATOR} STREQUAL "Unix Makefiles")
                add_custom_command(OUTPUT elf_${c}_app
                        COMMAND +${CMAKE_MAKE_PROGRAM} "__idf_${c}/fast"
                        COMMENT "Build Component: ${c}"
                )
                list(APPEND elf_dependencies "elf_${c}_app")
            else()
                list(APPEND elf_dependencies "idf::${c}")
            endif()
        endforeach()
    endif()
    if (ELF_LIBS)
        list(APPEND elf_libs "${ELF_LIBS}")
    endif()
    spaces2list(elf_libs)

    # Define how to build the ELF file
    add_custom_command(
        OUTPUT ${elf_app}
        COMMAND ${CMAKE_C_COMPILER} ${cflags} ${elf_libs} -o ${elf_app}
        COMMAND ${CMAKE_STRIP} ${strip_flags} ${elf_app}
        DEPENDS ${elf_dependencies}
        COMMENT "Build ELF: ${elf_app}"
    )

    # Create a custom target to generate the ELF file
    add_custom_target(elf ALL DEPENDS ${elf_app})
endmacro()

# Trick to temporarily redefine project(). When functions are overridden in CMake, the originals can still be accessed
# using an underscore prefixed function of the same name. The following lines make sure that project  calls
# the original project(). See https://cmake.org/pipermail/cmake/2015-October/061751.html.
function(project_so)
endfunction()

function(_project_so)
endfunction()

macro(project_so project_name)
    if(CONFIG_ELF_DYNAMIC_LOAD_SHARED_OBJECT)
        # Compile flags for building component sources into position-independent .o files
        set(so_compile_flags -c
                             -fPIC
                             -DCONFIG_ELF_DYNAMIC_LOAD_SHARED_OBJECT)

        # Link flags for producing a shared object from the collected .o files.
        # This setup favors size reduction (gc-sections, hidden visibility) and
        # passes strip flags to the linker to remove unneeded content.
        set(so_link_flags -shared
                          -fPIC
                          -static-libgcc
                          -nostdlib
                          -nostartfiles
                          #-Wl,-E
                          #-Wl,--export-dynamic
                          -fdata-sections
                          -ffunction-sections
                          -Wl,--gc-sections
                          -fvisibility=hidden)

        list(APPEND so_link_flags -Wl,--strip-all
                                  -Wl,--strip-debug
                                  -Wl,--strip-discarded)
        # Output file name for shared object
        set(so_output "${CMAKE_PROJECT_NAME}.so")

        # Strip flags for final .so file: remove unneeded symbols and selected sections
        # to reduce size. The .dynamic section is also removed to match current loader flow.
        string(REPLACE "-elf-gcc" "-elf-strip" CMAKE_STRIP_SO ${CMAKE_C_COMPILER})
        set(so_strip_flags --strip-unneeded
                           --remove-section=.comment
                           --remove-section=.got.loc
                           --remove-section=.dynamic)

        if(CONFIG_IDF_TARGET_ARCH_XTENSA)
            list(APPEND so_strip_flags --remove-section=.xt.lit
                                       --remove-section=.xt.prop
                                       --remove-section=.xtensa.info)
        elseif(CONFIG_IDF_TARGET_ARCH_RISCV)
            list(APPEND so_strip_flags --remove-section=.riscv.attributes)
        endif()

        # Get ESP-IDF compile definitions and include directories
        idf_build_get_property(build_dir BUILD_DIR)
        idf_build_get_property(idf_path IDF_PATH)
        idf_build_get_property(target IDF_TARGET)

        # Collect all C source files from components
        list(PREPEND ELF_COMPONENTS "main")
        set(so_c_sources "")
        set(so_dependencies "")
        set(so_obj_files "")
        set(so_link_libs "")

        if(ELF_COMPONENTS)
            foreach(c ${ELF_COMPONENTS})
                # Get component directory
                idf_component_get_property(component_dir ${c} COMPONENT_DIR)

                # Find all C source files in component directory
                file(GLOB_RECURSE component_sources
                     CONFIGURE_DEPENDS
                     "${component_dir}/*.c")

                # Filter out test files and build files
                foreach(src ${component_sources})
                    # Skip files in test directories and build directories
                    if(NOT src MATCHES "/(test|tests|testing)/" AND
                       NOT src MATCHES "/test_[^/]*\\.c$" AND
                       NOT src MATCHES "${build_dir}")
                        list(APPEND so_c_sources ${src})
                    endif()
                endforeach()

                # Ensure component is built first (to get dependencies resolved)
                if(${CMAKE_GENERATOR} STREQUAL "Unix Makefiles")
                    add_custom_command(OUTPUT so_${c}_app
                            COMMAND +${CMAKE_MAKE_PROGRAM} "__idf_${c}/fast"
                            COMMENT "Build Component: ${c}"
                    )
                    list(APPEND so_dependencies "so_${c}_app")
                else()
                    list(APPEND so_dependencies "idf::${c}")
                endif()
            endforeach()
        endif()

        # Handle ELF_LIBS if specified (for external sources and libraries)
        if(ELF_LIBS)
            foreach(lib ${ELF_LIBS})
                if(lib MATCHES "\\.c$")
                    # Direct C file
                    get_filename_component(lib_abs ${lib} ABSOLUTE BASE_DIR ${CMAKE_SOURCE_DIR})
                    list(APPEND so_c_sources ${lib_abs})
                elseif(lib MATCHES "\\.(a|o|so)$")
                    # Linkable library/object
                    list(APPEND so_link_libs ${lib})
                else()
                    message(WARNING "ELF_LIBS entry '${lib}' has unsupported extension and will be ignored")
                endif()
            endforeach()
            spaces2list(so_link_libs)
        endif()

        # Get ESP-IDF compile definitions and includes from config
        idf_build_get_property(compile_defs COMPILE_DEFINITIONS)
        idf_build_get_property(include_dirs COMPILE_INCLUDE_DIRECTORIES)

        # Build include flags as a list
        set(include_flags "")
        foreach(inc_dir ${include_dirs})
            list(APPEND include_flags "-I${inc_dir}")
        endforeach()

        # Build definition flags as a list
        set(def_flags "")
        foreach(def ${compile_defs})
            list(APPEND def_flags "-D${def}")
        endforeach()

        # Compile each C file to .o file
        set(so_obj_dir "${CMAKE_BINARY_DIR}/so_objs")
        foreach(c_file ${so_c_sources})
            # Create a unique object file name based on relative path
            file(RELATIVE_PATH c_file_rel ${CMAKE_SOURCE_DIR} ${c_file})
            string(REPLACE "/" "_" obj_file_name ${c_file_rel})
            string(REPLACE ".c" ".o" obj_file_name ${obj_file_name})
            set(obj_file "${so_obj_dir}/${obj_file_name}")
            list(APPEND so_obj_files ${obj_file})

            # Compile C file to .o file with ESP-IDF includes and definitions
            # Use list variables directly, not strings
            add_custom_command(OUTPUT ${obj_file}
                    COMMAND ${CMAKE_COMMAND} -E make_directory ${so_obj_dir}
                    COMMAND ${CMAKE_C_COMPILER} ${so_compile_flags} ${def_flags} ${include_flags} ${c_file} -o ${obj_file}
                    DEPENDS ${c_file}
                    COMMENT "Compile ${c_file} to ${obj_file_name}"
                    )
        endforeach()

        # Link all .o files to .so file
        # Use list variables directly for link flags
        add_custom_command(OUTPUT ${so_output}
                # Undefined symbols are allowed to support modules resolved at runtime.
                COMMAND ${CMAKE_C_COMPILER} ${so_link_flags} -o ${so_output} ${so_obj_files} ${so_link_libs} -Wl,--allow-shlib-undefined
                COMMAND ${CMAKE_COMMAND} -DOUT=${so_output} -P ${ELF_LOADER_CMAKE_DIR}/check_shared_object.cmake
                COMMAND ${CMAKE_COMMAND} -E echo "Linking ${so_output} completed"
                COMMAND ${CMAKE_STRIP_SO} ${so_strip_flags} ${so_output}
                DEPENDS ${so_dependencies} ${so_obj_files}
                COMMENT "Build Shared Object: ${so_output}"
                WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
                )
        add_custom_target(so ALL DEPENDS ${so_output})
    endif()
endmacro()
