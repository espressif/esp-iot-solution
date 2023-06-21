# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(usb_uf2_ota)

add_custom_command(
    TARGET gen_project_binary POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo "transforming ${PROJECT_BIN} to ${PROJECT_NAME}.uf2"
    COMMAND ${PYTHON} $ENV{UF2_UTILS_PATH}/uf2conv.py ${BUILD_DIR}/${PROJECT_BIN} -c -b 0x00 -f ${IDF_TARGET} -o ${PROJECT_DIR}/${PROJECT_NAME}.uf2
    COMMAND ${CMAKE_COMMAND} -E echo "transforming done"
    DEPENDS gen_project_binary
    VERBATIM USES_TERMINAL
)