# BTHome Bulb Example Instructions

The "BTHome Bulb" is based on the "BTHome component" and receives encrypted broadcast packets compliant with the BTHome protocol via BLE scan. It pairs with the [dimmer example](../dimmer/) and controls an LED strip based on button and dimmer events.

Software functionality:

- Passively scans for BTHome broadcasts from the dimmer (filtered by MAC address accept list).
- Decrypts and parses BTHome button and dimmer events.
- Toggles the LED on/off on button click and adjusts brightness on knob rotation.

### Supported Targets

- **ESP32-H2**: Default target, pairs with the dimmer example on ESP32-H2.
- **ESP32-H4**: Supported; LED strip GPIO defaults to 37 (typical EV board wiring).

### Hardware Requirements

- An ESP32 board with BLE support.
- A WS2812 LED strip connected to the configured GPIO data line (default GPIO 8; GPIO 37 on ESP32-H4).

LED strip GPIO, LED count, and RMT resolution can be adjusted under `BTHome Bulb Configuration` in menuconfig.

### Compilation and Flashing

1. Navigate to the `bulb` directory:

   ```
   cd ./esp-iot-solution/examples/bluetooth/ble_adv/bthome/bulb
   ```

2. Use the `idf.py` tool to set the target chip for compilation, then compile and download with the following commands:

   ```
   # ESP32-H2
   idf.py set-target esp32h2

   # ESP32-H4
   idf.py set-target esp32h4

   # Compile and download
   idf.py -p PORT build flash
   ```

   Replace `PORT` with the current port number you are using.

### Example Output

Flash the [dimmer example](../dimmer/) on a separate board, then flash this bulb example. Press the dimmer button to toggle the LED, or rotate the knob to adjust brightness.

For more details on the BTHome implementation, please refer to the [BTHome Homepage](https://bthome.io/). For instructions on how to integrate BTHome devices into HomeAssistant, please refer to the [BTHome Guide](https://www.home-assistant.io/integrations/bthome/).
