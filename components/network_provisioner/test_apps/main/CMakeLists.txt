idf_component_register(SRCS "network_provisioner_test_main.c"
                       INCLUDE_DIRS "."
                       PRIV_INCLUDE_DIRS "../../private_include" "../../proto-c"
                       PRIV_REQUIRES unity test_utils network_provisioner protocomm_ext
                                     protobuf-c esp_http_client)
