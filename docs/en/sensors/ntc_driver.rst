**NTC_Driver**
==================

:link_to_translation:`zh_CN:[中文]`

NTC stands for Negative Temperature Coefficient, which is a type of thermistor with a negative temperature coefficient. Its main characteristic is that the resistance decreases with increasing temperature within the operating temperature range. By utilizing its basic resistance temperature characteristics, voltage and current characteristics, the NTC series thermistors have been widely used in household appliances,To achieve functions such as automatic gain adjustment, overload protection, temperature control, temperature compensation, voltage stabilization, and amplitude stabilization.

The common packaging/forms of NTC are as follows:
    1. Probe type NTC thermistor: suitable for various application scenarios, suitable for detecting extremely high or low temperatures
    2. SMT type NTC thermistor: one-time mounted on PCB, suitable for temperature compensation circuits, LED lighting temperature monitoring, battery pack temperature monitoring

NTC key parameters:
    1. Resistance specifications (resistance values of 1K, 5K, 10K, 50K, 100K, etc. at 25 ℃)
    2. Resistance accuracy (0.5%, 1%, 2%, 3%, 5%)
    3. Temperature range for use
    4. B value (material constant, the higher the B value, the higher the rate of resistance change)
    5. Probe shape, probe material, wire material, and wire length

NTC Driver Applicable Circuit:
    Single ADC channel measurement:
    When the NTC is on top, as the temperature increases, the resistance value of the NTC decreases, causing the voltage at the NTC terminal in the divider circuit to drop, and the voltage at the fixed resistor side rises, and the final output voltage changes accordingly. The opposite is true when the NTC is below, where a drop in the resistance value of the NTC causes the voltage at the NTC side of the divider circuit to rise, and a drop in the voltage at the fixed resistor side, resulting in a corresponding change in the output voltage. As a result, the NTC will get opposite results above and below, which requires special care when designing and using NTC measurement circuits to ensure that the temperature characteristics of the NTC are properly understood and handled.
    
    When using the following circuits:
    Vcc  --------> Rt  --------> Rref  --------> GND
    The corresponding circuit mode needs to be selected through the ```circuit_mode``` field in ```ntc_config_t```. When using this circuit mode, ```CIRCUIT_MODE_NTC_VCC``` should be used.

    When using the following circuits:
    Vcc  --------> Rref  --------> Rt  --------> GND
    The corresponding circuit mode needs to be selected through the ```circuit_mode``` field in ```ntc_config_t```. When using this circuit mode, ```CIRCUIT_MODE_NTC_GND``` should be used.

    Regarding the value of voltage divider resistor Rref:
        The resistance value of Rref is generally taken as the resistance value of Rt at 25 degrees Celsius

NTC digital temperature conversion parameter, formula is: Rt = R * EXP (B * (1/T1-1/T2))
    -Rt is the resistance of a thermistor at T1 temperature
    -R is the nominal resistance of a thermistor at room temperature at T2
    -B value is an important parameter of thermistors
    -EXP is the nth power of e
    -T1 and T2 refer to K degrees, which is the Kelvin temperature. K degrees = 273.15 (absolute temperature) + Celsius

Demonstration
---------------

Create an NTC thermistor drive
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code:: c

    // Create a ntc driver and register call-back
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

    ntc_device_handle_t ntc = NULL;
    adc_oneshot_unit_handle_t adc_handle = NULL;
    ESP_ERROR_CHECK(ntc_dev_create(&ntc_config, &ntc, &adc_handle));
    ESP_ERROR_CHECK(ntc_dev_get_adc_handle(ntc, &adc_handle));

    //get ntc temperature
    float temp = 0.0;
    if (ntc_dev_get_temperature(ntc, &temp) == ESP_OK) {
        ESP_LOGI(TAG, "NTC temperature = %.2f ℃", temp);
    }
    
    //delete handle
    TEST_ASSERT_EQUAL(ESP_OK, ntc_dev_delete(ntc));

API reference
---------------

The following API implements hardware abstraction for thermistor sensors. Users can directly call this layer of code to write sensor applications, or use the sensor interface in doc: 'sensor_hub<sensor_hub>' to achieve simpler calls.

.. include-build-file:: inc/ntc_driver.inc
