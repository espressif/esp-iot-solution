if("${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}" VERSION_GREATER_EQUAL "5.5")
    set(DRIVER_REQUIRES esp_driver_gpio esp_driver_rmt)
else()
    set(DRIVER_REQUIRES driver)
endif()

idf_component_register(SRC_DIRS "."
                       PRIV_INCLUDE_DIRS "."
                       PRIV_REQUIRES unity test_utils nvs_flash ir_learn ${DRIVER_REQUIRES})
