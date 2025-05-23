if("${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}" VERSION_LESS "5.0")
    set(ADC_REQ "driver")
else()
    set(ADC_REQ "esp_adc")
endif()

idf_component_register(SRC_DIRS "."
                       PRIV_INCLUDE_DIRS "."
                       PRIV_REQUIRES unity adc_tp_calibration test_utils ${ADC_REQ})
