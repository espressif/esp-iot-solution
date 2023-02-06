idf_build_get_property(project_dir PROJECT_DIR)
idf_component_register(SRCS "app_main.c"
                       EMBED_TXTFILES ${project_dir}/rsa_key/private.pem)


create_esp_enc_img(${CMAKE_BINARY_DIR}/${CMAKE_PROJECT_NAME}.bin
    ${project_dir}/rsa_key/private.pem ${CMAKE_BINARY_DIR}/${CMAKE_PROJECT_NAME}_secure.bin app)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-unused-const-variable)
