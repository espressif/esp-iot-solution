# Example of a temperature sensor application

[中文版](./README_CN.md)

This example demonstrates how to develop a thermistor sensor temperature reading application using a 'ntc_driver' component on an ESP32-C2 / ESP32-C3 / ESP32 / ESP32-C6 / ESP32-S3 series chip. The specific features demonstrated are as follows:

- Control based on the lowest level of driver APIs
- Thermistor CMFA103J3950HANT-driven demo
- Optionally configurable with different thermistor drives

CMFA103J3950HANT Sensor wiring diagram:

    ```
    ESP8684-DevKitM-1       CMFA103J3950HANT
                            --    
    [ADC1 CHANEL3]  --------> | || NTC IO
                            --     
    ```

## How to use the sample

### Hardware requirements

1. Select the driver to be demonstrated via 'idf.py menuconfig' and configure the driver

2. Hardware Connection:
    - For CMFA103J3950HANT drivers, only the signal port needs to be connected to the configured GPIO.

### Compile and burn

1. Go to the '**' directory:

    ```linux
    cd ./esp-iot-solution/examples/sensors/ntc_temperature_sensor
    ```

2. Use the 'idf.py' tool to set up the compilation chip, and then compile and download it with the following commands:

    ```linux
    # Set up the compilation chip
    idf.py set-target esp32c2

    # Compile and download
    idf.py -p PORT build flash
    ```

Please replace 'PORT' with the port number you are currently using

3. You can use 'monitor' to view the output of the program, and the command is:

    ```
    idf.py -p PORT monitor
    ``` 


### Sample output

Here's the CMFA103J3950HANT-driven test log:

```log
I (333) app_start: Starting scheduler on CPU0
I (337) app_start: Starting scheduler on CPU1
I (337) main_task: Started on CPU0
I (347) main_task: Calling app_main()
I (347) ntc driver: IoT Ntc Driver Version: 0.1.0
I (347) ntc driver: calibration scheme version is Line Fitting
I (357) ntc driver: Calibration Success
I (367) NTC demo: NTC temperature = 23.44 ℃
I (367) NTC demo: NTC temperature = 23.47 ℃
I (377) NTC demo: NTC temperature = 23.47 ℃
I (377) NTC demo: NTC temperature = 23.47 ℃
I (387) NTC demo: NTC temperature = 23.50 ℃
I (387) NTC demo: NTC temperature = 23.50 ℃
I (397) NTC demo: NTC temperature = 23.44 ℃
I (397) NTC demo: NTC temperature = 23.44 ℃
I (407) NTC demo: NTC temperature = 23.43 ℃
I (407) NTC demo: NTC temperature = 23.43 ℃
```
