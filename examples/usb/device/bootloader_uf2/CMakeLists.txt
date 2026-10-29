# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(bootloader_uf2)

idf_build_get_property(project_bin PROJECT_BIN)
if(NOT project_bin)
    set(project_bin "${PROJECT_NAME}.bin")
endif()

set(esptoolpy_cmd ${ESPTOOLPY})
if(NOT esptoolpy_cmd)
    idf_component_get_property(esptoolpy_cmd esptool_py ESPTOOLPY_CMD)
endif()

add_custom_command(
    TARGET app
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo "Merge: bootloader_merge.bin to address ${CONFIG_BOOTLOADER_OFFSET_IN_FLASH}"
    COMMAND ${esptoolpy_cmd} merge_bin -o ${BUILD_DIR}/bootloader/bootloader_merge.bin ${CONFIG_BOOTLOADER_OFFSET_IN_FLASH} ${BUILD_DIR}/bootloader/bootloader.bin 0x10000 ${BUILD_DIR}/${project_bin}
    # Check if the merged bin file is smaller than the configured offset address.
    COMMAND sh ${CMAKE_SOURCE_DIR}/utils/redirect.sh ${BUILD_DIR}/bootloader/bootloader_merge.bin ${CONFIG_PARTITION_TABLE_OFFSET}
    VERBATIM
)
