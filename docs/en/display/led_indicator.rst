LED Indicator
==============
:link_to_translation:`zh_CN:[中文]`


This guide includes the following content:

.. contents:: Table of Contents
    :local:
    :depth: 2

As one of the simplest output peripherals, LED indicators can indicate the current operating state of the system by blinking in different types. ESP-IoT-Solution provides an LED indicator component with the following features:

- Can define multiple groups of different blink types
- Can define the priority of blink types
- Can set up multiple indicators
- The LEDC driver supports adjusting brightness, gradient, and color.

Supported Indicator Light Types
---------------------------------

+-------------+-------------------------------------------------------+--------+------------+-----------+-------+----------------+-------+
| Driver Type |                      Description                      | On/Off | Brightness | Breathing | Color | Color Gradient | Index |
+=============+=======================================================+========+============+===========+=======+================+=======+
| GPIO        | Control indicator lights through GPIO                 | √      | ×          | ×         | ×     | ×              | ×     |
+-------------+-------------------------------------------------------+--------+------------+-----------+-------+----------------+-------+
| LEDC        | Control indicator lights through PWM (single channel) | √      | √          | √         | ×     | ×              | ×     |
+-------------+-------------------------------------------------------+--------+------------+-----------+-------+----------------+-------+
| RGB LED     | Control indicator lights through PWM (three channels) | √      | √          | √         | √     | √              | ×     |
+-------------+-------------------------------------------------------+--------+------------+-----------+-------+----------------+-------+
| LED Strips  | Control lights like WS2812 through RMT/SPI            | √      | √          | √         | √     | √              | √     |
+-------------+-------------------------------------------------------+--------+------------+-----------+-------+----------------+-------+

Defining Blinking Type
------------------------

Control On/Off
^^^^^^^^^^^^^^^^^

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

.. code:: c

    typedef enum {
        BLINK_TEST_BLINK_ONE_TIME, /**< test_blink_one_time */
        BLINK_TEST_BLINK_LOOP,     /**< test_blink_loop */
        BLINK_MAX,                 /**< INVALID type */
    } led_indicator_blink_type_t;

    blink_step_t const * led_indicator_blink_lists[] = {
        [BLINK_TEST_BLINK_ONE_TIME] = test_blink_one_time,
        [BLINK_TEST_BLINK_LOOP] = test_blink_loop,
        [BLINK_MAX] = NULL,
    };


Controlling Brightness
^^^^^^^^^^^^^^^^^^^^^^^^

For drivers supporting brightness control, the indicator light's brightness can be controlled in the following ways:

Example 1: Defining a brightness setting: Setting the indicator light to 50% brightness for 0.5 seconds.

.. code:: c

    const blink_step_t test_blink_50_brightness[] = {
        {LED_BLINK_BRIGHTNESS, LED_STATE_50_PERCENT, 500},   // step1: set to half brightness 500 ms
        {LED_BLINK_STOP, 0, 0},                              // step4: stop blink (50% brightness)
    };

Example 2: Defining a looping blink: Gradually turning on for 0.5s, then gradually turning off for 0.5s, repeating the sequence.

.. code:: c

    const blink_step_t test_blink_breathe[] = {
        {LED_BLINK_HOLD, LED_STATE_OFF, 0},                  // step1: set LED off
        {LED_BLINK_BREATHE, LED_STATE_ON, 500},              // step2: fade from off to on 500ms
        {LED_BLINK_BREATHE, LED_STATE_OFF, 500},             // step3: fade from on to off 500ms
        {LED_BLINK_LOOP, 0, 0},                              // step4: loop from step1
    };

Example 3: Defining a blink: Gradually brightening from 50% to 100% brightness for 0.5s.

.. code:: c

    const blink_step_t test_blink_breathe_2[] = {
        {LED_BLINK_BRIGHTNESS, LED_STATE_50_PERCENT, 0},     // step1: set to half brightness 0 ms
        {LED_BLINK_BREATHE, LED_STATE_ON, 500},              // step2: fade from off to on 500ms
        {LED_BLINK_STOP, 0, 0},                              // step3: stop blink (100% brightness)
    };

