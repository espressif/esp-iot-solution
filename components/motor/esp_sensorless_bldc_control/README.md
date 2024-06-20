[![Component Registry](https://components.espressif.com/components/espressif/esp_sensorless_bldc_control/badge.svg)](https://components.espressif.com/components/espressif/esp_sensorless_bldc_control)

## ESP Sensorless BLDC Control Component Introduction

``esp_sensorless_bldc_control`` is a sensorless BLDC control library based on ESP32 series chips, supporting the following features:

* Supports zero-crossing detection based on ADC sampling
* Supports zero-crossing detection based on comparators
* Supports initial rotor position detection using pulse method
* Supports stall protection
* Supports overcurrent and undervoltage protection [feature]
* Supports phase loss protection [feature]

### ESP Sensorless BLDC Control User Guide

Please refer to：https://docs.espressif.com/projects/esp-iot-solution/zh_CN/latest/motor/index.html

### Adding the Component to Your Project

1. Use the component manager command `add-dependency` to add `esp_sensorless_bldc_control` to your project's dependencies. The component will be automatically downloaded to the project directory during `CMake` execution.

    ```
    idf.py add-dependency "espressif/esp_sensorless_bldc_control=*"
    ```

2. Copy the `esp_sensorless_bldc_control/user_cfg/bldc_user_cfg.h` file to your project directory and rename it to `bldc_user_cfg.h`. Add the following to your `main/CMakeLists.txt` file:

    ```
    idf_component_get_property(bldc_lib espressif__esp_sensorless_bldc_control COMPONENT_LIB)
    cmake_policy(SET CMP0079 NEW)
    target_link_libraries(${bldc_lib} PUBLIC ${COMPONENT_LIB})
    ```

    Note: This file is used to set motor control related parameters and must be included in the project.

## Example Usage

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