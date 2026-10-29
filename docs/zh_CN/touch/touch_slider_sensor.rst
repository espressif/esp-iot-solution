触摸滑动传感器
=====================

:link_to_translation:`en:[English]`

概述
----

``touch_slider_sensor`` 组件使用一个触摸 FSM 处理所有配置的频点，基于各通道的正向融合滤波幅度计算滑块位置，并报告位置、滑动和释放事件。组件用于测试和评估，支持的芯片及 ESP-IDF 版本以组件清单为准（需要 ESP-IDF 5.5 或更新版本）。

配置
----

使用 :cpp:type:`touch_slider_config_t` 配置：

.. code:: c

    typedef struct {
        uint32_t channel_num;
        uint32_t *channel_list;
        uint32_t *thresholds;
        uint32_t sample_period_ms;
        uint32_t sample_cfg_num;
        uint32_t debounce_active;
        uint32_t debounce_inactive;
        uint32_t filter_reset_times;
        uint32_t position_range;
        uint8_t calculate_window;
        float swipe_threshold;
        float swipe_hysterisis;
        float swipe_alpha;
        bool skip_lowlevel_init;
    } touch_slider_config_t;

``thresholds`` 是绝对的归一化 FSM 判定计数，属于融合判定域，不是物理 raw 计数，也不是相对于假定 5000 基准的比例。``calculate_window = 0`` 使用默认值（2 通道为 2，其余为 3）；显式设置时可使用 1 到 ``channel_num``。``position_range`` 必须能放入事件使用的有符号数值范围。

示例
----

.. code:: c

    static uint32_t channels[] = {2, 4, 6, 12, 10, 8};
    static uint32_t thresholds[] = {25, 25, 25, 25, 25, 25};
    touch_slider_config_t config = {
        .channel_num = 6,
        .channel_list = channels,
        .thresholds = thresholds,
        .sample_period_ms = 0,
        .sample_cfg_num = 0,
        .debounce_active = 0,
        .debounce_inactive = 0,
        .filter_reset_times = 5,
        .position_range = 10000,
        .calculate_window = 0,
        .swipe_threshold = 50,
        .swipe_hysterisis = 40,
        .swipe_alpha = 0.9f,
        .skip_lowlevel_init = false,
    };
    touch_slider_handle_t handle = NULL;
    ESP_ERROR_CHECK(touch_slider_sensor_create(&config, &handle,
                                               touch_slider_event_callback, NULL));

    while (true) {
        ESP_ERROR_CHECK(touch_slider_sensor_handle_events(handle));
        vTaskDelay(pdMS_TO_TICKS(20));
    }

回调收到的 ``TOUCH_SLIDER_EVENT_POSITION`` 数据位于 ``[0, position_range]``；释放事件携带有符号位移。可选的 ``touch_slider_sensor_get_data`` 查询返回指定通道的正向融合滤波幅度，不再接收频点参数。

应用示例
--------

- :example:`touch/touch_slider_sensor`

API 参考
--------

.. include-build-file:: inc/touch_slider_sensor.inc
