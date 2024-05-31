# Remote Control Hardware tutorial
The default configuration of this example would be to use the boot button on the ESP32 for button input, and reading from the console to emulate joystick input.

If user would like to connect the ESP to external hardware instead. This walkthrough would be for you.

This example was tested using simple switches and HW-504 joystick.

## Menuconfig selection
To enable interfacing with external hardware, user need to do the following:
1. Run `idf.py menuconfig`
2. Select `Example configuration`
3. Under `Joystick Input Mode`, select `ADC`
4. Under `Button Input Mode`, select `GPIO`

The default GPIO pin used are 4,5,6,7. 

## Joystick configuration
The wire connections are as following:

- Connect **3.3V** pin on the ESP to **5V** pin on the HW-504 (**IMPORTANT**)
- Connect `GND` pin on the ESP to `GND` pin on the HW-504
- Connect pin `0` (`ADC_CHANNEL_0`) on the ESP to `VRx` pin on the HW-504
- Connect pin `1` (`ADC_CHANNEL_1`) on the ESP to `VRy` pin on the HW-504

- `SW` pin on the HW-504 is left unconnected

> IMPORTANT NOTE:
> HW-504 works by using potentiometers and ESP32 can only analog read **up to 3.3V**, connecting HW-504 to the `5V` pin on the ESP32 might damage your ESP.

## External button configuration

For this demonstration, the buttons are configured with pull-up enabled. 

```c
const gpio_config_t pin_config = {
    .pin_bit_mask = BUTTON_PIN_BIT_MASK,
    .mode = GPIO_MODE_INPUT,
    .pull_up_en = GPIO_PULLUP_ENABLE,
    .pull_down_en = GPIO_PULLDOWN_DISABLE,
    .intr_type = GPIO_INTR_POSEDGE
};
```