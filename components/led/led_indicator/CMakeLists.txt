set(srcs "src/led_indicator_ledc.c"
        "src/led_gamma.c"
        "src/led_indicator_gpio.c"
        "src/led_indicator.c"
        "src/led_indicator_rgb.c"
        "src/led_indicator_strips.c"
        "src/led_convert.c"
        )
if(CONFIG_USE_MI_BLINK_DEFAULT)
    list(APPEND srcs     "src/mi_led_indicator_blink_default.c")
else()
    list(APPEND srcs     "src/led_indicator_blink_default.c")
endif()

idf_component_register( SRCS ${srcs}
                        INCLUDE_DIRS "include"
                        PRIV_INCLUDE_DIRS "private_include"
                        REQUIRES driver)

include(package_manager)
cu_pkg_define_version(${CMAKE_CURRENT_LIST_DIR})
