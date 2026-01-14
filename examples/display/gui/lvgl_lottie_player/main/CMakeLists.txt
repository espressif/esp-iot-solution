set(srcs "main.c")
set(include_dirs ".")

idf_component_register(
    SRCS ${srcs}
    INCLUDE_DIRS ${include_dirs}
    REQUIRES hw_init esp_lv_lottie_player
)

set(LOTTIE_URL "https://dl.espressif.com/AE/esp-dev-kits/welcome.json")
set(LOTTIE_FILE_NAME "welcome.json")
set(LOTTIE_ASSETS_DIR "${CMAKE_BINARY_DIR}/lottie_assets")
set(LOTTIE_ASSET_PATH "${LOTTIE_ASSETS_DIR}/${LOTTIE_FILE_NAME}")

file(MAKE_DIRECTORY ${LOTTIE_ASSETS_DIR})
message(STATUS "Downloading lottie asset from ${LOTTIE_URL}")
file(DOWNLOAD ${LOTTIE_URL} ${LOTTIE_ASSET_PATH} SHOW_PROGRESS)

if(EXISTS "${LOTTIE_ASSETS_DIR}")
    spiffs_create_partition_assets(lottie "${LOTTIE_ASSETS_DIR}" FLASH_IN_PROJECT
        MMAP_FILE_SUPPORT_FORMAT ".json"
        IMPORT_INC_PATH "${CMAKE_CURRENT_SOURCE_DIR}")
    add_dependencies(${COMPONENT_LIB} assets_lottie_bin)
endif()
