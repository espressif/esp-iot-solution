idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES nvs_flash freertos bt esp_gmp esp_flux ble_session_manager gmp_ota_common spiffs
)

set(OTA_IMAGE "${CMAKE_CURRENT_SOURCE_DIR}/../spiffs/ota_image.bin")
spiffs_create_partition_image(storage ../spiffs FLASH_IN_PROJECT)
if(NOT EXISTS "${OTA_IMAGE}")
    message(WARNING "ble_gmp_ota_client: spiffs/ota_image.bin missing; OTA upload will be skipped at runtime")
endif()