Controlling Color
^^^^^^^^^^^^^^^^^^^

For drivers supporting color control, we can use `LED_BLINK_RGB`, `LED_BLINK_RGB_RING`, `LED_BLINK_HSV`, `LED_BLINK_HSV_RING` to control the color.

    - `LED_BLINK_RGB`: Controls color via RGB, where R takes 8 bits (0-255), G takes 8 bits (0-255), and B takes 8 bits (0-255).
    - `LED_BLINK_RGB_RING`: Controls color gradient through RGB, transitioning from the previous color to the current set color. Use RGB value interpolation method.
    - `LED_BLINK_HSV`: Controls color via HSV, where H takes 9 bits (0-360), S takes 8 bits (0-255), and V takes 8 bits (0-255).
    - `LED_BLINK_HSV_RING`: Controls color gradient through HSV, transitioning from the previous color to the current set color. Use HSV value interpolation method.

Example 1: Defining a color setting to display red on the indicator light.

.. code:: c

    const blink_step_t test_blink_rgb_red[] = {
        {LED_BLINK_RGB, SET_RGB(255, 0, 0), 0},                // step1: set to red color 0 ms
        {LED_BLINK_STOP, 0, 0},                                // step2: stop blink (red color)
    };

Example 2: Defining a color gradient, transitioning the indicator light from red to blue and looping the sequence.

.. code:: c

    const blink_step_t test_blink_rgb_red_blue[] = {
        {LED_BLINK_RGB, SET_RGB(0xFF, 0, 0), 0},               // step1: set to red color 0 ms
        {LED_BLINK_RGB_RING, SET_RGB(0, 0, 0xFF), 4000},       // step2: fade from red to blue 4000ms
        {LED_BLINK_RGB_RING, SET_RGB(0xFF, 0, 0), 4000},       // step3: fade from blue to red 4000ms
        {LED_BLINK_LOOP, 0, 0},                                // step4: loop from step1
    };

Display color gradient using RGB interpolation. The effect is as follows.

.. figure:: ../../_static/display/led_indicator_rgb_ring.png
   :align: center
   :width: 60%

   RGB Gradient

Additionally, the driver supports setting colors through HSV similarly to RGB.

Example 3: Defining a color sequence to display red for 0.5s, green for 0.5s, blue for 0.5s, and then stop.

.. code:: c

    const blink_step_t test_blink_hsv_colors[] = {
        {LED_BLINK_HSV, SET_HSV(0, 255, 255), 500},            // step1: set color to red 500 ms
        {LED_BLINK_HSV, SET_HSV(120, 255, 255), 500},          // step2: set color to green 500 ms
        {LED_BLINK_HSV, SET_HSV(240, 255, 255), 500},          // step3: set color to blue 500 ms
        {LED_BLINK_STOP, 0, 0},                                // step4: stop blink (blue color)
    };

Example 4: Defining a color gradient, transitioning the indicator light from red to blue and looping the sequence using HSV.

.. code:: c

    const blink_step_t test_blink_hsv_red_blue[] = {
        {LED_BLINK_HSV, SET_HSV(0, 255, 255), 0},              // step1: set to red color 0 ms
        {LED_BLINK_HSV_RING, SET_HSV(240, 255, 255), 4000},    // step2: fade from red to blue 4000ms
        {LED_BLINK_HSV_RING, SET_HSV(0, 255, 255), 4000},      // step3: fade from blue to red 4000ms
        {LED_BLINK_LOOP, 0, 0},                                // step4: loop from step1
    };

Using HSV interpolation to display color gradient, the effect is as follows. This method creates a more vibrant color gradient.

.. figure:: ../../_static/display/led_indicator_hsv_ring.png
   :align: center
   :width: 60%

Controlling Index
^^^^^^^^^^^^^^^^^^^

