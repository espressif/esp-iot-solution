# Lightbulb Component

👉 [中文版](./README_CN.md)

The lightbulb component encapsulates several commonly used dimming schemes for lightbulbs, managing these schemes through an abstract layer for easy integration into developers' applications. Currently, support has been extended to all ESP32 series chips.

- PWM Scheme:

  - RGB + C/W
  - RGB + CCT/Brightness
- IIC Dimming Scheme

  - ~~SM2135E~~
  - SM2135EH
  - SM2182E
  - SM2X35EGH (SM2235EGH/SM2335EGH)
  - BP57x8D (BP5758/BP5758D/BP5768)
  - BP1658CJ
  - KP18058
- Single Bus Scheme:

  - WS2812
  - SM16825E

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

### WS2812 Usage example

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

### SM16825E usage example

The SM16825E is an RGBWY five-channel LED driver that uses an SPI interface and RZ coding protocol, supporting 16-bit grey scale control and current regulation.

```cpp
lightbulb_config_t config = {
    //1. Select SM16825E output and configure parameters
    .type = DRIVER_SM16825E,
    .driver_conf.sm16825e.led_num = 1,        // Number of LED chips
    .driver_conf.sm16825e.ctrl_io = 9,        // Control GPIO pin
    .driver_conf.sm16825e.freq_hz = 3333000,  // SPI frequency (default: 3.33MHz, auto-calculated based on RZ protocol timing)

    //2. Driver capability selection, enable/disable according to your needs
    .capability.enable_fade = true,
    .capability.fade_time_ms = 800,
    .capability.enable_lowpower = false,
    .capability.enable_status_storage = true,
    .capability.led_beads = LED_BEADS_5CH_RGBCW,  // Supports 5-channel RGBWY
    .capability.storage_cb = NULL,
    .capability.sync_change_brightness_value = true,

    //3. Limit parameters, see details in later sections
    .external_limit = NULL,

    //4. Color calibration parameters
    .gamma_conf = NULL,

    //5. Initialize lighting parameters; if "on" is set, the light will turn on during driver initialization
    .init_status.mode = WORK_COLOR,
    .init_status.on = true,
    .init_status.hue = 0,
    .init_status.saturation = 100,
    .init_status.value = 100,
};
lightbulb_init(&config);

// Optional: Configure channel mapping (if custom pin mapping is needed)
sm16825e_regist_channel(SM16825E_CHANNEL_R, SM16825E_PIN_OUTR);
sm16825e_regist_channel(SM16825E_CHANNEL_G, SM16825E_PIN_OUTG);
sm16825e_regist_channel(SM16825E_CHANNEL_B, SM16825E_PIN_OUTB);
sm16825e_regist_channel(SM16825E_CHANNEL_W, SM16825E_PIN_OUTW);
sm16825e_regist_channel(SM16825E_CHANNEL_Y, SM16825E_PIN_OUTY);

// Optional: Set channel current (10-300mA)
sm16825e_set_channel_current(SM16825E_CHANNEL_R, 100);  // 100mA
sm16825e_set_channel_current(SM16825E_CHANNEL_G, 100);  // 100mA
sm16825e_set_channel_current(SM16825E_CHANNEL_B, 100);  // 100mA
sm16825e_set_channel_current(SM16825E_CHANNEL_W, 100);  // 100mA
sm16825e_set_channel_current(SM16825E_CHANNEL_Y, 100);  // 100mA

// Optional: Enable/disable standby mode
sm16825e_set_standby(false);  // Disable standby mode, enable normal operation

// Optional: Shutdown all channels
// sm16825e_set_shutdown();
```

**Features of SM16825E**

* Supports RGBWY five-channel independent control
* 16-bit grayscale resolution (65,536 levels)
* Adjustable current control (10-300mA per channel)
* Supports standby mode (power consumption <2mW)
* RZ encoding protocol, 800Kbps data transmission rate, 1200ns code period
* Support multi-chip cascading
* Auto-timing calculation: dynamically calculates SPI frequency and bit patterns based on datasheet parameters
* Flexible channel mapping: supports arbitrary mapping from logical channels to physical pins
* Optimized SPI transmission: uses 3.33MHz SPI frequency, each SM16825E bit encoded with 4 SPI bits

#### SM16825E Advanced Features

##### Channel Mapping Configuration

SM16825E supports flexible channel mapping, allowing logical channels (R, G, B, W, Y) to be mapped to arbitrary physical pins:

```c
// Custom channel mapping example
sm16825e_regist_channel(SM16825E_CHANNEL_R, SM16825E_PIN_OUT1);  // Red mapped to OUT1
sm16825e_regist_channel(SM16825E_CHANNEL_G, SM16825E_PIN_OUT2);  // Green mapped to OUT2
sm16825e_regist_channel(SM16825E_CHANNEL_B, SM16825E_PIN_OUT3);  // Blue mapped to OUT3
sm16825e_regist_channel(SM16825E_CHANNEL_W, SM16825E_PIN_OUT4);  // White mapped to OUT4
sm16825e_regist_channel(SM16825E_CHANNEL_Y, SM16825E_PIN_OUT5);  // Yellow mapped to OUT5
```

##### Current Control

Each channel can independently set current value, range 10-300mA:

```c
// Set current for different channels
sm16825e_set_channel_current(SM16825E_CHANNEL_R, config->current[SM16825E_CHANNEL_R]);  // Red Channel
sm16825e_set_channel_current(SM16825E_CHANNEL_G, config->current[SM16825E_CHANNEL_G]);  // Green Channel
sm16825e_set_channel_current(SM16825E_CHANNEL_B, config->current[SM16825E_CHANNEL_B]);  // Blue Channel
sm16825e_set_channel_current(SM16825E_CHANNEL_W, config->current[SM16825E_CHANNEL_W]);  // White Channel
sm16825e_set_channel_current(SM16825E_CHANNEL_Y, config->current[SM16825E_CHANNEL_Y]);  // Yellow Channel
```

##### Standby Mode Control

Supports standby mode to reduce power consumption:

```c
// Enable standby mode (power consumption <2mW)
sm16825e_set_standby(true);

// Disable standby mode, resume normal operation
sm16825e_set_standby(false);
```

##### Direct Channel Control

Can directly set RGBWY channel values (0-255):

```c
// Set channel values
sm16825e_set_rgbwy_channel(255, 128, 64, 192, 96);
// Parameter order: red, green, blue, white, yellow

// Shutdown all channels
sm16825e_set_shutdown();
```

##### Timing Optimization Details

The driver automatically calculates optimal timing based on datasheet parameters:

- **RZ Encoding**: 800Kbps effective transmission rate, 1200ns code period
- **SPI Frequency**: 3.33MHz, each SM16825E bit encoded with 4 SPI bits
- **Timing Parameters**:
  - T0 bit: 300ns high level + 900ns low level
  - T1 bit: 900ns high level + 300ns low level
  - Reset signal: minimum 200μs low level

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

|    input    | output(color_max_power = 100) | output(color_max_power = 200) | output(color_max_power = 300) |
| :---------: | :---------------------------: | :---------------------------: | :---------------------------: |
|  255,255,0  |           127,127,0           |           255,255,0           |           255,255,0           |
|  127,127,0  |            63,63,0            |           127,127,0           |           127,127,0           |
|   63,63,0   |            31,31,0            |            63,63,0            |            63,63,0            |
| 255,255,255 |           85,85,85           |          170,170,170          |          255,255,255          |
| 127,127,127 |           42,42,42           |           84,84,84           |          127,127,127          |
|  63,63,63  |           21,21,21           |           42,42,42           |           63,63,63           |

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
