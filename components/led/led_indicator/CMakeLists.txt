set(srcs "src/led_ledc.c"
        "src/led_gamma.c"
        "src/led_gpio.c"
        "src/led_indicator.c"
        "src/led_indicator_blink_default.c"
        "src/led_rgb.c"
        "src/led_strips.c"
        "src/led_convert.c"
        )

idf_component_register( SRCS ${srcs}
                        INCLUDE_DIRS "include"
                        PRIV_INCLUDE_DIRS "private_include"
                        REQUIRES driver)

include(package_manager)
cu_pkg_define_version(${CMAKE_CURRENT_LIST_DIR})
