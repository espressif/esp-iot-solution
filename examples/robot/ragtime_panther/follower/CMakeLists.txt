# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/spiffs)
if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/spiffs/merged-binary.bin)
    file(WRITE ${CMAKE_CURRENT_SOURCE_DIR}/spiffs/merged-binary.bin "")
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

set(CXX_COMPILE_OPTIONS
    -Wno-deprecated-enum-enum-conversion
    -Wno-write-strings
)

add_compile_options(
    -Wno-format
    -Wno-unused-variable
    -Wno-missing-field-initializers
    -Wno-unused-but-set-variable
    -Wno-cast-function-type
    -Wno-ignored-qualifiers
)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated-enum-enum-conversion")

project(ragtime_panthera)


add_custom_command(
    OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/spiffs/merged-binary.bin
    COMMAND bash -c "cd ${CMAKE_CURRENT_SOURCE_DIR}/receiver && idf.py set-target esp32c6 && idf.py build && idf.py merge-bin"
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_SOURCE_DIR}/spiffs
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/receiver/build/merged-binary.bin ${CMAKE_CURRENT_SOURCE_DIR}/spiffs/merged-binary.bin
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Building receiver with target esp32c6 and copying merged-binary.bin to spiffs"
    VERBATIM
)

add_custom_target(build_receiver ALL
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/spiffs/merged-binary.bin
)
