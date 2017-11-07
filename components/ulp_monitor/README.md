# Component: Ulp_monitor

* This component provides some apis for running simple ulp program in deep sleep.

* A ulp_monitor component can provide:
    * read adc values from register during deep sleep
    * read temprature sensor value during deep sleep
    * store the values in RTC slow memory
    * set the sampling rate
    * wake up the chip if the value exceeds the threshold
    * wake up the chip if appointed numbers of value are sampled

* Follow these steps to use ulp_monitor:
    * make menuconfig: Component config -> ESP32-specific -> choose "Enable Ultra Low Power (ULP) Coprocessor"
    * call iot_ulp_monitor_init() to set program address and data address
    * call adc1_config_width() and adc1_config_channel_atten() to config adc channel
    * call iot_ulp_add_adc_monitor() or(and) ulp_add_temperature_monitor() to add adc or temperature snesor sampling(they can be added at the same time and more than one adc channels can be added)
    * call iot_ulp_monitor_start() to run the ulp program and set the measurement period
    * call esp_deep_sleep_start() to enter deep sleep