if("${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}" VERSION_GREATER_EQUAL "5.0")
    list(APPEND PRIVREQ esp_adc)
endif()

idf_component_register(SRC_DIRS "."
                       PRIV_REQUIRES esp_event
                       PRIV_INCLUDE_DIRS "."
                       PRIV_REQUIRES unity test_utils button ${PRIVREQ}
                       WHOLE_ARCHIVE)
