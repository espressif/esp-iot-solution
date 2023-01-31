set(srcs "src/led_ledc.c"
        "src/led_gpio.c"
        "src/led_indicator.c"
        "src/led_indicator_blink_default.c"
        )

idf_component_register(SRCS ${srcs}
                        INCLUDE_DIRS include
                        REQUIRES driver)

include(cmake_utilities)
cu_pkg_define_version(${CMAKE_CURRENT_LIST_DIR})        