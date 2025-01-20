按键
========

:link_to_translation:`en:[English]`

按键组件实现了 GPIO 和 ADC 两种按键，并允许同时创建两种不同的按键。下图显示了两种按键的硬件设计：

.. figure:: ../../_static/input_device/button/button_hardware.png
    :width: 650

- GPIO 按键优点有：每一个按键占用独立的 IO，之间互不影响，稳定性高；缺点有：按键数量多时占用太多 IO 资源。

- ADC 按键优点有：可多个按键共用一个 ADC 通道，占用 IO 资源少；缺点有：不能同时按下多按键，当按键因氧化等因素导致闭合电阻增大时，容易误触，稳定性不高。

.. note::

    - GPIO 按键需注意上下拉问题，组件内部会启用芯片内部的上下拉电阻，但是在仅支持输入的 IO 内部没有电阻， **需要外部连接**。
    - ADC 按键需注意电压不能超过 ADC 量程。

按键事件
---------

每个按键拥有下表的 8 个事件：

+--------------------------+-----------------------------------+
|           事件           |             触发条件              |
+==========================+===================================+
| BUTTON_PRESS_DOWN        | 按下                              |
+--------------------------+-----------------------------------+
| BUTTON_PRESS_UP          | 弹起                              |
+--------------------------+-----------------------------------+
| BUTTON_PRESS_REPEAT      | 按下弹起次数 >= 2次               |
+--------------------------+-----------------------------------+
| BUTTON_PRESS_REPEAT_DONE | 重复按下结束                      |
+--------------------------+-----------------------------------+
| BUTTON_SINGLE_CLICK      | 按下弹起 1 次                     |
+--------------------------+-----------------------------------+
| BUTTON_DOUBLE_CLICK      | 按下弹起 2 次                     |
+--------------------------+-----------------------------------+
| BUTTON_MULTIPLE_CLICK    | 指定重复按下次数 N 次，达成时触发 |
+--------------------------+-----------------------------------+
| BUTTON_LONG_PRESS_START  | 按下时间达到阈值的瞬间            |
+--------------------------+-----------------------------------+
| BUTTON_LONG_PRESS_HOLD   | 长按期间一直触发                  |
+--------------------------+-----------------------------------+
| BUTTON_LONG_PRESS_UP     | 长按弹起                          |
+--------------------------+-----------------------------------+
| BUTTON_PRESS_REPEAT_DONE | 多次按下弹起结束                  |
+--------------------------+-----------------------------------+
| BUTTON_PRESS_END         | 表示 button 此次检测已结束        |
+--------------------------+-----------------------------------+

每个按键可以有 **回调** 和 **轮询** 两种使用方式：

- 回调：一个按键的每个事件都可以为其注册一个回调函数，产生事件时回调函数将会被调用。这种方式的效率和实时性高，不会丢失事件。

- 轮询：在程序中周期性调用 :c:func:`iot_button_get_event` 查询按键当前的事件。这种方式使用简单，适合任务简单的场合。并不是所有的按键事件都会及时的拿到，存在丢失事件的风险。

当然你也可以将以上两种方式组合使用。

.. attention:: 回调函数中不能有 TaskDelay 等阻塞的操作

.. image:: https://dl.espressif.com/AE/esp-iot-solution/button_3.3.1.svg
    :alt: Button

配置项
-----------

- BUTTON_PERIOD_TIME_MS : 扫描周期

- BUTTON_DEBOUNCE_TICKS : 消抖次数

- BUTTON_SHORT_PRESS_TIME_MS : 连续短按有效时间

- BUTTON_LONG_PRESS_TIME_MS : 长按有效时间

- ADC_BUTTON_MAX_CHANNEL : ADC 按钮的最大通道数

- ADC_BUTTON_MAX_BUTTON_PER_CHANNEL : ADC 一个通道最多的按钮数

- ADC_BUTTON_SAMPLE_TIMES : 每次 ADC 扫描的样本数

- BUTTON_LONG_PRESS_HOLD_SERIAL_TIME_MS : 长按期间触发的 CALLBACK 间隔时间

应用示例
-----------

