idf_component_register(SRCS "bootloader_start.c"
                    REQUIRES bootloader bootloader_support)

idf_build_get_property(target IDF_TARGET)
# Use the linker script files from the actual bootloader.
# Since IDF v6.0 the bootloader subproject's own top-level CMakeLists.txt
# already applies bootloader.memory.ld.in/bootloader.sections.ld.in to the
# "main" component target unconditionally (regardless of this override), so
# re-adding them here would register the same ldgen preprocess target twice
# and fail the build. Only pre-6.0 IDF requires this override to supply the
# (then single, non-templated) bootloader.ld/bootloader.rom.ld explicitly.
if("${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}" VERSION_LESS "6.0")
    set(scripts "${IDF_PATH}/components/bootloader/subproject/main/ld/${target}/bootloader.ld"
                "${IDF_PATH}/components/bootloader/subproject/main/ld/${target}/bootloader.rom.ld")
    target_linker_script(${COMPONENT_LIB} INTERFACE "${scripts}")
endif()
