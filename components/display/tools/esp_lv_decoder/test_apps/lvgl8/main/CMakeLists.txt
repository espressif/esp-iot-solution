
idf_component_register(
    SRC_DIRS "."
    INCLUDE_DIRS ".")

# Mount SPIFFS for JPG/PNG tests
spiffs_create_partition_image(assets ../spiffs_assets FLASH_IN_PROJECT)

# Generate mmap assets from the same source assets
spiffs_create_partition_assets(qoi ../spiffs_assets FLASH_IN_PROJECT
    MMAP_FILE_SUPPORT_FORMAT ".jpg,.png"
    MMAP_SUPPORT_QOI
    IMPORT_INC_PATH "${CMAKE_CURRENT_SOURCE_DIR}"
)

spiffs_create_partition_assets(spng ../spiffs_assets FLASH_IN_PROJECT
    MMAP_FILE_SUPPORT_FORMAT ".png"
    MMAP_SUPPORT_SPNG
    MMAP_SPLIT_HEIGHT 1
    IMPORT_INC_PATH "${CMAKE_CURRENT_SOURCE_DIR}"
)

spiffs_create_partition_assets(sjpg ../spiffs_assets FLASH_IN_PROJECT
    MMAP_FILE_SUPPORT_FORMAT ".jpg"
    MMAP_SUPPORT_SJPG
    MMAP_SPLIT_HEIGHT 1
    IMPORT_INC_PATH "${CMAKE_CURRENT_SOURCE_DIR}"
)

spiffs_create_partition_assets(pjpg ../spiffs_assets FLASH_IN_PROJECT
    MMAP_FILE_SUPPORT_FORMAT ".png"
    MMAP_SUPPORT_PJPG
    IMPORT_INC_PATH "${CMAKE_CURRENT_SOURCE_DIR}"
)