创建按键
^^^^^^^^^^
.. code:: c

    // create gpio button
    const button_config_t btn_cfg = {0};
    const button_gpio_config_t btn_gpio_cfg = {
        .gpio_num = 0,
        .active_level = 0,
    };
    button_handle_t gpio_btn = NULL;
    esp_err_t ret = iot_button_new_gpio_device(&btn_cfg, &btn_gpio_cfg, &gpio_btn);
    if(NULL == gpio_btn) {
        ESP_LOGE(TAG, "Button create failed");
    }

    // create adc button
    const button_config_t btn_cfg = {0};
    button_adc_config_t btn_adc_cfg = {
        .unit_id = ADC_UNIT_1,
        .adc_channel = 0,
        .button_index = 0,
        .min = 100,
        .max = 400,
    };

    button_handle_t adc_btn = NULL;
    esp_err_t ret = iot_button_new_adc_device(&btn_cfg, &btn_adc_cfg, &adc_btn);
    if(NULL == adc_btn) {
        ESP_LOGE(TAG, "Button create failed");
    }

    // create matrix keypad button
    const button_config_t btn_cfg = {0};
    const button_matrix_config_t matrix_cfg = {
        .row_gpios = (int32_t[]){4, 5, 6, 7},
        .col_gpios = (int32_t[]){3, 8, 16, 15},
        .row_gpio_num = 4,
        .col_gpio_num = 4,
    };
    button_handle_t matrix_button = NULL;
    esp_err_t ret = iot_button_new_matrix_device(&btn_cfg, &matrix_cfg, btns, &matrix_button);
    if(NULL == matrix_button) {
        ESP_LOGE(TAG, "Button create failed");
    }

.. Note::
    当 ADC 按钮使用的是 ADC1 ,当项目中还有其他地方使用到了 ADC1 时，请传入 adc_handle 和 adc_channel 来配置 ADC 按钮。

    .. code::C
        adc_oneshot_unit_handle_t adc1_handle;
        adc_oneshot_unit_init_cfg_t init_config1 = {
            .unit_id = ADC_UNIT_1,
        };
        //-------------ADC1 Init---------------//
        adc_oneshot_new_unit(&init_config1, &adc1_handle);

        const button_config_t btn_cfg = {0};
        button_adc_config_t btn_adc_cfg = {
            .adc_handle = &adc1_handle,
            .unit_id = ADC_UNIT_1,
            .adc_channel = 0,
            .button_index = 0,
            .min = 100,
            .max = 400,
        };

        button_handle_t adc_btn = NULL;
        esp_err_t ret = iot_button_new_adc_device(&btn_cfg, &btn_adc_cfg, &adc_btn);
        if(NULL == adc_btn) {
            ESP_LOGE(TAG, "Button create failed");
        }

注册回调函数
^^^^^^^^^^^^^^

Button 组件支持为多个事件注册回调函数，每个事件都可以注册一个回调函数，当事件发生时，回调函数将会被调用。

其中，

- :cpp:enumerator:`BUTTON_LONG_PRESS_START` 和 :cpp:enumerator:`BUTTON_LONG_PRESS_UP` 支持设置特殊的长按时间。
- :cpp:enumerator:`BUTTON_MULTIPLE_CLICK` 支持设置多次按下的次数。


- 简单写法

    .. code:: c

        static void button_single_click_cb(void *arg,void *usr_data)
        {
            ESP_LOGI(TAG, "BUTTON_SINGLE_CLICK");
        }

        iot_button_register_cb(gpio_btn, BUTTON_SINGLE_CLICK, NULL, button_single_click_cb,NULL);

- 多个回调函数写法

    .. code:: C

        static void button_long_press_1_cb(void *arg,void *usr_data)
        {
            ESP_LOGI(TAG, "BUTTON_LONG_PRESS_START_1");
        }

        static void button_long_press_2_cb(void *arg,void *usr_data)
        {
            ESP_LOGI(TAG, "BUTTON_LONG_PRESS_START_2");
        }

        button_event_args_t args = {
            .long_press.press_time = 2000,
        };

        iot_button_register_cb(gpio_btn, BUTTON_LONG_PRESS_START, &args, button_auto_check_cb_1, NULL);

        args.long_press.press_time = 5000;
        iot_button_register_cb(gpio_btn, BUTTON_LONG_PRESS_START, &args, button_long_press_2_cb, NULL);

查询按键事件
^^^^^^^^^^^^^^

.. code:: c

    button_event_t event;
    event = iot_button_get_event(button_handle);

