Lightbulb Driver
=================
:link_to_translation:`zh_CN:[中文]`

The Lightbulb driver component encapsulates several commonly used dimming schemes for bulb lights, 
using an abstraction layer to manage these schemes, making it easier for developers to integrate into their applications. 
Currently, all ESP32 series chips are fully supported

Dimming scheme
---------------

PWM Dimming scheme
^^^^^^^^^^^^^^^^^^^

PWM dimming is a technique that controls LED brightness by adjusting the pulse width. 
The core idea is to vary the duty cycle of the current pulse (i.e., the proportion of high-level time within the entire cycle time). 
When the duty cycle is high, the LED receives current for a longer duration, resulting in higher brightness; conversely, a low duty cycle shortens the current duration, producing lower brightness.

All ESP chips support PWM output using hardware `LEDC driver <https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/ledc.html?highlight=ledc>`__ or `MCPWM <https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/mcpwm.html?highlight=mcpwm>`__ driver. 
It is recommended to use the LEDC driver as it supports hardware fading, with configurable frequency and duty cycle, and a maximum resolution of 20 bits. 
The following types of PWM dimming drivers are currently supported:

   -  RGB + C/W
   -  RGB + CCT/Brightnes

PWM scheme use cases
~~~~~~~~~~~~~~~~~~~~~~

Use the PWM scheme to light up a 3-channel RGB bulb with PWM frequency set to 4000Hz. Use software color mixing, enable the fade function, and set the power-on color to red.

.. code:: c

    lightbulb_config_t config = {
        // 1. Select PWM output and configure parameters
        .type = DRIVER_ESP_PWM,
        .driver_conf.pwm.freq_hz = 4000,

        // 2. capability selection: enable/disable according to your needs
        .capability.enable_fade = true,
        .capability.fade_time_ms = 800,
        .capability.enable_lowpower = false,
        /* Enable this feature if your white light output is hardware-controlled independently rather than software color-mixed */
        .capability.enable_hardware_cct = true,
        .capability.enable_status_storage = true,
        /* Configure the LED combination based on your LED aluminum substrate */
        .capability.led_beads = LED_BEADS_3CH_RGB,
        .capability.storage_cb = NULL,
        .capability.sync_change_brightness_value = true,

        // 3. Configure hardware pins for PWM output
        .io_conf.pwm_io.red = 25,
        .io_conf.pwm_io.green = 26,
        .io_conf.pwm_io.blue = 27,

        // 4. Limits parameter 
        .external_limit = NULL,

        // 5. Color calibration parameters
        .gamma_conf = NULL,

        // 6. Initialize lighting parameters; if 'on' is set, the lightbulb will light up during driver initialization
        .init_status.mode = WORK_COLOR,
        .init_status.on = true,
        .init_status.hue = 0,
        .init_status.saturation = 100,
        .init_status.value = 100,
    };
    lightbulb_init(&config);

I2C Dimming Scheme
^^^^^^^^^^^^^^^^^^^

`I2C Dimming <https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/i2c.html>`__ :  
Control commands are sent via the I2C bus to the dimming chip, adjusting the LED brightness by changing the output current of the dimming chip. 
The I2C bus consists of two lines: the data line (SDA) and the clock line (SCL). All ESP chips support dimming chips using hardware I2C communication. 
The following dimming chips are currently supported:

   -  SM2135EH
   -  SM2X35EGH (SM2235EGH/SM2335EGH)
   -  BP57x8D (BP5758/BP5758D/BP5768)
   -  BP1658CJ
   -  KP18058

I2C Dimming Use Case
~~~~~~~~~~~~~~~~~~~~~~

Use the I2C driver to control the BP5758D chip to light up a 5-channel RGBCW bulb. Set the I2C communication rate to 300KHz, with RGB LEDs driven at 50mA and CW LEDs at 30mA. 
Use software color mixing, enable the fade function, and set the power-on color to red.

