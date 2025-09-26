set(srcs "main.c")
set(include_dirs ".")

idf_component_register(
    SRCS ${srcs}
    INCLUDE_DIRS ${include_dirs}
    REQUIRES hw_init
)

idf_build_get_property(target IDF_TARGET)
if(NOT target)
    set(target ${IDF_TARGET})
endif()

# Download the font file from the internet into the build directory
set(FONT_URL "https://github.com/espressif/esp-docs/raw/f036a337d8bee5d1a93b2264ecd29255baec4260/src/esp_docs/fonts/DejaVuSans.ttf")
set(FONT_FILE "DejaVuSans.ttf")
set(FONT_ASSETS_DIR "${CMAKE_BINARY_DIR}/font_assets")

# Create directory and download font
file(MAKE_DIRECTORY ${FONT_ASSETS_DIR})
message(STATUS "Downloading font from ${FONT_URL}")
file(DOWNLOAD ${FONT_URL} ${FONT_ASSETS_DIR}/${FONT_FILE} SHOW_PROGRESS)

# Create font partition from downloaded file
if(EXISTS "${FONT_ASSETS_DIR}")
    spiffs_create_partition_assets(fonts "${FONT_ASSETS_DIR}" FLASH_IN_PROJECT
        MMAP_FILE_SUPPORT_FORMAT ".ttf"
        IMPORT_INC_PATH "${CMAKE_CURRENT_SOURCE_DIR}")
    add_dependencies(${COMPONENT_LIB} assets_fonts_bin)
endif()