动态修改按键默认值
^^^^^^^^^^^^^^^^^^

.. code:: c

    iot_button_set_param(btn, BUTTON_LONG_PRESS_TIME_MS, 5000);

低功耗支持
^^^^^^^^^^^

在 light_sleep 模式下，esp_timer 定时器会定时触发，导致 cpu 整体功耗居高不下。为了解决这个问题，button 组件提供了低功耗模式。

所需配置：

- 确保创建的所有按键类型为 GPIO 按键， 并且都开启了 `enable_power_save`，如存在其他按键，会导致低功耗模式失效

.. Note:: 该功能只保证 Button 组件只在使用中才唤醒 CPU, 不保证 CPU 一定会进入低功耗模式

功耗对比：

- 未开启低功耗模式，按下一次按键

    .. figure:: ../../_static/input_device/button/button_one_press.png
        :align: center
        :width: 70%
        :alt: 未开启低功耗模式，一次按下

- 开启低功耗模式，按下一次按键

    .. figure:: ../../_static/input_device/button/button_power_save_one_press.png
        :align: center
        :width: 70%
        :alt: 开启低功耗模式，一次按下

因为 GPIO 唤醒 CPU, 仅支持电平触发，所以当按键为工作电平时，CPU 会支持的被唤醒，取决于按下去的时长，因此在低功耗模式下，单次按下的平均电流高于未开启低功耗模式。但是在大的工作周期中，会比未开启低功耗模式更加省电。

- 未开启低功耗模式下，在 4s 内按下三次按键

    .. figure:: ../../_static/input_device/button/button_three_press_4s.png
        :align: center
        :width: 70%
        :alt: 非低功耗模式下，在 4s 内按下三次按键

- 低功耗模式下，在 4s 内按下三次按键

    .. figure:: ../../_static/input_device/button/button_power_save_three_press_4s.png
        :align: center
        :width: 70%
        :alt: 低功耗模式下，在 4s 内按下三次按键

如图，低功耗模式下更加的省电。

.. code:: c

    button_config_t btn_cfg = {0};
    button_gpio_config_t gpio_cfg = {
        .gpio_num = button_num,
        .active_level = BUTTON_ACTIVE_LEVEL,
        .enable_power_save = true,
    };

    button_handle_t btn;
    iot_button_new_gpio_device(&btn_cfg, &gpio_cfg, &btn);

什么时候进入 Light Sleep

- 使用 Auto Light Sleep: 会在 button 自动关闭 esp_timer 后进入 Light Sleep

- 用户控制 Light Sleep: 需要在 ``enter_power_save_cb`` 回调到来时进入 Light Sleep

.. code:: c

    void btn_enter_power_save(void *usr_data)
    {
        ESP_LOGI(TAG, "Can enter power save now");
    }

    button_power_save_config_t config = {
        .enter_power_save_cb = btn_enter_power_save,
    };

    iot_button_register_power_save_cb(&config);

**开启 CONFIG_PM_POWER_DOWN_PERIPHERAL_IN_LIGHT_SLEEP 选项后，如何正常使用按键？**

- 开启这个宏后，GPIO 模块会下电，如果需要使用按键功能，必须选用 RTC/LP GPIO，并将唤醒源修改为 EXT 1

.. list-table::
    :widths: 40 40 40
    :header-rows: 1

    * - GPIO 类型
      - 是否开启 `CONFIG_PM_POWER_DOWN_PERIPHERAL_IN_LIGHT_SLEEP`
      - 唤醒源
    * - **数字管脚**
      - N
      - GPIO 电平触发
    * - **数字管脚**
      - Y
      - 无
    * - **RTC/LP 管脚**
      - N
      - GPIO 电平触发 / EXT 1
    * - **RTC/LP 管脚**
      - Y
      - EXT 1

.. note:: ESP32-C5, ESP32-C6 的 LP GPIO 可以支持 GPIO 电平唤醒和 EXT 1 唤醒，同时也需要开启 ``gpio_hold_en``

开启和关闭
^^^^^^^^^^^^^

组件支持在任意时刻开启和关闭。

.. code:: c

    // stop button
    iot_button_stop();
    // resume button
    iot_button_resume();

API Reference
-----------------

.. include-build-file:: inc/iot_button.inc
