idf_component_register(
    SRC_DIRS "src"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "src"
    PRIV_REQUIRES esp_mm
)

idf_build_get_property(build_components BUILD_COMPONENTS)
if(lvgl IN_LIST build_components)
    set(lvgl_name lvgl)
    set(lvgl_ver $ENV{LVGL_VERSION})
else()
    set(lvgl_name lvgl__lvgl)
    idf_component_get_property(lvgl_ver ${lvgl_name} COMPONENT_VERSION)
endif()

if("${lvgl_ver}" STREQUAL "")
    message(WARNING "esp_lv_eaf_player: LVGL version not detected, assuming v9.x")
endif()

message(STATUS "esp_lv_eaf_player: LVGL version: ${lvgl_ver}")

set(extra_libs idf::${lvgl_name})

# Conditionally link software JPEG decoder based on Kconfig
if(CONFIG_ESP_LV_EAF_ENABLE_SW_JPEG)
    if("esp_new_jpeg" IN_LIST build_components)
        list(APPEND extra_libs idf::esp_new_jpeg)
        message(STATUS "esp_lv_eaf_player: Software JPEG decoder enabled (esp_new_jpeg)")
    endif()
    if("espressif__esp_new_jpeg" IN_LIST build_components)
        list(APPEND extra_libs idf::espressif__esp_new_jpeg)
        message(STATUS "esp_lv_eaf_player: Software JPEG decoder enabled (espressif__esp_new_jpeg)")
    endif()
else()
    message(STATUS "esp_lv_eaf_player: Software JPEG decoder DISABLED (saves ~19KB DRAM)")
endif()

# Add hardware JPEG decoder support for chips that have it
idf_build_get_property(target IDF_TARGET)
if(target STREQUAL "esp32p4")
    list(APPEND extra_libs idf::esp_driver_jpeg)
    message(STATUS "esp_lv_eaf_player: Hardware JPEG decoder enabled for ${target}")
endif()

idf_component_get_property(lvgl_dir ${lvgl_name} COMPONENT_DIR)
message(STATUS "esp_lv_eaf_player: lvgl component dir: ${lvgl_dir}")
if(EXISTS "${lvgl_dir}")
    target_include_directories(${COMPONENT_LIB} PRIVATE "${lvgl_dir}")
endif()

target_link_libraries(${COMPONENT_LIB} PUBLIC ${extra_libs})

include(package_manager)
cu_pkg_define_version(${CMAKE_CURRENT_LIST_DIR})
