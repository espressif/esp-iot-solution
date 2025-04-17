# ADC Tow-Point Calibration Component

``adc_tp_calibration`` is a component that performs two-point ADC calibration for ESP32 and ESP32-S2 chips at the application level. By loading calibration parameters in the application layer, it enables easy implementation of two-point ADC calibration. This component offers the following features:

1. Supports inputting calibration parameters at the application level, allowing users to store the calibration data in storage media such as NVS or SD cards.
2. Supports the two-point calibration scheme for ESP32 and the Method 2 two-point calibration scheme for ESP32-S2, without interfering with the existing calibration scheme provided by ESP-IDF.

## How to Configure Calibration Parameters

You can refer to the calibration principles described in the [documentation](https://docs.espressif.com/projects/esp-iot-solution/en/latest/others/adc_tp_calibration.html) to obtain the calibration parameters required by the adc_tp_calibration component.

Note that when collecting the raw ADC data needed for calibration, it is recommended to use an external 100 nF filtering capacitor and apply software filtering to ensure stable ADC readings.

## Add component to your project

Please use the component manager command `add-dependency` to add the `adc_tp_calibration` to your project's dependency, during the `CMake` step the component will be downloaded automatically

```
idf.py add-dependency "espressif/adc_tp_calibration=*"
```

Alternatively, you can create `idf_component.yml`. More is in [Espressif's documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/tools/idf-component-manager.html).

## How to use

Since the calibration scheme is closely related to the ADC attenuation setting, please ensure that the attenuation used by the ``adc_tp_calibration`` component matches the one configured in the ADC driver.

```c

// Step1: Initializing Calibration Parameters

#if CONFIG_IDF_TARGET_ESP32
adc_tp_cali_config_t tp_cali_config = {
    .adc_unit = ADC_UNIT_1,
    .adc_raw_value_150mv_atten0 = 323,
    .adc_raw_value_850mv_atten0 = 3300,
};
#elif CONFIG_IDF_TARGET_ESP32S2
adc_tp_cali_config_t tp_cali_config = {
    .adc_unit = ADC_UNIT_1,
    .adc_raw_value_600mv_atten0 = 5895,
    .adc_raw_value_800mv_atten2_5 = 5786,
    .adc_raw_value_1000mv_atten6 = 5820,
    .adc_raw_value_2000mv_atten12 = 6287,
};
#endif

// Step2: Initializing Two-Point ADC Calibration

adc_tp_cali_handle_t adc_tp_cali = adc_tp_cali_create(&tp_cali_config, ADC_ATTEN_DB_0);

// Step3: Initialize the ADC driver and acquire raw ADC data.
...

// Step4: Calibrate ADC Data
adc_tp_cali_raw_to_voltage(adc_tp_cali, raw_value, &voltage)
```

Please note that the calibration data used in the above process must be determined by the user based on the [documentation](https://docs.espressif.com/projects/esp-iot-solution/en/latest/others/adc_tp_calibration.html). This data is not universal and must be obtained through manual calibration.

## Reference

[Documentation](https://docs.espressif.com/projects/esp-iot-solution/en/latest/others/adc_tp_calibration.html)
