
set(srcs "lowpower_evb_adc.cpp"
                    "lowpower_evb_epaper.cpp"
                    "lowpower_evb_power.cpp"
                    "lowpower_evb_sensor.cpp"
                    "lowpower_evb_status_led.cpp")

idf_component_register(SRCS "${srcs}"
                        INCLUDE_DIRS "." "include"
                        REQUIRES  esp_adc_cal epaper i2c_bus hts221 ulp led bh1750)