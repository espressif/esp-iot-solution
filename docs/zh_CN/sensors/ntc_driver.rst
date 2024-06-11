**热敏电阻传感器**
====================

:link_to_translation:`en:[English]`

NTC 是 Negative Temperature Coefficient 的缩写， NTC 热敏电阻是一种具有负温度系数的热敏电阻,其主要的特点是在工作温度范围内电阻随温度的升高而降低。利用其基本的电阻温度特性、电压电流特性， NTC 系列热敏电阻已广泛应用于家用电器产品中，以达到自动增益调整、过负荷保护、温度控制、温度补偿、稳压稳幅等作用。

NTC 常见的封装/形态如下：
    1. 探针式 NTC 热敏电阻：适合多种应用场景，适合极高温或极低温探测
    2. 贴片式 NTC 热敏电阻：一次性贴装在 PCB 上，适合温度补偿电路， LED 照明温度监测，电池组温度监测

NTC 关键参数：
    1. 阻值规格(25℃ 阻值 1K,5K,10K,50K,100K 等)
    2. 阻值精度(0.5%,1%,2%,3%,5%)
    3. 使用温度范围
    4. B 值(材料常数， B 值越高电阻变化率越高)
    5. 探头外形、探头材料、导线材料、导线长度

NTC Driver 适用电路：
    单 ADC 通道测量：
    当NTC在上面时,随着温度升高， NTC 的电阻值下降，导致分压电路中 NTC 端的电压下降，固定电阻端的电压上升，最终输出电压会随之变化。而当 NTC 在下面时，情况正好相反， NTC 的电阻值下降会导致分压电路中 NTC 端的电压上升，固定电阻端的电压下降，输出电压也会相应变化。因此， NTC 在上面和下面会得到相反的结果，这一点在设计和使用 NTC 测量电路时需要特别注意，确保正确理解和处理 NTC 的温度特性。
    
    当使用以下电路时：
    Vcc  --------> Rt  --------> Rref  --------> GND
    需要通过 ```ntc_config_t``` 中的 ```circuit_mode``` 字段选择相应的电路模式。当使用这种电路模式时，应该使用 ```CIRCUIT_MODE_NTC_VCC```。

    当使用以下电路时：
    Vcc  --------> Rref  --------> Rt  --------> GND
    需要通过 ```ntc_config_t``` 中的 ```circuit_mode``` 字段选择相应的电路模式。当使用这种电路模式时，应该使用 ```CIRCUIT_MODE_NTC_GND```。

    关于分压电阻 Rref 取值：
        Rref 的阻值一般取 Rt 在 25 摄氏度的阻值

NTC 数字温度转换参数，公式为: Rt = R * EXP (B * (1/T1-1/T2))
    -Rt 是热敏电阻在 T1 温度下的电阻
    -R 是室温下热敏电阻在 T2 时的标称电阻
    -B 值是热敏电阻的一个重要参数
    -EXP 是 e 的 n 次方
    -T1 和 T2 是指 K 度，即开尔文温度， K 度 = 273.15 （绝对温度）+ 摄氏度

应用示例
--------

创建ntc热敏电阻驱动
^^^^^^^^^^^^^^^^^^^^^

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

API 参考
----------

以下 API 实现了对热敏电阻传感器的硬件抽象，用户可直接调用该层代码编写传感器应用程序，也可以使用 :doc:`sensor_hub <sensor_hub>` 中的传感器接口实现更简单的调用。

.. include-build-file:: inc/ntc_driver.inc
