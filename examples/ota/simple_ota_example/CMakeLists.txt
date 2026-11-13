# The following lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

# (Not part of the boilerplate)
# This example uses an extra component for common functions such as Wi-Fi and Ethernet connection.
set(EXTRA_COMPONENT_DIRS $ENV{IDF_PATH}/examples/common_components/protocol_examples_common)

# Use the in-tree bootloader_support_plus and cmake_utilities when this example
# is built inside the esp-iot-solution checkout. The main project search path
# and the BOOTLOADER_EXTRA_COMPONENT_DIRS property below feed the same two
# local components to the main project and to the bootloader sub-project, so
# the component manager never queries the registry for the not-yet-published
# >=0.8.0 / >=1.4.1 floors. The guards skip this for idf.py
# create-project-from-example copies, which resolve the published versions
# from the registry instead (after commenting out the override_path lines in
# the idf_component.yml files, as documented there).
set(_bsp_plus ${CMAKE_CURRENT_LIST_DIR}/../../../components/bootloader_support_plus)
set(_cmake_utils ${CMAKE_CURRENT_LIST_DIR}/../../../tools/cmake_utilities)
if(EXISTS ${_bsp_plus}/CMakeLists.txt AND EXISTS ${_cmake_utils}/CMakeLists.txt)
    list(APPEND EXTRA_COMPONENT_DIRS ${_bsp_plus} ${_cmake_utils})
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

if(EXISTS ${_bsp_plus}/CMakeLists.txt AND EXISTS ${_cmake_utils}/CMakeLists.txt)
    idf_build_set_property(BOOTLOADER_EXTRA_COMPONENT_DIRS "${_bsp_plus}" APPEND)
    idf_build_set_property(BOOTLOADER_EXTRA_COMPONENT_DIRS "${_cmake_utils}" APPEND)
endif()

project(simple_ota)
include(gen_compressed_ota)
include(gen_diff_ota)

# gen_diff_ota does not pad. If the packed patch is shorter than the 1024-byte
# image header esp_https_ota reads, print the gen_custom_ota.py command that does.
set(_diff_packed "${CMAKE_BINARY_DIR}/custom_ota_binaries/patch.xz.packed")
set(_diff_pack_script "${CMAKE_CURRENT_LIST_DIR}/../../../tools/cmake_utilities/scripts/gen_custom_ota.py")
add_custom_command(TARGET gen_diff_ota
    POST_BUILD
    COMMAND ${CMAKE_COMMAND}
        -DPACKED=${_diff_packed}
        -DSCRIPT=${_diff_pack_script}
        -DBASE=${DIFF_OTA_BASE_ABS}
        -DNEW=${DIFF_OTA_NEW_ABS}
        -P ${CMAKE_CURRENT_LIST_DIR}/check_diff_ota_size.cmake
    VERBATIM
)
