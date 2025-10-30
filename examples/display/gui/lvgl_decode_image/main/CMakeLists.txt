set(srcs "main.c")
set(include_dirs ".")

idf_component_register(
    SRCS ${srcs}
    INCLUDE_DIRS ${include_dirs}
    REQUIRES hw_init
)

idf_build_get_property(target IDF_TARGET)
if(NOT target)
    set(target ${IDF_TARGET})
endif()

set(assets_root "${CMAKE_CURRENT_LIST_DIR}/../assets")
set(asset_targets)

if(EXISTS "${assets_root}")
    if(NOT target STREQUAL "esp32c3")
        spiffs_create_partition_assets(jpg "${assets_root}/jpg" FLASH_IN_PROJECT
            MMAP_FILE_SUPPORT_FORMAT ".jpg"
            IMPORT_INC_PATH "${CMAKE_CURRENT_SOURCE_DIR}")
        list(APPEND asset_targets assets_jpg_bin)

        spiffs_create_partition_assets(png "${assets_root}/png" FLASH_IN_PROJECT
            MMAP_FILE_SUPPORT_FORMAT ".png"
            IMPORT_INC_PATH "${CMAKE_CURRENT_SOURCE_DIR}")
        list(APPEND asset_targets assets_png_bin)

        spiffs_create_partition_assets(qoi "${assets_root}/png" FLASH_IN_PROJECT
            MMAP_FILE_SUPPORT_FORMAT ".png"
            MMAP_SUPPORT_QOI
            IMPORT_INC_PATH "${CMAKE_CURRENT_SOURCE_DIR}")
        list(APPEND asset_targets assets_qoi_bin)
    endif()

    spiffs_create_partition_assets(spng "${assets_root}/png" FLASH_IN_PROJECT
        MMAP_FILE_SUPPORT_FORMAT ".png"
        MMAP_SUPPORT_SPNG
        MMAP_SPLIT_HEIGHT 16
        IMPORT_INC_PATH "${CMAKE_CURRENT_SOURCE_DIR}")
    list(APPEND asset_targets assets_spng_bin)

    spiffs_create_partition_assets(sjpg "${assets_root}/jpg" FLASH_IN_PROJECT
        MMAP_FILE_SUPPORT_FORMAT ".jpg"
        MMAP_SUPPORT_SJPG
        MMAP_SPLIT_HEIGHT 16
        IMPORT_INC_PATH "${CMAKE_CURRENT_SOURCE_DIR}")
    list(APPEND asset_targets assets_sjpg_bin)

    if(target STREQUAL "esp32p4")
        spiffs_create_partition_assets(pjpg "${assets_root}/png" FLASH_IN_PROJECT
            MMAP_FILE_SUPPORT_FORMAT ".png"
            MMAP_SUPPORT_PJPG
            IMPORT_INC_PATH "${CMAKE_CURRENT_SOURCE_DIR}")
        list(APPEND asset_targets assets_pjpg_bin)
    endif()
endif()

foreach(asset_target ${asset_targets})
    add_dependencies(${COMPONENT_LIB} ${asset_target})
endforeach()
