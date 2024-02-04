# Lightbulb Component

👉 [中文版](./README_CN.md)

The lightbulb component encapsulates several commonly used dimming schemes for lightbulbs, managing these schemes through an abstract layer for easy integration into developers' applications. Currently, support has been extended to all ESP32 series chips.

- PWM Scheme:

  - RGB + C/W
  - RGB + CCT/Brightness

- IIC Dimming Scheme

  - ~~SM2135E~~
  - SM2135EH
  - SM2X35EGH (SM2235EGH/SM2335EGH)
  - BP57x8D (BP5758/BP5758D/BP5768)
  - BP1658CJ
  - KP18058

- Single Bus Scheme:

  - WS2812

## Supported common functionalities

- Dynamic Effects: Supports various color transitions using fades, and allows configuring periodic breathing and blinking effects.
- Calibration: Enables fine-tuning of output data using coefficients to achieve white balance functionality, and supports gamma calibration curves.
- Status Storage: Utilizes the `NVS` component to store the current state of the lightbulb, facilitating features like power loss memory.
- LED Configuration: Supports up to 5 types of LED configurations, with the following combinations:
  - Single-channel, cold or warm temperature LED, capable of brightness control under a single color temperature.
  - Dual-channel, cold and warm LEDs, enabling control of both color temperature and brightness.
  - Tri-channel, red, green, and blue LEDs, allowing for arbitrary color control.
  - Four-channel, red, green, blue, and cold or warm temperature LEDs, enabling color control and brightness control under a single color temperature. If a mixing table is configured, different color temperatures can be achieved by mixing these LEDs, enabling color temperature control.
  - Five-channel, red, green, blue, cold, and warm temperature LEDs, enabling color and color temperature-controlled brightness.
- Power Limitation: Balances the output power under different color temperatures and colors.
- Low Power: Reduces overall power consumption without compromising dynamic effects.
- Software Color Temperature: Applicable for PWM-driven scenarios where hardware color temperature adjustment is not performed.

## Example of PWM Scheme

The PWM scheme is implemented using the LEDC driver, supporting both software and hardware fade functionalities. It also automatically configures resolution based on frequency. An example of usage is provided below:

```c
lightbulb_config_t config = {
    // 1. Select PWM output and configure parameters
    .type = DRIVER_ESP_PWM,
    .driver_conf.pwm.freq_hz = 4000,

    // 2. Capability Selection: Enable/Disable Based on Your Needs
    .capability.enable_fade = true,
    .capability.fade_time_ms = 800,
    .capability.enable_lowpower = false,
    /* If your driver controls white light output separately through hardware instead of software mixing, enable this feature. */
    .capability.enable_hardware_cct = true,
    .capability.enable_status_storage = true,
    /* Used to configure the combination of LED beads */
    .capability.led_beads = LED_BEADS_3CH_RGB,
    .capability.storage_cb = NULL,
    .capability.sync_change_brightness_value = true,

    // 3. Configure hardware pins for PWM output
    .io_conf.pwm_io.red = 25,
    .io_conf.pwm_io.green = 26,
    .io_conf.pwm_io.blue = 27,

    // 4. Limit parameters, defaults are usually sufficient
    .external_limit = NULL,

    // 5. Calibration parameters, defaults are usually sufficient
    .gamma_conf = NULL,

    // 6. Initialize status parameters; if "on" is set, the light will turn on during driver initialization.
    .init_status.mode = WORK_COLOR,
    .init_status.on = true,
    .init_status.hue = 0,
    .init_status.saturation = 100,
    .init_status.value = 100,
};
lightbulb_init(&config);
```

## Example of IIC Scheme

The IIC dimming chip solution now supports configuring all parameters of the IIC dimming chip. Please refer to the manual for the specific functions and parameters of the dimming chip and fill in accordingly. An example of usage is provided below:

```c
lightbulb_config_t config = {
    // 1. Select the desired chip and configure parameters. Each chip has different configuration parameters. Please carefully refer to the chip manual.
    .type = DRIVER_BP57x8D,
    .driver_conf.bp57x8d.freq_khz = 300,
    .driver_conf.bp57x8d.enable_iic_queue = true,
    .driver_conf.bp57x8d.iic_clk = 4,
    .driver_conf.bp57x8d.iic_sda = 5,
    .driver_conf.bp57x8d.current = {50, 50, 60, 30, 50},

    // 2. Capability Selection: Enable/Disable Based on Your Needs
    .capability.enable_fade = true,
    .capability.fade_time_ms = 800,
    .capability.enable_lowpower = false,

    .capability.enable_status_storage = true,
    .capability.led_beads = LED_BEADS_5CH_RGBCW,
    .capability.storage_cb = NULL,
    .capability.sync_change_brightness_value = true,

    // 3. Configure hardware pins for the IIC chip.
    .io_conf.iic_io.red = OUT3,
    .io_conf.iic_io.green = OUT2,
    .io_conf.iic_io.blue = OUT1,
    .io_conf.iic_io.cold_white = OUT5,
    .io_conf.iic_io.warm_yellow = OUT4,

    // 4. Limit parameters, defaults are usually sufficient
    .external_limit = NULL,

    // 5. Calibration parameters, defaults are usually sufficient
    .gamma_conf = NULL,

    // 6. Initialize status parameters; if "on" is set, the light will turn on during driver initialization.
    .init_status.mode = WORK_COLOR,
    .init_status.on = true,
    .init_status.hue = 0,
    .init_status.saturation = 100,
    .init_status.value = 100,
};
lightbulb_init(&config);
```

