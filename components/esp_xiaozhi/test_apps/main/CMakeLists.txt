idf_component_register(SRC_DIRS "."
                       PRIV_INCLUDE_DIRS "." "../../src/priv_include"
                       PRIV_REQUIRES unity test_utils esp_xiaozhi mcp-c-sdk nvs_flash esp_event esp_netif cjson esp_timer mbedtls
                       WHOLE_ARCHIVE)
