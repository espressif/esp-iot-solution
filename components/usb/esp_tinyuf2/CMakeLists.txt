idf_component_get_property(TUSB_PATH leeebo__tinyusb_src COMPONENT_DIR)
idf_component_get_property(DCD_CORE_INCLUDES leeebo__tinyusb_src DCD_CORE_INCLUDES)
idf_component_get_property(DCD_CORE_SRCS leeebo__tinyusb_src DCD_CORE_SRCS)
idf_component_get_property(TUSB_COMPILE_OPTIONS leeebo__tinyusb_src TUSB_COMPILE_OPTIONS)

message("TinyUSB path: ${TUSB_PATH}")
message("TinyUSB includes: ${DCD_CORE_INCLUDES}")
message("TinyUSB sources: ${DCD_CORE_SRCS}")
message("TinyUSB compile options: ${TUSB_COMPILE_OPTIONS}")

set(src_tusb
    "${DCD_CORE_SRCS}"
    "msc/msc.c"
    "usb_descriptors.c"
    "${TUSB_PATH}/src/class/cdc/cdc_device.c"
    "${TUSB_PATH}/src/class/msc/msc_device.c"
    )
set(inc_tusb "${DCD_CORE_INCLUDES}" "msc")

if(CONFIG_ENABLE_UF2_USB_CONSOLE)
list(APPEND src_tusb "cdc/cdc.c" "cdc/tusb_cdc_acm.c" "cdc/tusb_console.c" "cdc/vfs_tinyusb.c")
list(APPEND inc_tusb "cdc")
endif()

idf_component_register(
                    SRCS
                    "uf2/board_flash.c"
                    "uf2/ghostfat.c"
                    "esp_tinyuf2.c"
                    ${src_tusb}
                    INCLUDE_DIRS "." ${inc_tusb}
                    PRIV_INCLUDE_DIRS "private_include"
                    REQUIRES app_update
                    PRIV_REQUIRES usb nvs_flash vfs esp_ringbuf esp_pm)

if(NOT CONFIG_TINYUSB_SOURCE_CODE_ONLY)
message(FATAL_ERROR "Please disable 'Build tinyusb as a static library' in menuconfig 'TinyUSB soucre'")
endif()

target_compile_options(${COMPONENT_LIB} PUBLIC
    ${TUSB_COMPILE_OPTIONS}
)

execute_process(COMMAND git -C ${IDF_PATH} describe --dirty --always --tags
    OUTPUT_VARIABLE MY_IDF_VERSION
    ERROR_QUIET)
string(STRIP ${MY_IDF_VERSION} MY_IDF_VERSION)

set(GIT_IDF_VERSIONS "IDF:${MY_IDF_VERSION}")
idf_build_get_property(project_name PROJECT_NAME)
idf_build_get_property(project_version PROJECT_VER)
message("Project name: ${project_name}")
message("Project version: ${project_version}")

if(${CMAKE_VERSION} VERSION_LESS "3.12")
    message("Please consider to switch to CMake above 3.12")
    add_definitions(-DUF2_APP_NAME="${project_name}")
    add_definitions(-DUF2_APP_VERSION="${project_version}")
    add_definitions(-DUF2_GIT_IDF_VERSIONS="${GIT_IDF_VERSIONS}")
else()
    add_compile_definitions(UF2_APP_NAME="${project_name}")
    add_compile_definitions(UF2_APP_VERSION="${project_version}")
    add_compile_definitions(UF2_GIT_IDF_VERSIONS="${GIT_IDF_VERSIONS}")
endif()

get_filename_component(UF2_UTILS_PATH "./utils" ABSOLUTE)
set(ENV{UF2_UTILS_PATH} ${UF2_UTILS_PATH})

if (NOT TARGET uf2_ota)
add_custom_target (
    uf2-ota
    COMMAND ${CMAKE_COMMAND} -E echo "transforming ${PROJECT_BIN} to ${project_name}.uf2"
    COMMAND ${PYTHON} $ENV{UF2_UTILS_PATH}/uf2conv.py ${BUILD_DIR}/${PROJECT_BIN} -c -b 0x00 -f ${IDF_TARGET} -o ${PROJECT_DIR}/${project_name}.uf2
    COMMAND ${CMAKE_COMMAND} -E echo "transforming done"
    DEPENDS gen_project_binary
    VERBATIM USES_TERMINAL
)
endif()

include(package_manager)
cu_pkg_define_version(${CMAKE_CURRENT_LIST_DIR})
