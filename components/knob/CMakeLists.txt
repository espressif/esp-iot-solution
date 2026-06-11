if("${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}" VERSION_GREATER_EQUAL "5.3")
    set(REQ esp_driver_gpio)
else()
    set(REQ driver)
endif()

set(SRC_FILES "iot_knob.c" "knob_gpio.c")

if(CONFIG_SOC_RTCIO_INPUT_OUTPUT_SUPPORTED)
    list(APPEND SRC_FILES "knob_rtc.c")
endif()

idf_component_register(SRCS ${SRC_FILES}
                       INCLUDE_DIRS "include"
                       REQUIRES ${REQ}
                       PRIV_REQUIRES esp_timer)

include(package_manager)
cu_pkg_define_version(${CMAKE_CURRENT_LIST_DIR})
