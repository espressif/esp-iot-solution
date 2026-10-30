# Inject plus/XZ input-section rules into the generated bootloader linker
# script, inside .iram_loader.text and before the later .iram.text wildcard.
#
# Invoked as: cmake -DBUILD_DIR=<bootloader build dir> -P inject_xz_loader_rules.cmake
#
# GNU ld assigns an input section to the first matching output rule. Placing
# these archive patterns in .iram_loader.text keeps the assignment independent
# of -T order (CMake v1 and v2).
#
# Script selected by what the bootloader actually links:
#   IDF 6.0.2+              bootloader.sections.ld
#   IDF 6.0/6.0.1           bootloader.ld
#   IDF 6.0/6.0.1, ESP32-P4 bootloader.rev3.ld (rev >= 3)
#   IDF 5.5.5+, ESP32-P4    a build-dir copy of the static bootloader.rev3.ld
#                           (P4_REV3_SOURCE). The SDK original is not modified.
# Prefer sections over rev3 over bootloader.ld. On ESP32-P4, rev3 wins over
# bootloader.ld so a rev < 3 script left in the build dir is not patched.

if(NOT DEFINED BUILD_DIR)
    message(FATAL_ERROR "inject_xz_loader_rules: BUILD_DIR is not set")
endif()

# ESP32-P4 on IDF 5.5.x links a static script that lives in the IDF tree.
# Refresh a build-dir copy first, then inject into that copy.
if(DEFINED P4_REV3_SOURCE AND NOT "${P4_REV3_SOURCE}" STREQUAL "")
    if(NOT EXISTS "${P4_REV3_SOURCE}")
        message(FATAL_ERROR
            "inject_xz_loader_rules: ESP32-P4 rev>=3 script not found: ${P4_REV3_SOURCE}")
    endif()
    file(MAKE_DIRECTORY "${BUILD_DIR}/ld")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E copy "${P4_REV3_SOURCE}" "${BUILD_DIR}/ld/bootloader.rev3.ld"
        RESULT_VARIABLE _bsp_copy_rc
    )
    if(NOT _bsp_copy_rc EQUAL 0)
        message(FATAL_ERROR
            "inject_xz_loader_rules: failed to copy ${P4_REV3_SOURCE} "
            "to ${BUILD_DIR}/ld/bootloader.rev3.ld")
    endif()
endif()

if(NOT DEFINED IDF_TARGET)
    set(IDF_TARGET "")
endif()

file(GLOB_RECURSE ld_files LIST_DIRECTORIES false "${BUILD_DIR}/*.ld")
set(section_scripts "")
set(rev3_scripts "")
set(legacy_scripts "")
foreach(ld_file IN LISTS ld_files)
    get_filename_component(ld_name "${ld_file}" NAME)
    if(ld_name STREQUAL "bootloader.sections.ld")
        list(APPEND section_scripts "${ld_file}")
    elseif(ld_name STREQUAL "bootloader.rev3.ld")
        list(APPEND rev3_scripts "${ld_file}")
    elseif(ld_name STREQUAL "bootloader.ld")
        list(APPEND legacy_scripts "${ld_file}")
    endif()
endforeach()

if(section_scripts)
    set(target_scripts "${section_scripts}")
elseif(rev3_scripts AND (IDF_TARGET STREQUAL "esp32p4" OR P4_REV3_SOURCE))
    set(target_scripts "${rev3_scripts}")
elseif(legacy_scripts)
    set(target_scripts "${legacy_scripts}")
elseif(rev3_scripts)
    set(target_scripts "${rev3_scripts}")
else()
    message(FATAL_ERROR
        "inject_xz_loader_rules: neither bootloader.sections.ld, "
        "bootloader.rev3.ld, nor bootloader.ld found under ${BUILD_DIR}")
endif()

# Bracket argument so the ';' characters stay inside the string.
set(rules [=[    _xz_decompressor_start = ABSOLUTE(.);
    *libbootloader_support_plus.a:(.literal .text .literal.* .text.*)
    *libespressif__bootloader_support_plus.a:(.literal .text .literal.* .text.*)
    *libespressif__xz.a:(.literal .text .literal.* .text.*)
    *libxz.a:(.literal .text .literal.* .text.*)
    _xz_decompressor_end = ABSOLUTE(.);
]=])

foreach(section_script IN LISTS target_scripts)
    file(READ "${section_script}" content)
    string(FIND "${content}" "_xz_decompressor_start" already_injected)
    if(NOT already_injected EQUAL -1)
        message(STATUS "inject_xz_loader_rules: already injected ${section_script}")
        continue()
    endif()

    string(REGEX MATCH "_loader_text_end[ \t]*=[ \t]*ABSOLUTE\\([.]\\)[ \t]*;" has_anchor "${content}")
    if(NOT has_anchor)
        message(FATAL_ERROR
            "inject_xz_loader_rules: _loader_text_end anchor not found in ${section_script}")
    endif()

    string(REGEX REPLACE
        "([ \t]*)_loader_text_end[ \t]*=[ \t]*ABSOLUTE\\([.]\\)[ \t]*;"
        "${rules}\\1_loader_text_end = ABSOLUTE(.);"
        content "${content}")
    file(WRITE "${section_script}" "${content}")
    message(STATUS "inject_xz_loader_rules: injected loader rules into ${section_script}")
endforeach()
