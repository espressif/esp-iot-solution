set(srcs
    "src/esp_xiaozhi_keystore.c"
    "src/esp_xiaozhi_info.c"
    "src/esp_xiaozhi_board.c"
    "src/esp_xiaozhi_payload.c"
    "src/esp_xiaozhi_mqtt.c"
    "src/esp_xiaozhi_websocket.c"
    "src/esp_xiaozhi_transport.c"
    "src/esp_xiaozhi_mcp.c"
    "src/esp_xiaozhi_chat.c"
    "src/esp_xiaozhi_vision.c"
    "src/esp_xiaozhi_video.c"
    "src/esp_xiaozhi_camera.c"
)

idf_component_register(
    SRCS "${srcs}"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "src/priv_include"
    PRIV_REQUIRES  esp_common nvs_flash app_update spi_flash json mbedtls mqtt esp_http_client mcp-c-sdk esp_websocket_client
)
