# BTHome Dimmer Example Instructions

The "BTHome Dimmer" is based on the "BTHome component" and demonstrates how to implement a general Bluetooth dimmer with a button and a knob using the BTHome protocol. It sends encrypted broadcast packets compliant with the BTHome protocol via BLE broadcasts.

Software functionality:

- Reads the values of the button and knob and sends them via BLE ADV.
- Optimizes low-power configuration options.

### Hardware Requirements:

Refer to the ESP-Dimmer hardware design, open-source address: https://oshwhub.com/esp-college/esp32-h2-switch

### Compilation and Flashing

1. Navigate to the `dimmer` directory:

   ```
   cd ./esp-iot-solution/examples/bluetooth/ble_adv/bthome/dimmer
   ```

2. Use the `idf.py` tool to set the target chip for compilation, then compile and download with the following commands:

   ```
   # Set the target chip for compilation
   idf.py set-target esp32h2

   # Compile and download
   idf.py -p PORT build flash
   ```

   Replace `PORT` with the current port number you are using.

### Power Consumption Analysis

### Example Output

To optimize power consumption, logging output is disabled by default in the configuration. When pressing and rotating, you can use a mobile app to search for a device named `DIY`.

For more details on the BTHome implementation, please refer to the [BTHome Homepage](https://bthome.io/). For instructions on how to integrate BTHome devices into HomeAssistant, please refer to the [BTHome Guide](https://www.home-assistant.io/integrations/bthome/).