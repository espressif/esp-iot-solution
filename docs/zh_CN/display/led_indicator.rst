LED 指示灯
==============
:link_to_translation:`en:[English]`

LED 指示灯是最简单的输出外设之一，可以通过不同形式的闪烁指示系统当前的工作状态。ESP-IoT-Solution 提供的 LED 指示灯组件具有以下功能：

- 支持定义多组闪烁类型
- 支持定义闪烁类型优先级
- 支持创建多个指示灯
- LEDC 等驱动支持调节亮度，渐变

使用方法
^^^^^^^^^^^^^

预定义闪烁类型
++++++++++++++++++

闪烁步骤结构体 :cpp:type:`blink_step_t` 定义了该步骤的类型、指示灯状态和状态持续时间。多个步骤组合成一个闪烁类型，不同的闪烁类型可用于标识不同的系统状态。闪烁类型的定义方法如下：

例 1. 定义一个循环闪烁：亮 0.05 s，灭 0.1 s，开始之后一直循环。

.. code:: c

    const blink_step_t test_blink_loop[] = {
        {LED_BLINK_HOLD, LED_STATE_ON, 50},               // step1: turn on LED 50 ms
        {LED_BLINK_HOLD, LED_STATE_OFF, 100},             // step2: turn off LED 100 ms
        {LED_BLINK_LOOP, 0, 0},                           // step3: loop from step1
    };

例 2. 定义一个循环闪烁：亮 0.05 s，灭 0.1 s，亮 0.15 s，灭 0.1 s，执行完毕灯熄灭。

.. code:: c

    const blink_step_t test_blink_one_time[] = {
        {LED_BLINK_HOLD, LED_STATE_ON, 50},               // step1: turn on LED 50 ms
        {LED_BLINK_HOLD, LED_STATE_OFF, 100},             // step2: turn off LED 100 ms
        {LED_BLINK_HOLD, LED_STATE_ON, 150},              // step3: turn on LED 150 ms
        {LED_BLINK_HOLD, LED_STATE_OFF, 100},             // step4: turn off LED 100 ms
        {LED_BLINK_STOP, 0, 0},                           // step5: stop blink (off)
    };

例 3. 定义一个循环闪烁：渐亮 0.05s， 逐灭 0.5s， 执行完毕灯熄灭。（GPIO 模式不支持）

.. code:: c

    const blink_step_t test_blink_breathe[] = {
        {LED_BLINK_BREATHE, LED_STATE_ON, 500},              // step1: fade from off to on 500ms
        {LED_BLINK_BREATHE, LED_STATE_OFF, 500},             // step2: fade from on to off 500ms
        {LED_BLINK_BRIGHTNESS, 50, 500},                     // step3: set to half brightness 500 ms
        {LED_BLINK_STOP, 0, 0},                              // step4: stop blink (50% brightness)
    };

定义闪烁类型之后，需要在 ``led_indicator_blink_type_t`` 添加该类型对应的枚举成员，然后将其添加到闪烁类型列表 ``led_indicator_blink_lists``，示例如下：

.. code:: c

    typedef enum {
        BLINK_TEST_BLINK_ONE_TIME, /**< test_blink_one_time */
        BLINK_TEST_BLINK_LOOP,     /**< test_blink_loop */
        BLINK_MAX,                 /**< INVALIED type */ 
    } led_indicator_blink_type_t;

    blink_step_t const * led_indicator_blink_lists[] = {
        [BLINK_TEST_BLINK_ONE_TIME] = test_blink_one_time,
        [BLINK_TEST_BLINK_LOOP] = test_blink_loop,
        [BLINK_MAX] = NULL,
    };

预定义闪烁优先级
++++++++++++++++++++++

对于同一个指示灯，高优先级闪烁可以打断正在进行的低优先级闪烁，当高优先级闪烁结束，低优先级闪烁恢复执行。可以通过调整闪烁类型 ``led_indicator_blink_type_t`` 枚举成员的顺序调整闪烁的优先级，数值越小的成员执行优先级越高。

例如，在以下示例中闪烁 ``test_blink_one_time`` 比 ``test_blink_loop`` 优先级高，可优先闪烁：

