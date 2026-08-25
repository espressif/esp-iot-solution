# Embed the server root certificate into the final binary
idf_build_get_property(project_dir PROJECT_DIR)
idf_component_register(SRCS "simple_ota_example.c"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES esp_event app_update esp_http_client esp_https_ota
                                  protocol_examples_common mbedtls nvs_flash esp_wifi lwip
                    EMBED_TXTFILES ${project_dir}/server_certs/ca_cert.pem)
