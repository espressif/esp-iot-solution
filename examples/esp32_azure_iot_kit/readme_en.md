# ESP32-Azure IoT Kit Getting Started Guide

[[CN]](./readme_cn.md)

The ESP32-Azure IoT Kit is a development board, with the ESP32-WROVER-B module at its core. The ESP32-Azure IoT Kit integrates an OLED screen and five sensors. This board can get connected to, and perform data exchange with, a variety of cloud platforms, which is enabled by ESP32's Wi-Fi functionality. This guide introduces the hardware resources of this board and, subsequently, describes the implementation and application of the relevant demo. 

## 1. Hardware of the ESP32-Azure IoT Kit 
The ESP32-Azure IoT Kit consists of the following main parts:
 
  * ESP32 modules: ESP32-WROVER-B
  * Power supply options: USB cable or battery 
  * Sensors: temperature and humidity sensor, ambient brightness sensor, motion sensor, magnetometer, barometer
  * OLED screen: 0.96' OLED screen that ingrates an SSD1306 driver chip
  * Buttons: A reset button and a button for customized use.
  * Indicators: 2 x LEDs and 1 x buzzer
  * I/O connectors for extension: 16 pins
  * A MicroSD card slot

Each part is shown in the following figure. For detailed information about the hardware design and schematics, please refer to [ESP32-Azure IoT Kit Hardware Design Guide](https://www.espressif.com/sites/default/files/documentation/esp32-azure_iot_kit_hardware_design_guide__en.pdf).

<img src="../../documents/_static/esp32_azure_iot_kit/esp32-azure_iot_kit.jpg"> 

## 2. Demo for the ESP32-Azure IoT Kit

### 2.1 Function and Implementation of the Demo
This Demo aims to help developers quickly familiarize themselves with the use of the sensors and the OLED screen on the development board for the following purposes:

  * Read data from the temperature and humidity sensor.
  * Read data from the ambient brightness sensor.
  * Read data from the motion sensor and use the complementary algorithm to calculate the pitch and roll angle values.
  * Display data obtained from sensors on the OLED screen.
  * Switch to the display of different contents by pressing a button. 

All sensors on the board use the I2C interface. So far, the development of drivers for the temperature and humidity sensor, ambient brightness sensor and motion sensor has been completed, which enables users to obtain sensor data by simply calling the corresponding APIs. For more information on driver APIs and the relevant code, please check [[esp-iot-solution/components/i2c_devices/sensor]](./../../components/i2c_devices/sensor).
   
The OLED screen also uses the I2C interface. The development of the OLED driver has also been completed. For details, please refer to [[esp-iot-solution/components/i2c_devices/others/ssd1306]](./../../components/i2c_devices/others/ssd1306).

> Notice: 
> 
> The drivers for the magnetometer and barometer are still under development. Please refer to the drivers we have already developed (for the temperature and humidity sensor, ambient brightness sensor and motion sensor), so you can develop your own drivers for the magnetometer and barometer in your implementation.

### 2.2 Configuration and Downloading

Please refer to [[readme]](./../../README.md#preparation)

### 2.3 Operating

After the download, please press the KEY_EN button, so that the board starts operating. During its operation, the OLED screen displays simple icons and sensor data.
To display different contents, the user needs to press the KEY_IO0 button. This way, the OLED

  * Displays the sensor data read from the temperature and humidity sensor, as well as the ambient brightness sensor.
  * Displays the acceleration read from the motion sensor.
  * Displays the angular velocity (gyroscope) read from the motion sensor.
  * Displays the pitch and roll angle values obtained from the motion sensor and calculated with the complementary algorithm.

The respective screenshots can be seen below:

<img src="../../documents/_static/esp32_azure_iot_kit/oled_show_pages.png">



