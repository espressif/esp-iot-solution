set(PRIVREQ esp_timer)
set(REQ driver)
set(SRC_FILES "button_gpio.c" "iot_button.c" "button_matrix.c")

if("${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}" VERSION_GREATER_EQUAL "5.0")
    list(APPEND REQ esp_adc)
    if(CONFIG_SOC_ADC_SUPPORTED)
        list(APPEND SRC_FILES "button_adc.c")
    endif()
else()
    list(APPEND REQ esp_adc_cal)
    list(APPEND SRC_FILES "button_adc.c")
endif()

idf_component_register(SRCS ${SRC_FILES}
                        INCLUDE_DIRS include
                        REQUIRES ${REQ}
                        PRIV_REQUIRES ${PRIVREQ})

if("${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}" VERSION_LESS  "5.0")
    # Add the macro CONFIG_SOC_ADC_SUPPORTED for the following chips.
    if(CONFIG_IDF_TARGET STREQUAL "esp32" OR
       CONFIG_IDF_TARGET STREQUAL "esp32s2" OR
       CONFIG_IDF_TARGET STREQUAL "esp32s3" OR
       CONFIG_IDF_TARGET STREQUAL "esp32c3" OR
       CONFIG_IDF_TARGET STREQUAL "esp32h2")
       target_compile_definitions(${COMPONENT_LIB} PUBLIC CONFIG_SOC_ADC_SUPPORTED)
    endif()
endif()

include(package_manager)
cu_pkg_define_version(${CMAKE_CURRENT_LIST_DIR})
