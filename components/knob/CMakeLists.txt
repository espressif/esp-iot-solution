if("${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}" VERSION_GREATER_EQUAL "5.3")
    set(REQ esp_driver_gpio)
else()
    set(REQ driver)
endif()

idf_component_register(SRCS "iot_knob.c" "knob_gpio.c"
                       INCLUDE_DIRS "include"
                       REQUIRES ${REQ}
                       PRIV_REQUIRES esp_timer)

include(package_manager)
cu_pkg_define_version(${CMAKE_CURRENT_LIST_DIR})
