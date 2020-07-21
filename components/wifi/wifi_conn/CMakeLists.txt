idf_component_register(SRCS "wifi.c" "wifi_obj.cpp"
                        INCLUDE_DIRS include
                        PRIV_REQUIRES nvs_flash)