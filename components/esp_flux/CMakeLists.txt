idf_component_register(SRCS "esp_flux_transport.c" "flux_gatt_session.c"
                        INCLUDE_DIRS include
                        PRIV_INCLUDE_DIRS private_include
                        REQUIRES freertos esp_timer bt)
