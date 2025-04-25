
idf_component_register(SRCS app_wifi.c
                    INCLUDE_DIRS "." "include"
                    PRIV_REQUIRES esp_wifi esp_timer nvs_flash lwip)
target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-format")
