set(IMPORT_PATH "${CMAKE_SOURCE_DIR}")

idf_component_register(
    SRC_DIRS "."
    INCLUDE_DIRS "." "${IMPORT_PATH}"
)

set(DIR_SRC         "${PROJECT_DIR}/spiffs_assets")
set(DIR_INDEPEND    "${CMAKE_BINARY_DIR}/assert_independ")
set(DIR_APPEND      "${CMAKE_BINARY_DIR}/assert_append")

file(GLOB_RECURSE ASSET_FILES "${DIR_SRC}/*")

file(INSTALL ${ASSET_FILES} DESTINATION ${DIR_INDEPEND} USE_SOURCE_PERMISSIONS)
file(INSTALL ${ASSET_FILES} DESTINATION ${DIR_APPEND} USE_SOURCE_PERMISSIONS)

spiffs_create_partition_assets(
    assets
    ${DIR_INDEPEND}
    FLASH_IN_PROJECT
    MMAP_FILE_SUPPORT_FORMAT ".jpg,.png,.ttf"
)

spiffs_create_partition_assets(
    factory
    ${DIR_APPEND}
    FLASH_IN_PROJECT
    FLASH_APPEND_APP
    MMAP_FILE_SUPPORT_FORMAT ".jpg,.png,.ttf"
    IMPORT_INC_PATH "${IMPORT_PATH}"
)
