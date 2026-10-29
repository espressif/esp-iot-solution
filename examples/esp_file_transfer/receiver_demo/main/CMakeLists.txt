set(FILE_TRANSFER_EXAMPLE_COMMON_DIR
    "${CMAKE_CURRENT_LIST_DIR}/../../common_components")

idf_component_register(
    SRCS
        "app_main.c"
        "${FILE_TRANSFER_EXAMPLE_COMMON_DIR}/file_transfer_example_common.c"
        "${FILE_TRANSFER_EXAMPLE_COMMON_DIR}/file_transfer_example_console.c"
        "${FILE_TRANSFER_EXAMPLE_COMMON_DIR}/file_transfer_example_storage.c"
    INCLUDE_DIRS
        "."
        "${FILE_TRANSFER_EXAMPLE_COMMON_DIR}"
    REQUIRES
        nvs_flash
        freertos
        bt
        console
        fatfs
        wear_levelling
        esp_gmp
        esp_flux
        ble_session_manager
)

fatfs_create_spiflash_image(storage ../storage FLASH_IN_PROJECT PRESERVE_TIME ONE_FAT)
