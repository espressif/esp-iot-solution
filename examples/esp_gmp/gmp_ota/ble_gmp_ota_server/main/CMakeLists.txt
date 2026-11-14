idf_component_register(
    SRCS "main.c" "${CMAKE_CURRENT_LIST_DIR}/../../../../esp_flux/common_components/ble_link_gate/ble_link_gate.c"
    INCLUDE_DIRS "." "${CMAKE_CURRENT_LIST_DIR}/../../../../esp_flux/common_components/ble_link_gate"
    REQUIRES nvs_flash freertos bt esp_gmp esp_flux ble_conn_mgr gmp_ota_common
)
