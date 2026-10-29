idf_component_register(
    SRCS "test_esp_lvgl_adapter_features.c"
    INCLUDE_DIRS "."
    REQUIRES hw_init unity
)

# Get target platform
idf_build_get_property(target IDF_TARGET)
if(NOT target)
    set(target ${IDF_TARGET})
endif()

set(assets_root "${CMAKE_CURRENT_LIST_DIR}/../assets")

# Create font assets partition (used by FreeType scenario)
if(EXISTS "${assets_root}/fonts")
    spiffs_create_partition_assets(fonts "${assets_root}/fonts" FLASH_IN_PROJECT
        MMAP_FILE_SUPPORT_FORMAT ".ttf"
        IMPORT_INC_PATH "${CMAKE_CURRENT_SOURCE_DIR}")
    add_dependencies(${COMPONENT_LIB} assets_fonts_bin)
endif()

# Create image assets partition (used by Image Decode scenario)
if(EXISTS "${assets_root}/images")
    spiffs_create_partition_assets(images "${assets_root}/images" FLASH_IN_PROJECT
        MMAP_FILE_SUPPORT_FORMAT ".jpg,.png"
        IMPORT_INC_PATH "${CMAKE_CURRENT_SOURCE_DIR}")
    add_dependencies(${COMPONENT_LIB} assets_images_bin)
endif()
