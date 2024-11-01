
idf_component_register(
    SRC_DIRS "." "./assets"
    INCLUDE_DIRS ".")

spiffs_create_partition_image(assets ../spiffs_assets FLASH_IN_PROJECT)