## Example of Single-bus Scheme

Single-bus scheme utilizes the SPI driver to output data for WS2812 LEDs, with data packaging sequence set as GRB.

```c
lightbulb_config_t config = {
    // 1. Select WS2812 output and configure parameters
    .type = DRIVER_WS2812,
    .driver_conf.ws2812.led_num = 22,
    .driver_conf.ws2812.ctrl_io = 4,

    // 2. Capability Selection: Enable/Disable Based on Your Needs
    .capability.enable_fade = true,
    .capability.fade_time_ms = 800,
    .capability.enable_status_storage = true,

    /* For WS2812, you can only choose LED_BEADS_3CH_RGB */
    .capability.led_beads = LED_BEADS_3CH_RGB,
    .capability.storage_cb = NULL,

    // 3. Limit parameters, defaults are usually sufficient
    .external_limit = NULL,

    // 4. Calibration parameters, defaults are usually sufficient
    .gamma_conf = NULL,

    // 5. Initialize status parameters; if "on" is set, the light will turn on during driver initialization.
    .init_status.mode = WORK_COLOR,
    .init_status.on = true,
    .init_status.hue = 0,
    .init_status.saturation = 100,
    .init_status.value = 100,
};
lightbulb_init(&config);
```

## Example of Limit Parameters

The primary purpose of limit parameters is to restrict the maximum output power and constrain the brightness parameters within a specific range. This component allows independent control of colored light and white light, which results in two sets of maximum/minimum brightness parameters and power parameters. Colored light uses the HSV model, with the value representing colored light brightness, while white light uses the brightness parameter. The input range for both value and brightness is 0 <= x <= 100.

If brightness limitation parameters are set, the input values will be proportionally scaled. For instance, consider the following parameters:

```c
lightbulb_power_limit_t limit = {
    .white_max_brightness = 100,
    .white_min_brightness = 10,
    .color_max_value = 100,
    .color_min_value = 10,
    .white_max_power = 100,
    .color_max_power = 100
}
```

The relationship between value and brightness inputs and outputs is as follows:

```c
input   output
100      100
80       82
50       55
11       19
1        10
0        0
```

Power limit is applied after the brightness parameter limitations, and for the RGB channel adjustment, the range is 100 <= x <= 300. The relationship between input and output is as follows:

```c
input           output(color_max_power = 100)       output(color_max_power = 200)       output(color_max_power = 300)
255,255,0       127,127,0                           255,255,0                           255,255,0
127,127,0       63,63,0                             127,127,0                           127,127,0
63,63,0         31,31,0                             63,63,0                             63,63,0
255,255,255     85,85,85                            170,170,170                         255,255,255
127,127,127     42,42,42                            84,84,84                            127,127,127
63,63,63        21,21,21                            42,42,42                            63,63,63

```

## Example of Calibration Parameters

Color calibration parameters consist of two components: curve coefficients `curve_coefficient` used to generate the gamma grayscale table, and white balance coefficients `balance_coefficient` used for the final adjustments.

- Curve coefficients have values within the range of 0.8 <= x <= 2.2. The output for each parameter is as follows:

```c

   |  x = 1.0                           | x < 1.0                          | x > 1.0
max|                                    |                                  |
   |                                *   |                     *            |                           *
   |                             *      |                  *               |                          *
   |                          *         |               *                  |                         *
   |                       *            |             *                    |                       *
   |                    *               |           *                      |                     *
   |                 *                  |         *                        |                   *
   |              *                     |       *                          |                 *
   |           *                        |     *                            |              *
   |        *                           |    *                             |           * 
   |     *                              |   *                              |        *
   |  *                                 |  *                               |  *
0  |------------------------------------|----------------------------------|------------------------------   
  0               ...                255 
```

The component will generate a gamma calibration table based on the maximum input values supported by each driver. All 8-bit RGB values will be converted to their calibrated counterparts. It is recommended to set the same coefficient for the RGB channels.

- White balance coefficients have values within the range of 0.5 to 1.0. They are used for final fine-tuning of the data, and the calculation rule is: output value = input value * coefficient. Different coefficients can be set for each channel.

## Example Code

Click [here](https://github.com/espressif/esp-iot-solution/tree/master/examples/lighting/lightbulb) to access the example code and usage instructions.
