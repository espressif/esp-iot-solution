**过零检测**
==================

:link_to_translation:`en:[English]`

过零信号检测驱动程序是一个设计用于分析过零信号的组件。通过检查过零信号的周期和触发边缘，它可以确定信号的有效性、无效性、是否超出预期的频率范围以及是否存在信号丢失等情况。

过零信号检测组件支持检测两种类型的过零信号。

- 方波形：在信号经过零时翻转当前的电平。
- 脉冲型：在信号经过零时生成的脉冲。

由于响应延迟等因素，该组件支持两种驱动模式，包括 GPIO 中断 和 MCPWM 捕获。

用户可以灵活配置组件的驱动模式，并调整诸如有效频率范围和有效信号判断次数等参数。这提供了很高的灵活性。

该程序以事件的形式返回结果和数据，满足用户对信号进行及时处理的需求。

过零检测事件
--------------------

每个零检测事件的触发条件如下表所示：

+--------------------------+-----------------------------------------+
|          事件            |               触发条件                  |
+==========================+=========================================+
| SIGNAL_RISING_EDGE       | 上升沿                                  |
+--------------------------+-----------------------------------------+
| SIGNAL_FALLING_EDGE      | 下降沿                                  |
+--------------------------+-----------------------------------------+
| SIGNAL_VALID             | 频率在有效范围内次数大于设置阈值        |
+--------------------------+-----------------------------------------+
| SIGNAL_INVALID           | 频率在无效范围内次数大于设置阈值        |
+--------------------------+-----------------------------------------+
| SIGNAL_LOST              | 在 100ms 内信号没有上升沿、下降沿       |
+--------------------------+-----------------------------------------+
| SIGNAL_FREQ_OUT_OF_RANGE | 计算的频率在设置的频率范围之外          |
+--------------------------+-----------------------------------------+


.. attention:: 回调函数中不能有 TaskDelay 等阻塞的操作

配置项
-------

- USE_GPTIMER : 决定是否使用 GPTimer 驱动 默认使用 ESPTimer

应用示例
--------

创建过零检测
^^^^^^^^^^^^
.. code:: c

    void IRAM_ATTR zero_detection_event_cb(zero_detect_event_t zero_detect_event, zero_detect_cb_param_t *param, void *usr_data)  //User's callback API
    {
        switch (zero_detect_event) {
        case SIGNAL_FREQ_OUT_OF_RANGE:
            ESP_LOGE(TAG, "SIGNAL_FREQ_OUT_OF_RANGE");
            break;
        case SIGNAL_VALID:
            ESP_LOGE(TAG, "SIGNAL_VALID");
            break;
        case SIGNAL_LOST:
            ESP_LOGE(TAG, "SIGNAL_LOST");
            break;
        default:
            break;
        }
    }

    // Create a zero detection and register call-back
    zero_detect_config_t config = {
        .capture_pin = 2,
        .freq_range_max_hz = 65,
        .freq_range_min_hz = 45,  //Hz
        .valid_times = 6,
        .invalid_times = 5,
        .zero_signal_type = SQUARE_WAVE,
        .zero_driver_type = MCPWM_TYPE,
    };
    zero_detect_handle_t *g_zcds = zero_detect_create(&config);
    if(NULL == g_zcds) {
        ESP_LOGE(TAG, "Zero Detection create failed");
    }
    zero_detect_register_cb(g_zcds, zero_detection_event_cb, NULL);

API Reference
-------------

.. include-build-file:: inc/zero_detection.inc