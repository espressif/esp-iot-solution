# The script is to generate ELF for application

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
    string(REPLACE "-elf-gcc" "-elf-strip" ${CMAKE_STRIP} ${CMAKE_C_COMPILER})
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
    if(ELF_COMPONENTS)
        foreach(c ${ELF_COMPONENTS})
            list(APPEND elf_libs "esp-idf/${c}/lib${c}.a")

            if(${CMAKE_GENERATOR} STREQUAL "Unix Makefiles")
                add_custom_command(OUTPUT elf_${c}_app
                        COMMAND +${CMAKE_MAKE_PROGRAM} "__idf_${c}/fast"
                        COMMENT "Build Component: ${c}"
                )
                list(APPEND elf_dependeces "elf_${c}_app")
            else()
                list(APPEND elf_dependeces "idf::${c}")
            endif()
        endforeach()
    endif()
    if (ELF_LIBS)
        list(APPEND elf_libs "${ELF_LIBS}")
    endif()
    spaces2list(elf_libs)

    add_custom_command(OUTPUT elf_app
        COMMAND ${CMAKE_C_COMPILER} ${cflags} ${elf_libs} -o ${elf_app}
        COMMAND ${CMAKE_STRIP} ${strip_flags} ${elf_app}
        DEPENDS ${elf_dependeces}
        COMMENT "Build ELF: ${elf_app}"
        )
    add_custom_target(elf ALL DEPENDS elf_app)
endmacro()
