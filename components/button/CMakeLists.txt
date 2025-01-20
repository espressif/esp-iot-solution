set(PRIVREQ esp_timer)
set(REQ driver)
set(SRC_FILES "button_gpio.c" "iot_button.c" "button_matrix.c")

if("${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}" VERSION_GREATER_EQUAL "5.0")
    list(APPEND REQ esp_adc)
    if(CONFIG_SOC_ADC_SUPPORTED)
        list(APPEND SRC_FILES "button_adc.c")
    endif()
endif()

idf_component_register(SRCS ${SRC_FILES}
                        INCLUDE_DIRS include interface
                        REQUIRES ${REQ}
                        PRIV_REQUIRES ${PRIVREQ})

include(package_manager)
cu_pkg_define_version(${CMAKE_CURRENT_LIST_DIR})
