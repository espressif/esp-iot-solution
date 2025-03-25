触摸按键传感器
=====================

:link_to_translation:`en:[English]`

概述
--------

``touch_button_sensor`` 组件为 ESP32 系列芯片提供增强型触摸按键检测功能。它使用多频采样（适用于 ESP32-P4）和基于软件 FSM 的处理来提供可靠的触摸检测，即使在嘈杂的环境中也能正常工作。

.. note::
   - ESP32/ESP32-S2/ESP32-S3 触摸相关组件仅用于测试或演示目的。由于触摸功能的抗干扰能力差，可能无法通过 EMS 测试，因此不建议用于量产产品。
   - 该组件目前适用于 ESP32、ESP32-S2 和 ESP32-S3，并且需要 IDF 版本大于等于 v5.3。

主要特性
^^^^^^^^^^^^^^

* 多频触摸采样，提高抗噪声性能
* 基于 FSM 的触摸检测，具有可配置的阈值
* 支持去抖动以确保状态变化的可靠性
* 支持多个触摸通道
* 基于回调的事件通知

依赖项
^^^^^^^^^^^^^^

* touch_sensor_fsm
* touch_sensor_lowlevel

配置参数
-----------------------------

触摸按键配置结构体
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

触摸按键可以通过 :cpp:type:`touch_button_config_t` 结构体进行配置：

.. code:: c

    typedef struct {
        uint32_t channel_num;          /*!< 触摸按键传感器通道数量 */
        uint32_t *channel_list;        /*!< 触摸通道列表 */
        float *channel_threshold;       /*!< 每个通道的触摸检测阈值 */
        uint32_t *channel_gold_value;   /*!< （可选）触摸通道的参考值 */
        uint32_t debounce_times;        /*!< 确认状态改变所需的连续读数次数 */
        bool skip_lowlevel_init;        /*!< 当与现有触摸驱动程序一起工作时跳过底层初始化 */
    } touch_button_config_t;

参数说明
^^^^^^^^^^^^^^^^^^^^^^^

+--------------------+--------------------------------+--------+
|        参数        |              描述              | 默认值 |
+====================+================================+========+
| channel_num        | 触摸按键传感器通道数量         | -      |
+--------------------+--------------------------------+--------+
| channel_list       | 要使用的触摸通道号数组         | -      |
+--------------------+--------------------------------+--------+
| channel_threshold  | 每个通道的阈值数组             | -      |
+--------------------+--------------------------------+--------+
| channel_gold_value | 触摸通道的参考值（可选）       | NULL   |
+--------------------+--------------------------------+--------+
| debounce_times     | 确认状态改变所需的连续读数次数 | 3      |
+--------------------+--------------------------------+--------+
| skip_lowlevel_init | 如果存在触摸驱动则跳过初始化   | false  |
+--------------------+--------------------------------+--------+

API 使用示例
---------------------

创建和初始化
^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code:: c

    touch_button_config_t config = {
        .channel_num = 1,
        .channel_list = (uint32_t[]){8},            // 使用触摸通道 8
        .channel_threshold = (float[]){0.02},       // 2% 变化阈值
        .channel_gold_value = NULL,                  // 无参考值
        .debounce_times = 3,                        // 3 次读数确认
        .skip_lowlevel_init = false                 // 初始化触摸硬件
    };
    touch_button_handle_t btn_handle = NULL;
    esp_err_t ret = touch_button_sensor_create(&config, &btn_handle, button_state_callback, NULL);

状态改变回调函数
^^^^^^^^^^^^^^^^^^^^^^^^^^

当触摸按键状态改变时会调用回调函数：

.. code:: c

    void button_state_callback(touch_button_handle_t handle, uint32_t channel, touch_state_t state, void *arg)
    {
        switch (state) {
            case TOUCH_STATE_ACTIVE:
                printf("按键通道 %d 被按下\n", channel);
                break;
            case TOUCH_STATE_INACTIVE:
                printf("按键通道 %d 被释放\n", channel);
                break;
        }
    }

事件处理
^^^^^^^^^^^^^^^^

触摸按键传感器组件提供了一个事件处理机制，以非阻塞方式处理触摸事件。事件应该在应用程序的主循环或专用任务中定期处理。

.. code:: c

    // 在主循环或任务中
    while (1) {
        // 处理所有待处理的触摸事件
        touch_button_sensor_handle_events(btn_handle);
        
        // 添加延时以防止紧密循环
        vTaskDelay(pdMS_TO_TICKS(20));  // 20ms 间隔通常足够
    }

示例
--------

- :example:`touch/touch_button_sensor`

API 参考
------------

.. include-build-file:: inc/touch_button_sensor.inc
