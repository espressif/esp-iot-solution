# NTC driver Component

[中文版](./README_CN.md)

The NTC driver components are packaged for different thermistor initializations and corresponding temperature readings, and these solutions are managed using an abstraction layer for developers to integrate into their own applications, which are now supported by the ESP32-C2 / ESP32-C3 / ESP32 / ESP32-C6 / ESP32-S3 family of chips.

NTC Driver Applicable Circuit:
    Single ADC channel measurement:
    When the NTC is on top, as the temperature increases, the resistance value of the NTC decreases, causing the voltage at the NTC terminal in the divider circuit to drop, and the voltage at the fixed resistor side rises, and the final output voltage changes accordingly. The opposite is true when the NTC is below, where a drop in the resistance value of the NTC causes the voltage at the NTC side of the divider circuit to rise, and a drop in the voltage at the fixed resistor side, resulting in a corresponding change in the output voltage. As a result, the NTC will get opposite results above and below, which requires special care when designing and using NTC measurement circuits to ensure that the temperature characteristics of the NTC are properly understood and handled.

    When using the following circuits:
    Vcc  --------> Rt  --------> Rref  --------> GND
    The corresponding circuit mode needs to be selected through the ```circuit_mode``` field in ```ntc_config_t```. When using this circuit mode, ```CIRCUIT_MODE_NTC_VCC``` should be used.

    When using the following circuits:
    Vcc  --------> Rref  --------> Rt  --------> GND
    The corresponding circuit mode needs to be selected through the ```circuit_mode``` field in ```ntc_config_t```. When using this circuit mode, ```CIRCUIT_MODE_NTC_GND``` should be used.

> `Rref` is the voltage dividing resistor; `Rt` is the resistance of the thermistor at T1 temperature.

## Example of NTC thermistor scheme usage

```c
    //1.Select the NTC sensor and initialize the hardware parameters
    ntc_config_t ntc_config = {
        .b_value = 3950,
        .r25_ohm = 10000,
        .fixed_ohm = 10000,
        .vdd_mv = 3300,
        .circuit_mode = CIRCUIT_MODE_NTC_GND,
        .atten = ADC_ATTEN_DB_11,
        .channel = ADC_CHANNEL_3,
        .unit = ADC_UNIT_1
    };

    //2.Create the NTC Driver and Init ADC
    ntc_device_handle_t ntc = NULL;
    adc_oneshot_unit_handle_t adc_handle = NULL;
    ESP_ERROR_CHECK(ntc_dev_create(&ntc_config, &ntc, &adc_handle));
    ESP_ERROR_CHECK(ntc_dev_get_adc_handle(ntc, &adc_handle));

    //3.Call the temperature function
    float temp = 0.0;
    if (ntc_dev_get_temperature(ntc, &temp) == ESP_OK) {
        ESP_LOGI(TAG, "NTC temperature = %.2f ℃", temp);
    }
    ESP_ERROR_CHECK(ntc_dev_delete(ntc));
```

## Sample code

[Click here](https://github.com/espressif/esp-iot-solution/tree/master/examples/sensors/ntc_temperature_sensor) Get sample code and instructions for use.
