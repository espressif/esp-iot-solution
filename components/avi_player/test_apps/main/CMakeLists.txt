idf_component_register(SRC_DIRS "."
                       INCLUDE_DIRS ".")

spiffs_create_partition_image(avi ${CMAKE_CURRENT_SOURCE_DIR}/../spiffs FLASH_IN_PROJECT)
