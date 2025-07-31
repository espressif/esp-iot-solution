触摸按键传感器
=====================

:link_to_translation:`en:[English]`

概述
--------

``touch_button_sensor`` 组件为 ESP32 系列芯片提供增强型触摸按键检测功能。

.. note::
   - ESP32/ESP32-S2/ESP32-S3 触摸相关组件仅用于测试或演示目的。由于触摸功能的抗干扰能力差，可能无法通过 EMS 测试，因此不建议用于量产产品。
   - 该组件目前适用于 ESP32、ESP32-S2 和 ESP32-S3，并且需要 IDF 版本大于等于 v5.3。

主要特性
^^^^^^^^^^^^^^

* 多频触摸采样，提高抗噪声性能
* 支持滑动手势识别
* 基于回调的事件通知

依赖项
^^^^^^^^^^^^^^

* touch_sensor_fsm
* touch_sensor_lowlevel

配置参数
-----------------------------

触摸滑动配置结构体
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

触摸滑动可以通过 :cpp:type:`touch_slider_config_t` 结构体进行配置：

.. code:: c

    typedef struct {
        uint32_t channel_num;         /*!< 触摸按键传感器通道数量 */
        uint32_t *channel_list;       /*!< 触摸通道列表 */
        float *channel_threshold;     /*!< 每个通道的触摸检测阈值 */
        uint32_t *channel_gold_value; /*!< （可选）触摸通道的参考值 */
        uint32_t debounce_times;      /*!< 确认状态改变所需的连续读数次数 */
        uint32_t filter_reset_times;  /*!< 重置位置的读数次数 */
        uint32_t position_range;       /*!< 滑块位置的最大值，位置范围为 [0, position_range]。数值越大位置精度越高 */
        uint8_t calculate_window;    /*!< 位置计算的窗口大小（应该 <= channel_num）。设置为 0 则自动默认：2通道用2，3+通道用3 */
        float swipe_threshold;        /*!< 滑动检测的速度阈值 */
        float swipe_hysterisis;       /*!< 滑动检测的速度迟滞 */
        float swipe_alpha;            /*!< 速度滤波参数 */
        bool skip_lowlevel_init;      /*!< 当与现有触摸驱动程序一起工作时跳过底层初始化 */
    } touch_slider_config_t;

参数说明
^^^^^^^^^^^^^^^^^^^^^^^

+--------------------+------------------------------------+---------+
|        参数        |                描述                | 默认值  |
+====================+====================================+=========+
| channel_num        | 触摸按键传感器通道数量             | -       |
+--------------------+------------------------------------+---------+
| channel_list       | 要使用的触摸通道号数组             | -       |
+--------------------+------------------------------------+---------+
| channel_threshold  | 每个通道的阈值数组                 | -       |
+--------------------+------------------------------------+---------+
| channel_gold_value | 触摸通道的参考值（可选）           | NULL    |
+--------------------+------------------------------------+---------+
| debounce_times     | 确认状态改变所需的连续读数次数     | 3       |
+--------------------+------------------------------------+---------+
| filter_reset_times | 重置位置的读数次数                 | -       |
+--------------------+------------------------------------+---------+
| position_range     | 滑块位置最大值（数值越大精度越高） | -       |
+--------------------+------------------------------------+---------+
| calculate_window   | 位置计算的窗口大小                 | 自动(0) |
+--------------------+------------------------------------+---------+
| swipe_threshold    | 滑动检测的速度阈值                 | -       |
+--------------------+------------------------------------+---------+
| swipe_hysterisis   | 滑动检测的速度迟滞                 | -       |
+--------------------+------------------------------------+---------+
| swipe_alpha        | 速度滤波参数                       | -       |
+--------------------+------------------------------------+---------+
| skip_lowlevel_init | 如果存在触摸驱动则跳过初始化       | false   |
+--------------------+------------------------------------+---------+

计算窗口配置
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

``calculate_window`` 参数决定了用于位置计算的相邻触摸通道数量。这会影响滑块的精度和抗噪性能。

**默认值（当设置为 0 时）：**

- **2 通道**: ``calculate_window = 2`` （使用所有可用通道）
- **3+ 通道**: ``calculate_window = 3`` （精度和抗噪性能之间的最佳平衡）

**手动配置：**

- 您可以显式设置从 2 到 ``channel_num`` 的任何值
- 较小值 (2)：更高的灵敏度但更容易受噪声影响
- 较大值 (3+)：更好的抗噪性能但可能降低分辨率
- 对于高精度应用：推荐值为 ``min(3, channel_num)``
- **注意**：最小值为 2，因为滑动条位置计算至少需要 2 个相邻通道

**向后兼容性：**

- 设置 ``calculate_window = 0`` 启用自动默认选择
- 未初始化此字段的现有代码将使用最优默认值
- 显式设置的值继续按原来的方式工作

API 使用示例
---------------------

创建和初始化
^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code:: c

    uint32_t channel_list[] = {2, 4, 6, 12, 10, 8};
    float threshold[] = {0.005f, 0.005f, 0.005f, 0.005f, 0.005f, 0.005f};
    touch_slider_config_t config = {
        .channel_num = 6,
        .channel_list = channel_list,
        .channel_threshold = threshold,
        .filter_reset_times = 5,
        .position_range = 10000,
        .swipe_alpha = 0.9,
        .swipe_threshold = 50,
        .swipe_hysterisis = 40,
        .channel_gold_value = NULL,
        .debounce_times = 0,
        .calculate_window = 0,  // 使用默认值（6通道时自动选择为3）
        .skip_lowlevel_init = false
    };

    // Test successful creation
    TEST_ASSERT_EQUAL(ESP_OK, touch_slider_sensor_create(&config, &s_touch_slider,  touch_slider_event_callback, NULL));
    TEST_ASSERT_NOT_NULL(s_touch_slider);

事件回调函数
^^^^^^^^^^^^^^^^^^^^^^^^^^

当位置改变或者状态改变时会调用回调函数。结合滑动速度或者松开的位移可以判断手势。

.. code:: c

    static void touch_slider_event_callback(touch_slider_handle_t handle, touch_slider_event_t event, int32_t data, void *cb_arg)
    {
        if (event == TOUCH_SLIDER_EVENT_RIGHT_SWIPE) {
            printf("右滑（速度）\n");
        } else if (event == TOUCH_SLIDER_EVENT_LEFT_SWIPE) {
            printf("左滑（速度）\n");
        } else if (event == TOUCH_SLIDER_EVENT_RELEASE) {
            printf("滑动 %ld\n", data);
            if (data > 1000)
            {
                printf("右滑（位移）\n");
            }
            else if (data < -1000)
            {
                printf("左滑（位移）\n");
            }
        } else if (event == TOUCH_SLIDER_EVENT_POSITION)
        {
            printf("位置,%" PRId64 ",%lu\n", get_time_in_ms(), data);
        }
    }


事件处理
^^^^^^^^^^^^^^^^

触摸滑动传感器组件提供了一个事件处理机制，以非阻塞方式处理触摸事件。事件应该在应用程序的主循环或专用任务中定期处理。

.. code:: c

    // 在主循环或任务中
    while (1) {
        // 处理所有待处理的触摸事件
        touch_slider_sensor_handle_events(s_touch_slider);
        
        // 添加延时以防止紧密循环
        vTaskDelay(pdMS_TO_TICKS(20));  // 20ms 间隔通常足够
    }

示例
--------

- :example:`touch/touch_slider_sensor`

API 参考
------------

.. include-build-file:: inc/touch_slider_sensor.inc
