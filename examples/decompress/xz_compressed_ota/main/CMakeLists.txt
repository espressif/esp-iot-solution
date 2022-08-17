# Embed the server root certificate into the final binary
idf_build_get_property(project_dir PROJECT_DIR)
set(srcs "compressed_ota_example.c")
set(embed_file)

if(CONFIG_EXAMPLE_HTTP_DOWNLOAD)
    list(APPEND srcs "download_with_http.c")
    list(APPEND embed_file ${project_dir}/server_certs/ca_cert.pem)
else()
    list(APPEND srcs "download_with_ble.c")
endif()

idf_component_register(SRCS "${srcs}"
                    INCLUDE_DIRS "." "../bootloader_components/include"
                    EMBED_TXTFILES "${embed_file}")
