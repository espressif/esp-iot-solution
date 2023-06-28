按键
========

:link_to_translation:`en:[English]`

按键组件实现了 GPIO 和 ADC 两种按键，并允许同时创建两种不同的按键。下图显示了两种按键的硬件设计：

.. figure:: ../../_static/button_hardware.png
    :width: 650

- GPIO 按键优点有：每一个按键占用独立的 IO，之间互不影响，稳定性高；缺点有：按键数量多时占用太多 IO 资源。

- ADC 按键优点有：可多个按键共用一个 ADC 通道，占用 IO 资源少；缺点有：不能同时按下多按键，当按键因氧化等因素导致闭合电阻增大时，容易误触，稳定性不高。

.. note:: 

    - GPIO 按键需注意上下拉问题，组件内部会启用芯片内部的上下拉电阻，但是在仅支持输入的 IO 内部没有电阻， **需要外部连接**。
    - ADC 按键需注意电压不能超过 ADC 量程。

按键事件
---------

每个按键拥有下表的 8 个事件：

+--------------------------+------------------------+
|           事件           |        触发条件        |
+==========================+========================+
| BUTTON_PRESS_DOWN        | 按下                   |
+--------------------------+------------------------+
| BUTTON_PRESS_UP          | 弹起                   |
+--------------------------+------------------------+
| BUTTON_PRESS_REPEAT      | 按下弹起次数 >= 2次    |
+--------------------------+------------------------+
| BUTTON_SINGLE_CLICK      | 按下弹起 1 次          |
+--------------------------+------------------------+
| BUTTON_DOUBLE_CLICK      | 按下弹起 2 次          |
+--------------------------+------------------------+
| BUTTON_LONG_PRESS_START  | 按下时间达到阈值的瞬间 |
+--------------------------+------------------------+
| BUTTON_LONG_PRESS_HOLD   | 长按期间一直触发       |
+--------------------------+------------------------+
| BUTTON_PRESS_REPEAT_DONE | 多次按下弹起结束       |
+--------------------------+------------------------+

每个按键可以有 **回调** 和 **轮询** 两种使用方式：

- 回调：一个按键的每个事件都可以为其注册一个回调函数，产生事件时回调函数将会被调用。这种方式的效率和实时性高，不会丢失事件。

- 轮询：在程序中周期性调用 :c:func:`iot_button_get_event` 查询按键当前的事件。这种方式使用简单，适合任务简单的场合

当然你也可以将以上两种方式组合使用。

.. attention:: 回调函数中不能有 TaskDelay 等阻塞的操作

配置项
-----------

- BUTTON_PERIOD_TIME_MS : 扫描周期

- BUTTON_DEBOUNCE_TICKS : 消抖次数

- BUTTON_SHORT_PRESS_TIME_MS : 连续短按有效时间

- BUTTON_LONG_PRESS_TIME_MS : 长按有效时间

- ADC_BUTTON_MAX_CHANNEL : ADC 按钮的最大通道数

- ADC_BUTTON_MAX_BUTTON_PER_CHANNEL : ADC 一个通道最多的按钮数

- ADC_BUTTON_SAMPLE_TIMES : 每次扫描的样本数

- BUTTON_SERIAL_TIME_MS : 长按期间触发的 CALLBACK 间隔时间

应用示例
-----------

创建按键
^^^^^^^^^^
.. code:: c

    // create gpio button
    button_config_t gpio_btn_cfg = {
        .type = BUTTON_TYPE_GPIO,
        .long_press_ticks = CONFIG_BUTTON_LONG_PRESS_TIME_MS,
        .short_press_ticks = CONFIG_BUTTON_SHORT_PRESS_TIME_MS,
        .gpio_button_config = {
            .gpio_num = 0,
            .active_level = 0,
        },
    };
    button_handle_t gpio_btn = iot_button_create(&gpio_btn_cfg);
    if(NULL == gpio_btn) {
        ESP_LOGE(TAG, "Button create failed");
    }

    // create adc button
    button_config_t adc_btn_cfg = {
        .type = BUTTON_TYPE_ADC,
        .long_press_ticks = CONFIG_BUTTON_LONG_PRESS_TIME_MS,
        .short_press_ticks = CONFIG_BUTTON_SHORT_PRESS_TIME_MS,
        .adc_button_config = {
            .adc_channel = 0,
            .button_index = 0,
            .min = 100,
            .max = 400,
        },
    };
    button_handle_t adc_btn = iot_button_create(&adc_btn_cfg);
    if(NULL == adc_btn) {
        ESP_LOGE(TAG, "Button create failed");
    }

.. Note::
    ADC 按钮使用的是 ADC1 ,当项目中还有其他地方使用到了 ADC1 时，请传入 adc_handle 和 adc_channel 来配置 ADC 按钮。

    .. code::C
        adc_oneshot_unit_handle_t adc1_handle;
        adc_oneshot_unit_init_cfg_t init_config1 = {
            .unit_id = ADC_UNIT_1,
        };
        //-------------ADC1 Init---------------//
        adc_oneshot_new_unit(&init_config1, &adc1_handle);
        // create adc button
        button_config_t adc_btn_cfg = {
            .type = BUTTON_TYPE_ADC,
            .adc_button_config = {
                .adc_handle = &adc1_handle,
                .adc_channel = 1,
            },
        };
        button_handle_t adc_btn = iot_button_create(&adc_btn_cfg);
        if(NULL == adc_btn) {
            ESP_LOGE(TAG, "Button create failed");
        }

注册回调函数
^^^^^^^^^^^^^^

.. code:: c

    static void button_single_click_cb(void *arg,void *usr_data)
    {
        ESP_LOGI(TAG, "BUTTON_SINGLE_CLICK");
    }

    iot_button_register_cb(gpio_btn, BUTTON_SINGLE_CLICK, button_single_click_cb,NULL);

查询按键事件
^^^^^^^^^^^^^^

.. code:: c

    button_event_t event;
    event = iot_button_get_event(button_handle);

API Reference
-----------------

.. include-build-file:: inc/iot_button.inc
