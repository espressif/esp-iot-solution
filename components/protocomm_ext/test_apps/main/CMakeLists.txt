idf_component_register(SRC_DIRS "."
                       PRIV_INCLUDE_DIRS "."
                       PRIV_REQUIRES unity test_utils protocomm_ext esp_http_client esp_driver_uart bt
                                     nvs_flash esp_netif esp_event
                       WHOLE_ARCHIVE)
