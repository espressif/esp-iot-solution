# BTHome Dimmer Example Instructions

The "BTHome Dimmer" is based on the "BTHome component" and demonstrates how to implement a general Bluetooth dimmer with a button and a knob using the BTHome protocol. It sends encrypted broadcast packets compliant with the BTHome protocol via BLE broadcasts.

Software functionality:

- Reads the values of the button and knob and sends them via BLE ADV.
- Broadcasts only on button press or knob rotation; advertising stops automatically after 300 ms.
- Supports automatic light sleep with RTC IO wakeup for button and knob events.

### Supported Targets

- **ESP32-H2**: Default target, matches the ESP-Dimmer hardware reference design.
- **ESP32-H4**: Low-power optimized; `sdkconfig.defaults.esp32h4` is applied automatically when this target is selected.

### Hardware Requirements

Refer to the ESP-Dimmer hardware design, open-source address: https://oshwhub.com/esp-college/esp32-h2-switch

### GPIO Pin Constraints

- **ESP32-H4**: Button and knob pins must be RTC GPIO 0–5. The RTC IO driver (`CONFIG_EXAMPLE_BUTTON_KNOB_USE_RTC_IO`) is enabled by default.
- **ESP32-H2**: When light sleep peripheral power-down is enabled, button and knob pins must be RTC wakeup GPIO 7–14.

### Compilation and Flashing

1. Navigate to the `dimmer` directory:

   ```
   cd ./esp-iot-solution/examples/bluetooth/ble_adv/bthome/dimmer
   ```

2. Use the `idf.py` tool to set the target chip for compilation, then compile and download with the following commands:

   ```
   # ESP32-H2 (default hardware reference design)
   idf.py set-target esp32h2

   # ESP32-H4 (low-power optimized, loads sdkconfig.defaults.esp32h4 automatically)
   idf.py set-target esp32h4

   # Compile and download
   idf.py -p PORT build flash
   ```

   Replace `PORT` with the current port number you are using.

### Power Consumption Analysis

The example uses power management with dynamic frequency scaling and automatic light sleep:

- **ESP32-H4 defaults** (via `sdkconfig.defaults.esp32h4`): CPU DFS at 16–32 MHz, FreeRTOS tickless idle, peripheral power-down in light sleep, BLE controller sleep, and RTC IO wakeup for button/knob.
- **RTC IO driver**: On ESP32-H4, button and knob use `iot_button_new_rtc_device()` and `iot_knob_create_rtc()` for low-power wakeup instead of the GPIO driver.
- **External 32 kHz crystal**: Recommended on ESP32-H4 (`CONFIG_RTC_CLK_SRC_EXT_CRYS`) for lower sleep current than the internal RC oscillator.

GPIO and CPU frequency options can be adjusted under `Example Configuration` in menuconfig.

### Example Output

To optimize power consumption, logging output is disabled by default in the configuration. When pressing and rotating, you can use a mobile app to search for a device named `DIY`.

For more details on the BTHome implementation, please refer to the [BTHome Homepage](https://bthome.io/). For instructions on how to integrate BTHome devices into HomeAssistant, please refer to the [BTHome Guide](https://www.home-assistant.io/integrations/bthome/).
