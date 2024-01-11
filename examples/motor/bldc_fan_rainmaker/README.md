# bldc_fan_rainmaker

`bldc_fan_rainmaker` 例程将无刷电机驱动的电风扇接入 ESP Rainmaker 云，实现了以下功能

* 风扇无极调速
* 摇头
* 自然风
* 远程启动关闭
* OTA
* BLE 配网

## 组件介绍

* [esp_sensorless_bldc_control](https://components.espressif.com/components/espressif/esp_sensorless_bldc_control) 是基于 ESP32 系列芯片的 BLDC 无感方波控制库。支持以下功能：
    * 支持基于 ADC 采样检测过零点
    * 支持基于比较器检测过零点
    * 支持基于脉冲法实现转子初始相位检测
    * 支持堵转保护
    * 支持过流，过欠压保护[feature]
    * 支持缺相保护[feature]

* [esp_rainmaker](https://components.espressif.com/components/espressif/esp_rainmaker) 是一个完整且轻量级的 AIoT 解决方案，能够以简单、经济高效且高效的方式为您的企业实现私有云部署。