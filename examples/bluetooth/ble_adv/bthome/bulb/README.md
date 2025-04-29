# BTHome bulb Example Instructions

The "BTHome bulb" is based on the "bthome component" and involves receiving and parsing broadcast packets compliant with the BTHome protocol through BLE broadcasts.

Software functionality:

- Receives encrypted broadcast packets sent by the dimmer example and controls the LED light status.

### Compilation and Flashing

1. Navigate to the `bulb` directory:

   ```
   cd ./esp-iot-solution/examples/lighting/bulb
   ```

2. Use the `idf.py` tool to set the target chip for compilation, then compile and download with the following commands:

   ```
   # Set the target chip for compilation
   idf.py set-target esp32h2

   # Compile and download
   idf.py -p PORT build flash
   ```

   Replace `PORT` with the current port number you are using.

### Example Output

For more details on the BTHome implementation, please refer to the [BTHome Homepage](https://bthome.io/).