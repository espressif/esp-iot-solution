set(PRIVREQ esp_timer)

if("${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}" VERSION_GREATER_EQUAL "5.0")
    list(APPEND REQ esp_adc)
else() 
    list(APPEND REQ esp_adc_cal)
endif()

idf_component_register(SRCS "button_adc.c" "button_gpio.c" "iot_button.c"
                        INCLUDE_DIRS include
                        REQUIRES driver ${REQ}
                        PRIV_REQUIRES ${PRIVREQ})
