idf_component_register(
    SRC_DIRS
        "."
        "assets"
    INCLUDE_DIRS
        "."
    )

    idf_component_get_property(lvgl_lib lvgl__lvgl COMPONENT_LIB)
    idf_component_get_property(freetype_lib espressif__freetype COMPONENT_LIB)
    cmake_policy(SET CMP0079 NEW)
    target_link_libraries(${lvgl_lib} PUBLIC ${freetype_lib})

spiffs_create_partition_image(storage ../spiffs FLASH_IN_PROJECT)