.. code:: c

    lightbulb_config_t config = {
        // 1. Select the required chip and configure parameters. Each chip has different configuration parameters, so please refer carefully to the chip manual.
        .type = DRIVER_BP57x8D,
        .driver_conf.bp57x8d.freq_khz = 300,
        .driver_conf.bp57x8d.enable_iic_queue = true,
        .driver_conf.bp57x8d.iic_clk = 4,
        .driver_conf.bp57x8d.iic_sda = 5,
        .driver_conf.bp57x8d.current = {50, 50, 50, 30, 30},

        // 2. capability selection: enable/disable according to your needs
        .capability.enable_fade = true,
        .capability.fade_time_ms = 800,
        .capability.enable_lowpower = false,

        .capability.enable_status_storage = true,
        .capability.led_beads = LED_BEADS_5CH_RGBCW,
        .capability.storage_cb = NULL,
        .capability.sync_change_brightness_value = true,

        // 3. Configure the hardware pins for the IIC chip
        .io_conf.iic_io.red = OUT3,
        .io_conf.iic_io.green = OUT2,
        .io_conf.iic_io.blue = OUT1,
        .io_conf.iic_io.cold_white = OUT5,
        .io_conf.iic_io.warm_yellow = OUT4,

        // 4. Limits parameter 
        .external_limit = NULL,

        // 5. Color calibration parameters
        .gamma_conf = NULL,

        // 6. Initialize lighting parameters; if 'on' is set, the lightbulb will light up during driver initialization
        .init_status.mode = WORK_COLOR,
        .init_status.on = true,
        .init_status.hue = 0,
        .init_status.saturation = 100,
        .init_status.value = 100,
    };
    lightbulb_init(&config);

Single-wire dimming scheme
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

A single-wire dimming scheme is a method of controlling LED brightness through a single communication line.
The core idea is to adjust LED brightness by sending control signals through a specific communication protocol. 
This scheme can be implemented on ESP chips using the `RMT peripheral <https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/rmt.html>`__ or `SPI peripheral <https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/spi_master.html>`__.
SPI is recommended for controlling LED communication. The following LED types are currently supported:

-  WS2812

WS2812 Use Case
~~~~~~~~~~~~~~~~

Use the SPI driver to light up 10 WS2812 LEDs, enable the fade function, and set the power-on color to red.

.. code:: c

    lightbulb_config_t config = {
        // 1. Select WS2812 output and configure parameters
        .type = DRIVER_WS2812,
        .driver_conf.ws2812.led_num = 10,
        .driver_conf.ws2812.ctrl_io = 4,

        // 2. capability selection: enable/disable according to your needs
        .capability.enable_fade = true,
        .capability.fade_time_ms = 800,
        .capability.enable_status_storage = true,

        /* For WS2812, only LED_BEADS_3CH_RGB can be selected */
        .capability.led_beads = LED_BEADS_3CH_RGB,
        .capability.storage_cb = NULL,

        // 4. Limits parameter
        .external_limit = NULL,

        // 5. Color calibration parameters
        .gamma_conf = NULL,

        // 6. Initialize lighting parameters; if 'on' is set, the lightbulb will light up during driver initialization
        .init_status.mode = WORK_COLOR,
        .init_status.on = true,
        .init_status.hue = 0,
        .init_status.saturation = 100,
        .init_status.value = 100,
    };
    lightbulb_init(&config);

Fade principle
---------------

The fade effect in the bulb light component is implemented through software. Each channel records the current output value, target value, step size, and remaining steps. 
When the API is used to set a color, it updates the target value, step size, step count, and enables a fade timer. 
This timer triggers a callback every 12ms, where each channel's steps are checked. 
As long as there are steps remaining, the current value is adjusted by the step size and updated in the underlying driver.
When all channels have reached zero steps, the fade is complete, and the timer is stopped.

Low-power implementation process
-----------------------------------

To pass certifications like T20 for low power consumption, after optimizing the light board's power supply, some low-power configurations are needed in the software. 
Apart from the settings mentioned in the `Low-Power Mode Usage Guide <https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/api-guides/low-power-mode.html>`__ the driver logic also requires adjustments. 
In the lightbulb component, low-power adjustments have been added for both PWM and I2C dimming schemes. The specific logic involves the I2C scheme using the dimming chip's low-power command to exit or enter low power when switching the light on or off. 
For the PWM scheme, ESP32 requires power lock management with the APB clock source to prevent flickering—locking power and disabling dynamic frequency scaling when the light is on, and releasing the lock when off, other chips use the XTAL clock source, so no further measures are necessary.

Color calibration scheme
---------------------------

