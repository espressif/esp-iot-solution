触摸按键传感器
=====================

:link_to_translation:`en:[English]`

概述
----

``touch_button_sensor`` 组件（发布版本为 0.3.0）组合触摸底层驱动和触摸 FSM，接收完整的多频数据帧，输出正向融合幅度，并通过回调报告经过消抖的按下和释放状态。组件用于测试和评估，支持的芯片及 ESP-IDF 版本以组件清单为准（需要 ESP-IDF 5.5 或更新版本）。

底层驱动会在注册按键回调前完成启动对齐。上电时如果电极上已有手指，初始工作点可能因此改变；请根据目标板实测的 FSM 判定域幅度调整阈值。

配置
----

使用 :cpp:type:`touch_button_config_t` 配置：

.. code:: c

    typedef struct {
        uint32_t channel_num;
        uint32_t *channel_list;
        uint32_t *thresholds;
        uint32_t sample_period_ms;
        uint32_t sample_cfg_num;
        uint32_t debounce_active;
        uint32_t debounce_inactive;
        bool skip_lowlevel_init;
    } touch_button_config_t;

``thresholds`` 是每个通道一个绝对的、归一化 FSM 判定阈值，不是百分比，也不再使用旧的参考值数组。采样周期和采样次数为零时使用底层默认值；两个消抖参数为零时使用 FSM 默认值。设置 ``skip_lowlevel_init`` 后，应用必须先自行创建并启动匹配的底层实例。

示例
----

.. code:: c

    static uint32_t channels[] = {1, 2, 3, 4};
    static uint32_t thresholds[] = {70, 70, 70, 70};
    touch_button_config_t config = {
        .channel_num = 4,
        .channel_list = channels,
        .thresholds = thresholds,
        .sample_period_ms = 20,
        .sample_cfg_num = 0,
        .debounce_active = 0,
        .debounce_inactive = 0,
        .skip_lowlevel_init = false,
    };
    touch_button_handle_t handle = NULL;
    ESP_ERROR_CHECK(touch_button_sensor_create(&config, &handle,
                                               button_state_callback, NULL));

    while (true) {
        ESP_ERROR_CHECK(touch_button_sensor_handle_events(handle));
        vTaskDelay(pdMS_TO_TICKS(10));
    }

回调会收到通道号和稳定状态。使用 :cpp:func:`touch_button_sensor_get_data` 查询当前正向融合幅度，使用 :cpp:func:`touch_button_sensor_get_state` 查询当前状态。已发布组件只提供按键公共 API，不提供 wrapper debug 开关，也不发布 FSM private 头文件。如需查看 raw、baseline、融合过程或 FSM 快照，请编译仓库中的 FSM 示例；调试配置和 private include 路径由该示例负责。

应用示例
--------

- :example:`touch/touch_button_sensor`

API 参考
--------

结构化的公共 API 头文件随已发布组件维护：

`touch_button_sensor 公共 API <https://components.espressif.com/components/espressif/touch_button_sensor>`_.
