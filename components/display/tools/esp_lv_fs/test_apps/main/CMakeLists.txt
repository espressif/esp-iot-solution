
idf_component_register(
    SRC_DIRS "."
    INCLUDE_DIRS ".")

spiffs_create_partition_assets(
    assets_A
    ../Drive_A
    FLASH_IN_PROJECT
    MMAP_FILE_SUPPORT_FORMAT ".jpg")

spiffs_create_partition_assets(
    assets_B
    ../Drive_B
    FLASH_IN_PROJECT
    MMAP_FILE_SUPPORT_FORMAT ".jpg")
