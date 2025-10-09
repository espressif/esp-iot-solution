set(srcs "src/led_indicator_ledc.c"
        "src/led_gamma.c"
        "src/led_indicator_gpio.c"
        "src/led_indicator.c"
        "src/led_indicator_blink_default.c"
        "src/led_indicator_rgb.c"
        "src/led_indicator_strips.c"
        "src/led_convert.c"
        )

if("${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}" VERSION_GREATER_EQUAL "5.3")
    set(REQ esp_driver_ledc esp_driver_gpio)
else()
    set(REQ driver)
endif()

idf_component_register( SRCS ${srcs}
                        INCLUDE_DIRS "include"
                        PRIV_INCLUDE_DIRS "private_include"
                        REQUIRES ${REQ})

include(package_manager)
cu_pkg_define_version(${CMAKE_CURRENT_LIST_DIR})
