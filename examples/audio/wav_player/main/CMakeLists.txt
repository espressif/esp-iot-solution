idf_component_register(SRCS "file_manager.c" "app_main.c"
                    INCLUDE_DIRS .)

if(NOT CONFIG_STORAGE_SDCARD)
    spiffs_create_partition_image(audio ${CMAKE_CURRENT_SOURCE_DIR}/../spiffs_image FLASH_IN_PROJECT)
endif()