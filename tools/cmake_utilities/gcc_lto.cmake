if(CONFIG_CU_GCC_LTO_ENABLE) 
    # Enable cmake interprocedural optimization(IPO) support to check if GCC supports link time optimization(LTO)
    cmake_policy(SET CMP0069 NEW)
    include(CheckIPOSupported)

    # Compare to "ar" and "ranlib", "gcc-ar" and "gcc-ranlib" integrate GCC LTO plugin
    set(CMAKE_AR ${_CMAKE_TOOLCHAIN_PREFIX}gcc-ar)
    set(CMAKE_RANLIB ${_CMAKE_TOOLCHAIN_PREFIX}gcc-ranlib)

    macro(cu_gcc_lto_set)
        check_ipo_supported(RESULT result)
        if(result)
            message(STATUS "GCC link time optimization(LTO) is enable")

            set(multi_value COMPONENTS DEPENDS)
            cmake_parse_arguments(LTO "" "" "${multi_value}" ${ARGN})

            # Use full format LTO object file
            set(GCC_LTO_OBJECT_TYPE         "-ffat-lto-objects")
            # Set compression level 9(min:0, max:9)
            set(GCC_LTO_COMPRESSION_LEVEL   "-flto-compression-level=9")
            # Set partition level max to removed used symbol 
            set(GCC_LTO_PARTITION_LEVEL     "-flto-partition=max")

            # Set mode "auto" to increase compiling speed
            set(GCC_LTO_COMPILE_OPTIONS     "-flto=auto"
                                            ${GCC_LTO_OBJECT_TYPE}
                                            ${GCC_LTO_COMPRESSION_LEVEL})

            # Enable GCC LTO and plugin when linking stage
            set(GCC_LTO_LINK_OPTIONS        "-flto"
                                            "-fuse-linker-plugin"
                                            ${GCC_LTO_OBJECT_TYPE}
                                            ${GCC_LTO_PARTITION_LEVEL})

            message(STATUS "GCC LTO for components: ${LTO_COMPONENTS}")
            foreach(c ${LTO_COMPONENTS})
                idf_component_get_property(t ${c} COMPONENT_LIB)
                target_compile_options(${t} PRIVATE ${GCC_LTO_COMPILE_OPTIONS})
            endforeach()

            message(STATUS "GCC LTO for dependencies: ${LTO_DEPENDS}")
            foreach(d ${LTO_DEPENDS})
                target_compile_options(${d} PRIVATE ${GCC_LTO_COMPILE_OPTIONS})
            endforeach()

            target_link_libraries(${project_elf} PRIVATE ${GCC_LTO_LINK_OPTIONS})
        else()
            message(FATAL_ERROR "GCC link time optimization(LTO) is not supported")
        endif()
    endmacro()
else()
    macro(cu_gcc_lto_set)
        message(STATUS "GCC link time optimization(LTO) is not enable")
    endmacro()
endif()
