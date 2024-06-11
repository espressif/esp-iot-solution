# NTC 驱动组件概述

[English Version](./README.md)

ntc驱动组件针对于不同热敏电阻初始化以及相应的温度读取做了封装，使用一个抽象层管理这些方案，便于开发者集成到自己的应用程序中，目前 ESP32-C2 / ESP32-C3 / ESP32 / ESP32-C6 / ESP32-S3 系列芯片都已经完成支持。

NTC Driver 适用电路：
    单 ADC 通道测量：
    当NTC在上面时,随着温度升高， NTC 的电阻值下降，导致分压电路中 NTC 端的电压下降，固定电阻端的电压上升，最终输出电压会随之变化。而当 NTC 在下面时，情况正好相反， NTC 的电阻值下降会导致分压电路中 NTC 端的电压上升，固定电阻端的电压下降，输出电压也会相应变化。因此， NTC 在上面和下面会得到相反的结果，这一点在设计和使用 NTC 测量电路时需要特别注意，确保正确理解和处理 NTC 的温度特性。

    当使用以下电路时：
    Vcc  --------> Rt  --------> Rref  --------> GND
    需要通过 ```ntc_config_t``` 中的 ```circuit_mode``` 字段选择相应的电路模式。当使用这种电路模式时，应该使用 ```CIRCUIT_MODE_NTC_VCC```。

    当使用以下电路时：
    Vcc  --------> Rref  --------> Rt  --------> GND
    需要通过 ```ntc_config_t``` 中的 ```circuit_mode``` 字段选择相应的电路模式。当使用这种电路模式时，应该使用 ```CIRCUIT_MODE_NTC_GND```。

> `Rref` 是分压电阻；`Rt` 是热敏电阻在 T1 温度下的电阻。

## ntc 热敏电阻方案使用示例

```c
    //1.选择ntc传感器并进行硬件参数初始化配置
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

    //2.创建 NTC 驱动并初始化 ADC
    ntc_device_handle_t ntc = NULL;
    adc_oneshot_unit_handle_t adc_handle = NULL;
    ESP_ERROR_CHECK(ntc_dev_create(&ntc_config, &ntc, &adc_handle));
    ESP_ERROR_CHECK(ntc_dev_get_adc_handle(ntc, &adc_handle));

    //3.调用温度函数
    float temp = 0.0;
    if (ntc_dev_get_temperature(ntc, &temp) == ESP_OK) {
        ESP_LOGI(TAG, "NTC temperature = %.2f ℃", temp);
    }
    ESP_ERROR_CHECK(ntc_dev_delete(ntc));
```

## 示例代码

[点击此处](https://github.com/espressif/esp-iot-solution/tree/master/examples/sensors/ntc_temperature_sensor) 获取示例代码及使用说明。