.. code:: c

    typedef enum {
        BLINK_TEST_BLINK_ONE_TIME, /**< test_blink_one_time */
        BLINK_TEST_BLINK_LOOP,     /**< test_blink_loop */
        BLINK_MAX,                 /**< INVALIED type */ 
    } led_indicator_blink_type_t;

控制指示灯闪烁
+++++++++++++++++++

创建一个指示灯：指定一个 IO 和一组配置信息创建一个指示灯

.. code:: c

    led_indicator_config_t config = {
        .mode = LED_GPIO_MODE,
        .led_gpio_config = {
            .active_level = 1,
            .gpio_num = 1,
        },
        .blink_lists = led_indicator_get_sample_lists(),
        .blink_list_num = led_indicator_get_sample_lists_num(),
    };
    led_indicator_handle_t led_handle = led_indicator_create(8, &config); // attach to gpio 8


开始/停止闪烁：控制指示灯开启/停止指定闪烁类型，函数调用后立刻返回，内部由定时器控制闪烁流程。同一个指示灯可以开启多种闪烁类型，将根据闪烁类型优先级依次执行。

.. code:: c

    led_indicator_start(led_handle, BLINK_TEST_BLINK_LOOP); // call to start, the function not block

    /*
    *......
    */

    led_indicator_stop(led_handle, BLINK_TEST_BLINK_LOOP); // call stop


删除指示灯：您也可以在不需要进一步操作时，删除指示灯以释放资源

.. code:: c

    led_indicator_delete(&led_handle);


抢占操作： 您可以在任何时候直接闪烁指定的类型。

.. code:: c

    led_indicator_preempt_start(led_handle, BLINK_TEST_BLINK_LOOP);

停止抢占：您可以使用停止抢占函数，来取消正在抢占的闪烁模式。

.. code:: c

    led_indicator_preempt_stop(led_handle, BLINK_TEST_BLINK_LOOP);

.. Note::

    该组件支持线程安全操作，您可使用全局变量共享 LED 指示灯的操作句柄 ``led_indicator_handle_t``，也可以使用 :cpp:type:`led_indicator_get_handle` 在其它线程通过 LED 的 IO 号获取句柄以进行操作。

自定义指示灯闪烁
+++++++++++++++++++

.. code:: c
    
    static blink_step_t const *led_blink_lst[] = {
        [BLINK_DOUBLE] = double_blink,
        [BLINK_TRIPLE] = triple_blink,
        [BLINK_NUM] = NULL,
    };

    led_indicator_config_t config = {
        .mode = LED_GPIO_MODE,
        .led_gpio_config = {
            .active_level = 1,
            .gpio_num = 1,
        },
        .blink_lists = led_blink_lst,
        .blink_list_num = BLINK_MAX,
    };

通过定义 ``led_blink_lst[]`` 实现自定义指示灯。

gamma 曲线调光
+++++++++++++++++++

人眼感知亮度的方式并非线性，而是具有一定的非线性特征。在标准情况下，人眼对较暗的区域更敏感，对较亮的区域不太敏感。然而，在数字显示设备（如显示器）上，图像的亮度值通常以线性方式编码。这就导致在将线性编码的亮度值转换为人眼感知的亮度时，图像会出现明暗失真或细节丢失的情况。为了解决这个问题，需要对图像进行Gamma校正。Gamma校正是通过对亮度值进行非线性调整来纠正图像的显示。通过施加一个Gamma值（通常介于2.2至2.4之间），可以将线性编码的亮度值映射到更符合人眼感知的非线性亮度曲线上。这样可以提高暗部细节的可见性，并使图像在视觉上更加准确和平衡。

.. figure:: ../../_static/display/led_indicator_gamma_correction.png
   :align: center
   :width: 60%

   Gamma 曲线

.. code:: c

    float gamma = 2.3
    led_indicator_new_gamma_table(gamma);

默认的 gamma 表是 2.3，可以通过 ``led_indicator_new_gamma_table()`` 生成新的 gamma 表。

API 参考
^^^^^^^^^^^^^^^^

.. include-build-file:: inc/led_indicator.inc

