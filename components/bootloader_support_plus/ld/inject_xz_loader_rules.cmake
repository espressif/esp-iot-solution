# Inject plus/XZ input-section rules into the generated bootloader linker
# script, inside .iram_loader.text and before the later .iram.text wildcard.
#
# Invoked as: cmake -DBUILD_DIR=<bootloader build dir> -P inject_xz_loader_rules.cmake
#
# GNU ld assigns an input section to the first matching output rule. Placing
# these archive patterns in .iram_loader.text keeps the assignment independent
# of -T order (CMake v1 and v2).
#
# IDF v6.0 and v6.0.1 preprocess bootloader.ld.in into bootloader.ld.
# IDF v6.0.2 and newer split that into bootloader.sections.ld. Prefer the
# split script when both names are present so a stale bootloader.ld is ignored.

if(NOT DEFINED BUILD_DIR)
    message(FATAL_ERROR "inject_xz_loader_rules: BUILD_DIR is not set")
endif()

file(GLOB_RECURSE ld_files LIST_DIRECTORIES false "${BUILD_DIR}/*.ld")
set(section_scripts "")
set(legacy_scripts "")
foreach(ld_file IN LISTS ld_files)
    get_filename_component(ld_name "${ld_file}" NAME)
    if(ld_name STREQUAL "bootloader.sections.ld")
        list(APPEND section_scripts "${ld_file}")
    elseif(ld_name STREQUAL "bootloader.ld")
        list(APPEND legacy_scripts "${ld_file}")
    endif()
endforeach()

if(section_scripts)
    set(target_scripts "${section_scripts}")
elseif(legacy_scripts)
    set(target_scripts "${legacy_scripts}")
else()
    message(FATAL_ERROR
        "inject_xz_loader_rules: neither bootloader.sections.ld nor bootloader.ld "
        "found under ${BUILD_DIR}")
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
