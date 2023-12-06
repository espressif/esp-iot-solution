idf_component_register(SRCS ./src/qrcodegen.c ./src/esp_qrcode_main.c ./src/esp_qrcode_wrapper.c
                       INCLUDE_DIRS include
                       REQUIRES
                       PRIV_REQUIRES "rmaker_common")
