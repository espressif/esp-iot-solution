idf_component_register(
    SRC_DIRS "."
    INCLUDE_DIRS ".")

idf_component_get_property(lvgl_lib lvgl__lvgl COMPONENT_LIB)
idf_component_get_property(freetype_lib espressif__freetype COMPONENT_LIB)
cmake_policy(SET CMP0079 NEW)
target_link_libraries(${lvgl_lib} PUBLIC ${freetype_lib})

# Download the example font into a directory "spiffs_assets" in the build directory
set(URL "https://github.com/espressif/esp-docs/raw/f036a337d8bee5d1a93b2264ecd29255baec4260/src/esp_docs/fonts/DejaVuSans.ttf")
set(FILE "DejaVuSans.ttf")
set(SPIFFS_DIR "${CMAKE_BINARY_DIR}/spiffs_assets")
file(MAKE_DIRECTORY ${SPIFFS_DIR})
file(DOWNLOAD ${URL} ${SPIFFS_DIR}/${FILE} SHOW_PROGRESS)

spiffs_create_partition_assets(
    assets
    ${SPIFFS_DIR}
    FLASH_IN_PROJECT
    MMAP_FILE_SUPPORT_FORMAT ".ttf"
)
