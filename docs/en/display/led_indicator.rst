LED Indicator
==============
:link_to_translation:`zh_CN:[中文]`

As one of the simplest output peripherals, LED indicators can indicate the current operating state of the system by blinking in different types. ESP-IoT-Solution provides an LED indicator component with the following features:

- Can define multiple groups of different blink types
- Can define the priority of blink types
- Can set up multiple indicators
- LEDC and other drivers support adjustable brightness, gradient

Instructions
^^^^^^^^^^^^^^^^^

Pre-define Blink Types
++++++++++++++++++++++++++++++++

The blink step structure :cpp:type:`blink_step_t` defines the type of a step, indicator state and the state duration. Multiple steps are combined into one blink type, and different blink types can be used to identify different system states. The blink type is defined as follows:

Example 1. define a blink loop: turn on 0.05 s, turn off 0.1 s, and loop.

.. code:: c

    const blink_step_t test_blink_loop[] = {
        {LED_BLINK_HOLD, LED_STATE_ON, 50},               // step1: turn on LED 50 ms
        {LED_BLINK_HOLD, LED_STATE_OFF, 100},             // step2: turn off LED 100 ms
        {LED_BLINK_LOOP, 0, 0},                           // step3: loop from step1
    };

Example 2. define a blink loop: turn on 0.05 s, turn off 0.1 s, turn on 0.15 s, turn off 0.1 s, and stop blink.

.. code:: c

    const blink_step_t test_blink_one_time[] = {
        {LED_BLINK_HOLD, LED_STATE_ON, 50},               // step1: turn on LED 50 ms
        {LED_BLINK_HOLD, LED_STATE_OFF, 100},             // step2: turn off LED 100 ms
        {LED_BLINK_HOLD, LED_STATE_ON, 150},              // step3: turn on LED 150 ms
        {LED_BLINK_HOLD, LED_STATE_OFF, 100},             // step4: turn off LED 100 ms
        {LED_BLINK_STOP, 0, 0},                           // step5: stop blink (off)
    };

Example 3. Define a cyclic blink: 0.05s fade, 0.5s fade, and the light goes off at the end of execution. (GPIO mode is not supported)

.. code:: c

    const blink_step_t test_blink_breathe[] = {
        {LED_BLINK_BREATHE, LED_STATE_ON, 500},              // step1: fade from off to on 500ms
        {LED_BLINK_BREATHE, LED_STATE_OFF, 500},             // step2: fade from on to off 500ms
        {LED_BLINK_BRIGHTNESS, 50, 500},                     // step3: set to half brightness 500 ms
        {LED_BLINK_STOP, 0, 0},                              // step4: stop blink (50% brightness)
    };

After defining a blink type, you need to add its corresponding enumeration member to ``led_indicator_blink_type_t`` and then add the type to the blink type list ``led_indicator_blink_lists``, as the following example:

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

Pre-define Blink Priorities
++++++++++++++++++++++++++++++++++++

For the same indicator, a high-priority blink can interrupt an ongoing low-priority blink, which will resume execution after the high-priority blink stop. The blink priority can be adjusted by configuring the enumeration member order of the blink type ``led_indicator_blink_type_t``, the smaller order value the higher execution priority.

For instance, in the following example, ``test_blink_one_time`` has higher priority than ``test_blink_loop``, and should blink first:

.. code:: c

    typedef enum {
        BLINK_TEST_BLINK_ONE_TIME, /**< test_blink_one_time */
        BLINK_TEST_BLINK_LOOP,     /**< test_blink_loop */
        BLINK_MAX,                 /**< INVALIED type */ 
    } led_indicator_blink_type_t;

Control Indicator Blinks
++++++++++++++++++++++++++++++

Create an indicator by specifying an IO and a set of configuration information.

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


Start/stop blinking: control your indicator to start/stop a specified type of blink by calling corresponding functions. The functions are returned immediately after calling, and the blink process is controlled by the internal timer. The same indicator can perform multiple blink types in turn based on their priorities.

.. code:: c

    led_indicator_start(led_handle, BLINK_TEST_BLINK_LOOP); // call to start, the function not block

    /*
    *......
    */

    led_indicator_stop(led_handle, BLINK_TEST_BLINK_LOOP); // call stop


Delete an indicator: you can also delete an indicator to release resources if there are no further operations required.

.. code:: c

    led_indicator_delete(&led_handle);

Preempt operation: You can flash the specified type directly at any time.

.. code:: c

    led_indicator_preempt_start(led_handle, BLINK_TEST_BLINK_LOOP);

Stop preempt: You can use the stop queueing function to cancel the blinking mode that is being queued.

.. code:: c

    led_indicator_preempt_stop(led_handle, BLINK_TEST_BLINK_LOOP);

.. Note::

    This component supports thread-safe operations. You can share the LED indicator handle ``led_indicator_handle_t`` with global variables, or use :cpp:type:`led_indicator_get_handle` to get the handle in other threads via the LED's IO number for operation.

Custom light blink
++++++++++++++++++++++

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

By defining ``led_blink_lst[]`` to achieve the custom indicator.

Adjustment of Gamma
++++++++++++++++++++++

The way human eyes perceive brightness is not linear but has certain nonlinear characteristics. Under normal conditions, the human eye is more sensitive to darker areas and less sensitive to brighter areas. However, on digital display devices such as monitors, the brightness values of images are usually encoded in a linear manner. This leads to issues of brightness distortion or loss of details when converting the linearly encoded brightness values to the perceived brightness by the human eye. To address this problem, gamma correction is applied to the image. Gamma correction involves adjusting the brightness values nonlinearly to correct the image display. By applying a gamma value (typically ranging from 2.2 to 2.4), the linearly encoded brightness values are mapped to a nonlinear brightness curve that better matches the perception of the human eye. This improves the visibility of details in darker areas and enhances the overall visual accuracy and balance of the image.

.. figure:: ../../_static/display/led_indicator_gamma_correction.png
   :align: center
   :width: 60%

   Gamma Curve

.. code:: c

    float gamma = 2.3;
    led_indicator_new_gamma_table(gamma);

The default gamma table is 2.3, and a new gamma table can be generated using the `led_indicator_new_gamma_table()` function.

API Reference
^^^^^^^^^^^^^^^^

.. include-build-file:: inc/led_indicator.inc
