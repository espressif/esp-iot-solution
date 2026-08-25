idf_component_register(SRCS "bootloader_start.c"
                    REQUIRES bootloader bootloader_support)

idf_build_get_property(target IDF_TARGET)
# Use the linker script files from the actual bootloader.
if("${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}" VERSION_LESS "6.0")
    # Pre-6.0 IDF ships a single, non-templated bootloader.ld/bootloader.rom.ld
    # pair that the default "main" component's own CMakeLists.txt would
    # normally attach; since this component replaces "main" entirely, supply
    # them explicitly.
    set(scripts "${IDF_PATH}/components/bootloader/subproject/main/ld/${target}/bootloader.ld"
                "${IDF_PATH}/components/bootloader/subproject/main/ld/${target}/bootloader.rom.ld")
else()
    # IDF >= 6.0, Build System v1: bootloader/subproject/CMakeLists.txt attaches
    # bootloader.memory.ld.in/bootloader.sections.ld.in directly to the
    # "__idf_main" target regardless of which component is registered as
    # "main", so BOOTLOADER_LINKER_SCRIPT is unset here and this is a no-op.
    # IDF >= 6.0, Build System v2: CMakeLists_v2.txt only stores the scripts in
    # the BOOTLOADER_LINKER_SCRIPT property and expects the "main" component to
    # apply them itself (mirroring subproject/main/CMakeLists.txt), so fetch
    # and apply them here to cover that case.
    idf_build_get_property(scripts BOOTLOADER_LINKER_SCRIPT)
endif()
target_linker_script(${COMPONENT_LIB} INTERFACE "${scripts}")

