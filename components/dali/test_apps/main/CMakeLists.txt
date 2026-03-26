idf_component_register(SRCS "dali_test.c"
                            "dali_test_main.c"
                        INCLUDE_DIRS "."
                        PRIV_REQUIRES unity dali
                                      esp_timer
                                      esp_netif
                                      esp_wifi
                                      esp_event
                                      esp_http_client
                                      esp_https_ota
                                      nvs_flash
                        WHOLE_ARCHIVE)
