if("${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}" VERSION_GREATER_EQUAL "5.0")
    idf_component_register(SRCS "zero_detection.c"
                        REQUIRES driver esp_timer
                        INCLUDE_DIRS "include"
                        REQUIRED_IDF_TARGETS esp32 esp32s3 esp32c6 esp32h2)
else()
    message(FATAL_ERROR "Not supported in the current version")
endif()