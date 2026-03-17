set(srcs
    "fs_backend.c"
    "main.c"
)

idf_component_register(
    SRCS ${srcs}
    INCLUDE_DIRS "."
    REQUIRES example_lvgl_init fatfs spiffs littlefs
)

set(example_assets_dir "${CMAKE_CURRENT_LIST_DIR}/../fs_assets")

if(EXISTS "${example_assets_dir}")
    if(CONFIG_EXAMPLE_FS_SPIFFS)
        spiffs_create_partition_image(storage_spiffs "${example_assets_dir}" FLASH_IN_PROJECT)
    elseif(CONFIG_EXAMPLE_FS_FATFS)
        fatfs_create_spiflash_image(storage_fatfs "${example_assets_dir}" FLASH_IN_PROJECT PRESERVE_TIME ONE_FAT)
    elseif(CONFIG_EXAMPLE_FS_LITTLEFS)
        littlefs_create_partition_image(storage_littlefs "${example_assets_dir}" FLASH_IN_PROJECT)
    endif()
endif()