For drivers supporting index control, we can manipulate the state of each light on the strip using macros like INSERT_INDEX, SET_IHSV, SET_IRGB. Setting a value of MAX_INDEX:127 indicates setting all the lights.

Example 1: Defining a color pattern where light at index 0 displays red, index 1 displays green, index 2 displays blue, and then exits.

.. code:: c

    const blink_step_t test_blink_index_setting1[] = {
        {LED_BLINK_RGB, SET_IRGB(0, 255, 0, 0), 0},      // step1: set index 0 to red color 0 ms
        {LED_BLINK_RGB, SET_IRGB(1, 0, 255, 0), 0},      // step2: set index 1 to green color 0 ms
        {LED_BLINK_RGB, SET_IRGB(2, 0, 0, 255), 0},      // step3: set index 2 to blue color 0 ms
        {LED_BLINK_LOOP, 0, 0},                           // step4: loop from step1
    };

Example 2: Defining a color pattern where all lights breathe continuously.

.. code:: c

    const blink_step_t test_blink_all_breath[] = {
        {LED_BLINK_BRIGHTNESS, INSERT_INDEX(MAX_INDEX, LED_STATE_OFF), 0},     // step1: set all leds to off 0 ms
        {LED_BLINK_BREATHE, INSERT_INDEX(MAX_INDEX, LED_STATE_ON), 1000},     // step2: set all leds fade to on 1000 ms
        {LED_BLINK_BREATHE, INSERT_INDEX(MAX_INDEX, LED_STATE_OFF), 1000},    // step3: set all leds fade to off 1000 ms
        {LED_BLINK_LOOP, 0, 0},                                              // step4: loop from step1
    };

Predefined Blinking Priorities
-----------------------------------

These examples demonstrate controlling individual or all lights on the strip using specific indexes or macros like MAX_INDEX.

For the same indicator, a high-priority blink can interrupt an ongoing low-priority blink, which will resume execution after the high-priority blink stop. The blink priority can be adjusted by configuring the enumeration member order of the blink type ``led_indicator_blink_type_t``, the smaller order value the higher execution priority.

For instance, in the following example, ``test_blink_one_time`` has higher priority than ``test_blink_loop``, and should blink first:

.. code:: c

    typedef enum {
        BLINK_TEST_BLINK_ONE_TIME, /**< test_blink_one_time */
        BLINK_TEST_BLINK_LOOP,     /**< test_blink_loop */
        BLINK_MAX,                 /**< INVALID type */
    } led_indicator_blink_type_t;

Control Indicator Blinks
------------------------------

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
--------------------

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
--------------------

The way human eyes perceive brightness is not linear but has certain nonlinear characteristics. Under normal conditions, the human eye is more sensitive to darker areas and less sensitive to brighter areas. However, on digital display devices such as monitors, the brightness values of images are usually encoded in a linear manner. This leads to issues of brightness distortion or loss of details when converting the linearly encoded brightness values to the perceived brightness by the human eye. To address this problem, gamma correction is applied to the image. Gamma correction involves adjusting the brightness values nonlinearly to correct the image display. By applying a gamma value (typically ranging from 2.2 to 2.4), the linearly encoded brightness values are mapped to a nonlinear brightness curve that better matches the perception of the human eye. This improves the visibility of details in darker areas and enhances the overall visual accuracy and balance of the image.

.. figure:: ../../_static/display/led_indicator_gamma_correction.png
   :align: center
   :width: 60%

   Gamma Curve

.. code:: c

    float gamma = 2.3;
    led_indicator_new_gamma_table(gamma);

The default gamma table is 2.3, and a new gamma table can be generated using the `led_indicator_new_gamma_table()` function.

Drive Level Setting
---------------------

For different hardware configurations, it might involve either common anode or common cathode connections. You can adjust the `is_active_level_high` in the settings to either `true` or `false` to configure the drive level.

API Reference
^^^^^^^^^^^^^^^^

.. include-build-file:: inc/led_indicator.inc
