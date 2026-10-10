触摸接近感应传感器
=========================

:link_to_translation:`en:[English]`

概述
----

``touch_proximity_sensor`` 使用 ESP32-S3 的触摸接近感应数据通路。接近感应保持独立的底层数据类型：累计测量值不会按普通触摸 raw 计数处理，不使用普通触摸的 5000 启动目标，也不使用普通触摸频率估计接口。组件需要 ESP-IDF 5.5 或更新版本，并运行在 ESP32-S3 上。

封装层把完整的接近感应帧交给一个 FSM 处理。FSM 的有效周期等于底层扫描周期乘以底层实际报告的 ``proximity_count``。应在独立的接近基准稳定后，根据目标板空闲数据调整阈值。

配置
----

使用 :cpp:type:`touch_proxi_config_t` 配置：

.. code:: c

    typedef struct {
        uint32_t channel_num;
        uint32_t *channel_list;
        uint32_t *thresholds;
        uint32_t sample_period_ms;
        uint32_t sample_cfg_num;
        uint32_t proximity_count;
        uint32_t debounce_active;
        uint32_t debounce_inactive;
        bool skip_lowlevel_init;
    } touch_proxi_config_t;

``thresholds`` 是接近感应数据域中的绝对归一化 FSM 判定计数，不是百分比，也不是普通触摸 raw 计数。采样字段为零时使用底层默认值；``proximity_count`` 为零时使用 Kconfig 默认值（范围 1 到 255）。设置 ``skip_lowlevel_init = true`` 时，现有底层实例必须是接近感应类型；如果显式指定计数，还必须与底层实际计数一致。

示例
----

.. code:: c

    static uint32_t channels[] = {8};
    static uint32_t thresholds[] = {25};
    touch_proxi_config_t config = {
        .channel_num = 1,
        .channel_list = channels,
        .thresholds = thresholds,
        .sample_period_ms = 0,
        .sample_cfg_num = 0,
        .proximity_count = 0,
        .debounce_active = 2,
        .debounce_inactive = 2,
        .skip_lowlevel_init = false,
    };
    touch_proximity_handle_t handle = NULL;
    ESP_ERROR_CHECK(touch_proximity_sensor_create(&config, &handle,
                                                  proximity_callback, NULL));

    while (true) {
        ESP_ERROR_CHECK(touch_proximity_sensor_handle_events(handle));
        vTaskDelay(pdMS_TO_TICKS(20));
    }

使用 :cpp:func:`touch_proximity_sensor_get_data` 查询正向融合滤波幅度，使用 :cpp:func:`touch_proximity_sensor_get_state` 查询稳定状态；回调会报告按下和释放状态变化。

应用示例
--------

- :example:`touch/touch_proximity`

API 参考
--------

.. include-build-file:: inc/touch_proximity_sensor.inc
