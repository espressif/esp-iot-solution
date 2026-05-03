if("${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}" VERSION_GREATER_EQUAL "5.3")
    set(REQ esp_driver_ledc esp_driver_gpio)
else()
    set(REQ driver)
endif()

idf_component_register(SRCS "iot_servo.c"
                        INCLUDE_DIRS include
                        REQUIRES ${REQ})
