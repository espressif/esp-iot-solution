if("${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}" VERSION_GREATER_EQUAL "5.3")
    set(PRIV_REQ "esp_driver_gpio")
else()
    set(PRIV_REQ "driver")
endif()

idf_component_register(SRC_DIRS "." "hal"
                        INCLUDE_DIRS "include"
                        REQUIRES "esp_event" "esp_timer"
                        PRIV_REQUIRES ${PRIV_REQ}
                        LDFRAGMENTS linker.lf)
