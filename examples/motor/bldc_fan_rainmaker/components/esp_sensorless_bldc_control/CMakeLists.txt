if(NOT CONFIG_SOC_MCPWM_SUPPORTED)
    set(exclude_srcs  "hardware_driver/bldc_mcpwm.c" "control/bldc_zero_cross_adc.c")
endif()


idf_component_register(SRC_DIRS "." "common" "control" "hardware_driver"
                       INCLUDE_DIRS "." "include" "common/include" "control/include" "hardware_driver/include"
                       EXCLUDE_SRCS ${exclude_srcs}
                       REQUIRES driver esp_timer esp_event esp_adc)
