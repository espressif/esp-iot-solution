set(src_cdc)
set(inc_cdc)
if(CONFIG_ENABLE_UF2_USB_CONSOLE)
set(src_cdc "cdc/cdc.c" "cdc/tusb_cdc_acm.c" "cdc/tusb_console.c" "cdc/vfs_tinyusb.c")
set(inc_cdc "cdc")
endif()

idf_component_register(
                    SRCS
                    "uf2/board_flash.c"
                    "uf2/ghostfat.c"
                    "msc/msc.c"
                    "msc/usb_descriptors.c"
                    "esp_tinyuf2.c"
                    ${src_cdc}
                    INCLUDE_DIRS "." "msc" ${inc_cdc}
                    PRIV_INCLUDE_DIRS "private_include"
                    REQUIRES app_update
                    PRIV_REQUIRES usb nvs_flash vfs esp_ringbuf)

idf_component_get_property(tusb_lib leeebo__tinyusb_src COMPONENT_LIB)
cmake_policy(SET CMP0079 NEW)
target_link_libraries(${tusb_lib} PRIVATE ${COMPONENT_LIB})

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