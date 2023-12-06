[![Component Registry](https://components.espressif.com/components/espressif/esp_sensorless_bldc_control/badge.svg)](https://components.espressif.com/components/espressif/esp_sensorless_bldc_control)

## ESP Sensorless bldc control 组件介绍

``esp_sensorless_bldc_control`` 是基于 ESP32 系列芯片的 BLDC 无感方波控制库，支持以下功能：

* 支持基于 ADC 采样检测过零点
* 支持基于比较器检测过零点
* 支持基于脉冲法实现转子初始相位检测
* 支持堵转保护
* 支持过流，过欠压保护[feature]
* 支持缺相保护[feature]

### ESP Sensorless bldc control 用户指南

请参考：https://docs.espressif.com/projects/esp-iot-solution/zh_CN/latest/motor/index.html

### 添加组件到工程

1. 请使用组件管理器指令 `add-dependency` 将 `esp_sensorless_bldc_control` 添加到项目的依赖项, 在 `CMake` 执行期间该组件将被自动下载到工程目录。

    ```
    idf.py add-dependency "espressif/esp_sensorless_bldc_control=*"
    ```

2. 将 `esp_sensorless_bldc_control/user_cfg/bldc_user_cfg.h.tpl` 文件复制到工程目录下，并更名为 `bldc_user_cfg.h`。并在 `main/CMakeLists.txt` 文件中加入：

    ```
    idf_component_get_property(bldc_lib espressif__esp_sensorless_bldc_control COMPONENT_LIB)
    cmake_policy(SET CMP0079 NEW)
    target_link_libraries(${bldc_lib} PUBLIC ${COMPONENT_LIB})
    ```

    Note: 该文件用于设置电机控制相关参数，一定要包含在工程中。

## 使用示例

```C
    esp_event_loop_create_default();
    esp_event_handler_register(BLDC_CONTROL_EVENT, ESP_EVENT_ANY_ID, &bldc_control_event_handler, NULL);
    switch_config_t_t upper_switch_config = {
        .control_type = CONTROL_TYPE_MCPWM,
        .bldc_mcpwm = {
            .group_id = 0,
            .gpio_num = {17,16,15},
        },
    };

    switch_config_t_t lower_switch_config = {
        .control_type = CONTROL_TYPE_GPIO,
        .bldc_gpio[0] = {
            .gpio_num = 12,
            .gpio_mode = GPIO_MODE_OUTPUT,
        },
        .bldc_gpio[1] = {
            .gpio_num = 11,
            .gpio_mode = GPIO_MODE_OUTPUT,
        },
        .bldc_gpio[2] = {
            .gpio_num = 10,
            .gpio_mode = GPIO_MODE_OUTPUT,
        },
    };

    bldc_zero_cross_adc_config_t zero_cross_adc_config = {
        .adc_handle = NULL,
        .adc_unit = ADC_UNIT_1,
        .chan_cfg = {
            .bitwidth = ADC_BITWIDTH_12,
            .atten = ADC_ATTEN_DB_0,
        },
        .adc_channel = {ADC_CHANNEL_3, ADC_CHANNEL_4, ADC_CHANNEL_5, ADC_CHANNEL_0, ADC_CHANNEL_1},
    };

    bldc_control_config_t config = {
        .speed_mode = SPEED_CLOSED_LOOP,
        .control_mode = BLDC_SIX_STEP,
        .alignment_mode = ALIGNMENT_ADC,
        .six_step_config = {
            .lower_switch_active_level = 0,
            .upper_switch_config = upper_switch_config,
            .lower_switch_config = lower_switch_config,
            .mos_en_config.has_enable = false,
        },
        .zero_cross_adc_config = zero_cross_adc_config,
    };

    /*!< Init hardware driver */
    bldc_control_init(&bldc_control_handle, &config);

    /*!< Setting motor direction */
    bldc_control_set_dir(bldc_control_handle, CCW);

    /*!< Start motor control */
    bldc_control_start(bldc_control_handle, 300);
```