CCT Mode
^^^^^^^^^^^^

    The calibration for color temperature mode requires configuring the following structure.

.. code:: c

    union {
        struct {
            uint16_t kelvin_min;
            uint16_t kelvin_max;
        } standard;
        struct {
            lightbulb_cct_mapping_data_t *table;
            int table_size;
        } precise;
    } cct_mix_mode;


- Standard mode: calibrate the maximum and minimum Kelvin values, use linear interpolation to fill in intermediate values, and then adjust the output ratio of cool and warm LEDs according to the target color temperature.

- Precision mode: calibrate the required output ratios of red, green, blue, cool, and warm LEDs at different color temperatures. Use these calibration points to directly output the corresponding ratios. The more calibration points, the more accurate the color temperature.

Color Mode
^^^^^^^^^^^^

    The calibration for color mode requires configuring the following structure.

.. code:: c

    union {
        struct {
            lightbulb_color_mapping_data_t *table;
            int table_size;
        } precise;
    } color_mix_mode;

- Standard Mode: No parameter configuration is required. Internal theoretical algorithms will convert HSV, XYY, and other color models into RGB ratios, and LEDs will light up directly based on this ratio.

- Precision Mode: Calibrate colors using the HSV model. Measure the required output ratios of red, green, blue, cool, and warm LEDs for various hues and saturation levels as calibration points. Use linear interpolation to fill in intermediate values and light up the LEDs based on these calibrated ratios.


Power limiting scheme
----------------------

Power limiting is used to balance and fine-tune the output current of a specific channel or the overall system to meet power requirements.

To limit the overall power, configure the following structure.

.. code:: c

    typedef struct {
        /* Scale the incoming value
         * range: 10% <= value <= 100%
         * step: 1%
         * default min: 10%
         * default max: 100%
         */
        uint8_t white_max_brightness; /**< Maximum brightness limit for white light output. */
        uint8_t white_min_brightness; /**< Minimum brightness limit for white light output. */
        uint8_t color_max_value;      /**< Maximum value limit for color light output. */
        uint8_t color_min_value;      /**< Minimum value limit for color light output. */

        /* Dynamically adjust the final power
         * range: 100% <= value <= 500%
         * step: 10%
         */
        uint16_t white_max_power;     /**< Maximum power limit for white light output. */
        uint16_t color_max_power;     /**< Maximum power limit for color light output. */
    } lightbulb_power_limit_t;

- ``white_max_brightness`` and ``white_min_brightness`` are used in color temperature mode to constrain the ``brightness`` parameter set by the color temperature API within these maximum and minimum values.
- ``color_max_value`` and ``color_min_value`` are used in color mode to constrain the ``value`` parameter set by the color API within these maximum and minimum values.
- ``white_max_power`` is used to limit power in color temperature mode. The default value is 100, meaning the maximum output power is half of the full power; if set to 200, it can reach the maximum power of the cool and warm LEDs.
- ``color_max_power`` is used to limit power in color mode. The default value is 100, meaning the maximum output power is one-third of full power; if set to 300, it can reach the maximum power of the red, green, and blue LEDs.

To limit the power of individual LEDs, configure the following structure:

.. code:: c

    typedef struct {
        float balance_coefficient[5]; /**< Array of float coefficients for adjusting the intensity of each color channel (R, G, B, C, W).
                                           These coefficients help in achieving the desired color balance for the light output. */

        float curve_coefficient;      /**< Coefficient for gamma correction. This value is used to modify the luminance levels
                                           to suit the non-linear characteristics of human vision, thus improving the overall
                                           visual appearance of the light. */
    } lightbulb_gamma_config_t;
  

- ``balance_coefficient`` is used to fine-tune the output current of each LED. The final output of the driver will be reduced according to this ratio, with a default value of 1.0, meaning no reduction.
- ``curve_coefficient`` is used to convert linear changes during fading into curve-based changes.

.. Note::

    Modifying ``balance_coefficient`` will affect the accuracy of color calibration, so this parameter should be adjusted before performing color calibration. 
    This parameter is especially useful for I2C dimming chips that output current in multiples of 5 or 10; if specific currents are needed, this parameter can be used for adjustment.


API Reference
-----------------

.. include-build-file:: inc/lightbulb.inc
