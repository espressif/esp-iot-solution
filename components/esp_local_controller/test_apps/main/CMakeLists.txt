idf_component_register(SRCS "esp_local_controller_test_main.c"
                       INCLUDE_DIRS "."
                       PRIV_INCLUDE_DIRS "../../private_include" "../../proto-c"
                       PRIV_REQUIRES unity test_utils esp_local_controller protocomm_ext
                                     protobuf-c esp_http_client)
