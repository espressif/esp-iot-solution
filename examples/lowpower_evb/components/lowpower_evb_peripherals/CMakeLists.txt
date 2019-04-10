
set(COMPONENT_SRCS "lowpower_evb_adc.cpp"
                    "lowpower_evb_epaper.cpp"
                    "lowpower_evb_power.cpp"
                    "lowpower_evb_sensor.cpp"
                    "lowpower_evb_status_led.cpp")

set(COMPONENT_ADD_INCLUDEDIRS ". include")

set(COMPONENT_REQUIRES esp_adc_cal epaper hts221 ulp led bh1750)

register_component()
