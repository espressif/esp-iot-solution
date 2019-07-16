[[中文]](./readme_cn.md)

# User Guide for Developing Low Power Applications

In order to help customers get familiar with the development of low-power applications, we have designed a low-power application demo (as a reference design) based on our ESP32-MeshKit-Sense development board and Espressif Low Power Software Framework.

This document first describes the hardware requirement, software design and the usage of this demo, then provides information on how to measure the power consumption of the board in different modes, and presents the measuring results.

## 1. How to Use the Hardware

The hardware includes:

  * 1 x development board（ESP32-MeshKit-Sense）
  * 1 x downloader（ESP-Prog）
  * 1 x E-ink screen（optional）
  * 1 x lithium battery (power supply via USB, optional)

<img src="../../documents/_static/lowpower_evb/low_power_evb_kit.png" width = "600">  

### 1.1 Development Board

The figure below demonstrates all dev board components. For the hardware design, please refer to [ESP32-MeshKit-Sense Hardware Design Guidelines](https://github.com/espressif/esp-iot-solution/blob/master/documents/evaluation_boards/ESP32-MeshKit-Sense_guide_en.md).

<img src="../../documents/_static/lowpower_evb/low_power_evb.png" width = "600">  

Jumper caps can be used at 5 places on the board. Please find the table below detailing their functions and placement:

| Location   | Function | Placement |
|------------|----------|-----------|
| EXT5V-CH5V | Connects the 5V USB power supply with the input pin of the charge management chip. When these pins are shorted, the battery charging is enabled. | Place a jumper cap during battery charging  |
| CHVBA-VBA  | Connects the output pin of the charge management chip with the battery cathode. When these pins are shorted, the battery charging is enabled. | Place a jumper cap during battery charging |
| DCVCC-SUFVCC | Connects the power supply with the input of DC-DC converter, which is used while measuring the power consumption of the whole board. | Place a jumper cap when the dev board is powered up via battery or USB. |
| 3.3V-PER_3.3V | Connects the 3.3V power supply with the power supply for all peripherals, which is used while measuring the power consumption of peripherals | Place a jumper cap when peripherals are used. |
| 3.3V-3.3V_ESP32 | Connects the 3.3V power supply with the power supply for the module, which is used while measuring the power consumption of the module (as well as Reset Protect chip) | Place a jumper cap when modules are used. |

### 1.2 Downloader

The detailed user guide can be found in [ESP-Prog Guide](https://github.com/espressif/esp-iot-solution/blob/master/documents/evaluation_boards/ESP-Prog_guide_en.md).

As the current version of dev board cannot be powered up by ESP-Prog, a jumper cap should be used to connect IO0_ON/OFF, as shown in the figure below.

<img src="../../documents/_static/lowpower_evb/esp_prog_download_header.png" width = "600">

### 1.3 E-ink Screen

The E-ink screen is an optional external device, which can be used in accordance with users' specific requirements. A reference design for the E-ink screen is also provided in this demo.

<img src="../../documents/_static/lowpower_evb/e_ink.png">

### 1.4 Lithium Battery

The lithium battery is also an optional external device, which can be used when a USB power supply is not available. If you want to power up the board using a battery, please make sure that a USB cable is not connected to the board. Otherwise, the board automatically chooses to be powered by USB.

> Note: please use 3.7V lithium batteries.

## 2. How to Use This Demo

### 2.1 Pre-conditions

* Espressif Low Power Development Kit
* Mobile phone: Install Espressif network configuration app
	* For Android phones, please install [EspTouch](https://github.com/EspressifApp/EsptouchForAndroid/tree/master/releases/apk)
	* For iOS phones, please install `Espressif EspTouch`, which can be downloaded from the Apple App Store
* Router: 2.4G router
* Server: Use the script `sensor_server.py` to build a simple server, which can be used to receive and print data from the development board.
	* The port number that the server uses should be passed to the script, for example: `python sensor_server.py 8000`

### 2.2 Compiling Environment Configuration

* To install and configure the C compiling environment for the development of ESP32 products, please refer to [this link](https://docs.espressif.com/projects/esp-idf/en/stable/get-started/index.html)
* To install and configure the toolchain for ULP, please refer to [this link](https://docs.espressif.com/projects/esp-idf/en/stable/api-guides/ulp.html)

### 2.3 Complier Download

Please go to **menuconfig**, and configure the following parameters under `Lowpower EVB settings`:

* `Set lowpower evb task stack size`: configure the stack size of the task.
* `Wake up type`: the board can be woken up by a ULP, GPIO or Timer.
	* If the Timer Wake-up is enabled, the wake-up interval should also be configured with the `wake up time(us)` option.
* `WiFi configure mode`: the network configuration can be done with ESP-TOUCH or by connecting the board to a designated router.
	* If you want to connect this device to a specific router, please set the SSID and password using the `Set SSID of fixed router` and `Set password of fixed router` options, respectively, which eliminates the need for network configuration.
* `Set server IP to connect to`: IP address of the server to be connected.
* `Set server port to connect to`: Port number of the server to be connected.
* `Set upload data size`: Maximum size of the uploaded data in bytes.
* `Read sensor value number`: Number of times for sensor reading in each deep sleep cycle.
* `Read sensor interval(ms)`: Set the time interval between the sensor readings.
* `Enable E-ink`: Indicate if E-ink is enabled or not.

### 2.4 Operation Procedure

This section details the steps for running this demo.

1. Compile and download firmware.
2. Supply power to the device and start network configuration:
	* If the network has not been configured before, the device enters Network Configuration mode. At this point, the Wi-Fi LED indicator blinks rapidly.
	* If the network has been configured before, the device requires network re-configuration. At this point, please make sure the device is in Wake-up mode:
		* Wi-Fi indicator light on: Device is in Wake-up mode. Now press Wakeup button, the Wi-Fi indicator should start blinking rapidly, which indicates that the device has now entered Network Configuration mode.
		* Wi-Fi indicator light off: Device is in Sleep mode. Now press Wakeup button to wake up the device, the Wi-Fi indicator light should turn on. Then press Wakeup button again, the Wi-Fi indicator should start blinking rapidly, which indicates that the device has now entered Network Configuration mode.
3. Connect mobile phones to the corresponding routers and start the network configuration app. After the network is configured successfully, the Wi-Fi indicator light turns on, which shows that the device has been successfully connected to the router.
4. If the device is woken up not from Deep-sleep mode, but, for example, as a result of restart or reset, the device enters Deep-sleep mode and then gets woken up by a ULP, GPIO or Timer, depending on the `Wake up type` setting:
	* GPIO Wake-up - the device wakes up if the Wakeup button is pressed. After that, the device's CPU reads sensor data via I2C interface.
	* Timer Wake-up - the device automatically wakes up after a specified time interval. After that, the device's CPU reads sensor data via I2C interface.
	* ULP Wake-up - the device wakes up after ULP has finished reading sensor data for a certain number of times, which is specified in `Read sensor value number`. After that, the device's CPU reads sensor data from ULP.
5. After being woken up, the device automatically connects to the pre-configured router.
6. Use the `sensor_server.py` script to build a server.
7. ESP32 connects to the server and sends the data readings from the sensor to this server. At this point, the Network indicator light shows the status of ESP32's connection to the server:
	* Blinking: ESP32 is trying to connect to the server.
	* On: ESP32 has been successfully connected to the server.
8. If the data is sent to the server, the E-ink screen will refresh and display the last set of data read by ULP.
9. Then the device enters deep sleep and waits for the next wake-up.

## 3. Software Framework

### 3.1  Overview of Low Power Software Framework

The software framework of Espressif Low Power Solution offers a typical application flow by combining the Deep-sleep design with other functions. For the code-level implementation of the framework, please refer to the folder: `esp-iot-solution/components/framework/lowpower_framework`. This framework eliminates the need to design your own flows. All you have to do is call the corresponding functions for their designs, which helps you greatly save on development time and costs.

The software framework for Espressif Low Power Solution mainly consists of:

* Network configuration and connection
* Sleep and Wake-up modes
* Read sensor data
* Send data to server
* Process after the data is sent to the server

Please see the flow chart below:

<img src="../../documents/_static/lowpower_evb/workflow_of_framwork.png">

> The "ULP Running", which can be seen in the figure above in the dotted box, only occurs when the ULP Wake-up is enabled as the wake-up source. For details, please see Section 3.3.

### 3.2 Documents and Interface

See the main structure of the demo below.

    lowpower_evb
    ├── components
    │   ├── lowpower_evb_callback              // Implementation of the callback function
    │   │   ├── lowpower_evb_callback.cpp
    │   │   ├── component.mk
    │   │   └── include
    │   │       └── lowpower_evb_callback.h
    │   ├── lowpower_evb_peripherals           // Peripherals operation
    │   │   ├── lowpower_evb_adc.cpp           // ADC operation
    │   │   ├── lowpower_evb_epaper.cpp        // E-ink operation
    │   │   ├── lowpower_evb_power.cpp         // Power switch of E-ink and sensor
    │   │   ├── lowpower_evb_sensor.cpp        // Sensor operation
    │   │   ├── lowpower_evb_status_led.cpp    // Indicator light operation
    │   │   ├── component.mk
    │   │   └── include
    │   │       ├── lowpower_evb_adc.h
    │   │       ├── lowpower_evb_epaper.h
    │   │       ├── lowpower_evb_power.h
    │   │       ├── lowpower_evb_sensor.h
    │   │       └── lowpower_evb_status_led.h
    │   ├── lowpower_evb_ulp                    // ULP operation
    │   │   ├── lowpower_evb_ulp_opt.cpp
    │   │   ├── component.mk
    │   │   ├── include
    │   │   │   └── lowpower_evb_ulp_opt.h
    │   │   └── ulp                             // Implementation of ULP reading sensor data
    │   │       ├── bh1750.S                    // Light sensor BH1750 operation
    │   │       ├── hts221.S                    // Temperature & humidity Sensor HTS221 operation
    │   │       ├── i2c_dev.S                   // Program entry
    │   │       ├── i2c.S                       // I2C drive (software implementation)
    │   │       └── stack.S                     // Implementation of stack
    │   └── lowpower_evb_wifi                   // Wi-Fi configuration and connection
    │       ├── lowpower_evb_wifi.cpp
    │       ├── component.mk
    │       └── include
    │           └── lowpower_evb_wifi.h
    ├── main
    │   ├── component.mk
    │   ├── Kconfig.projbuild
    │   └── main.cpp
    ├── Makefile
    ├── readme_cn.md
    ├── README.md
    ├── sdkconfig.defaults
    └── sensor_server.py                        // The script to build a simple server

### 3.3 Demo Design Based on Low Power Software Framework

This demo is specially designed as a quick start guide to help you get familiar with Espressif Low Power Software Framework. The demo includes the implementations of the following functions:

* Initialization
* Wi-Fi configuration and connection
* Importing programs to ULP
* Obtaining sensor data
* Uploading data to server
* Sleep and wake up

Please note that the peripheral drivers in this demo, along with other Wi-Fi and network interfaces, are available in the folder `esp-iot-solution/components`. For your convenience, these components are ready to be called directly.

#### 3.3.1 Initialization

This part mainly focuses on the initialization of the peripherals to be used. For example, this demo performs the initialization of the sensors using I2C interface. It also initializes the E-ink using SPI interface, buttons, and LED indicators.

#### 3.3.2 Wi-Fi Configuration and Connection

The network configuration can be done with ESP-Touch, a technology developed by Espressif. In this demo, the network configuration information is stored in flash, so the device can use this information to automatically connect to the pre-configured network after rebooting. More specifically, the device always reads the network configuration information from the flash each time it reboots. If the device finds an existing configuration information, it directly connects to the previously configured access point (AP). Otherwise, the device enters `Smartconfig` and starts the network configuration process. Please see the details below:

<img src="../../documents/_static/lowpower_evb/wifi_config.png">   

In this demo, the Wi-Fi indicator light shows the status of network configuration: (a) blinks when Smartconfig is triggered, (b) stays on when the device is successfully connected to the network.

If you want to re-configure the network (for example, connect the device to another router), a WakeUp button has been designed for this demo to trigger the network configuration process. Pressing this button will erase the network configuration information stored in flash and reboot the device. After that, the device automatically enters the network configuration mode, since it cannot read any configuration information from flash. Please note that network configuration can only be triggered with this button while the device is not sleeping.

You can also design your own methods for triggering network configuration.

#### 3.3.3 Importing Programs to ULP

The ULP co-processor has its own specialized assembly instructions for programming. The binary files generated after compiling are imported to ULP via CPU. If ULP Wake-up is opted, the ULP co-processor runs the imported programs after the device enters deep sleep. It is recommended that you use the programs without making any changes.

#### 3.3.4 Obtaining Sensor Data

There are two ways for obtaining sensor data：

* From ULP -  After waking up, the CPU accesses the ULP memory and obtains the sensor data readings collected by the ULP co-processor during the device's deep sleep. This procedure greatly lowers power consumption as the data gets captured from ULP while the chip stays in deep sleep. In this demo, ULP will wake up the host CPU after reading the data for the number of times specified in `Read sensor value number`.
* From CPU - After waking up, the CPU reads the sensor data by using I2C interface. In this case, only the current values can be read after the device wakes up. This is more suitable for applications that require the status data occasionally. With this method, the power consumption of the device can be lowered to no more than 20-30 μA in total (the current of ESP32 module is only 5 uA), as the device stays in deep sleep with all the peripherals not running except for the power supply and ESP32 module. However, only the GPIO Wake-up and Timer Wake-up are supported here.

#### 3.3.5 Uploading Data to Server

The device can upload data to the server and allow it to save, check and analyze the uploaded data. The demo also allows the device to upload the data readings to a designated server.

In this demo, the device connection timeout is set to 10 seconds to reduce the host CPU run time. If the device's attempt to connect to the server times out, the device enters deep sleep and waits to be woken up again. The network indicator light displays the connection status: (a) blinks when the device is trying to connect to the server, (b) stays on when the device is successfully connected. Please note that before connecting to the server, the device must have a Wi-Fi connection.

The data received by the server will be printed as shown in the figure below:

<img src="../../documents/_static/lowpower_evb/sensor_server_data.png" width = "800">

To provide a reference design for E-ink, the last set of the uploaded data will be displayed in E-ink after the data transmission in this demo. You can modify the steps after the data transmission based on your specific requirements.

#### 3.3.6 Sleep and Wake Up

Sleep and Wake-up are usually the last programs to be performed as CPU will stop running when device enters deep sleep. At this point, the device waits to be woken up to start all over again, which is quite similar to the reboot mechanism. Now you need to set the Wake-up types (namely, Timer Wake-up, GPIO Wake-up and ULP Wake-up) before the device enters deep sleep, awaiting wake-up signals.

When the chip is in Deep-sleep mode, the ULP can be configured to go into sleep right after it has completed its routine. Then it waits to be waken up by a timer. The demo below shows the process of ULP running and waking up:

<img src="../../documents/_static/lowpower_evb/ulp_sleep.png">

## 4. Power Measurement of the Low Power Development Board

Pins for the power measurements of different parts of the board are all led out. It allows you to easily measure the power consumption of a specific part using an Ampere meter (jumper caps should be placed on the led-out pins when the board is in Operation mode). The detailed measurable parts include:

* 3.3V-3.3V_ESP32: module current, mainly from the Reset Protect chip and ESP32-WROOM-32
* 3.3V-PER_3.3V: peripheral current, mainly from two sensors and E-ink
* DCVCC-SUFVCC: board operating current, including the currents from the module, peripherals and other peripheral circuits
* CHVBA-VBA: battery charging current
* EXT5V-CH5V: the current at the USB port during the battery charging

The table below lists the current measurement results for different parts：

| Part | Condition | Current | Description | Note |
|------|-----------|---------|-------------|------|
|3.3V-3.3V_ESP32 | Deep sleep (GPIO Wake-up enabled) | 10 uA  | The measurement is performed when the device enters deep sleep and waits to be woken up by GPIO (no other actions). | 5 uA (Reset Protect Chip) + 5 uA (ESP32-WROOM-32) |
|3.3V-3.3V_ESP32 | Deep sleep (ULP Wake-up enabled)  | 1.8 mA | The measurement is performed when the device enters deep sleep while the ULP stays awake. | / |
|3.3V-PER_3.3V   | Two sensors are running           | 1.6 mA | Providing power supply to sensors by controlling IO27. | / |
|3.3V-PER_3.3V   | Two sensors and E-ink are running | 9.2 mA | Providing power supply to sensors by controlling IO27 and provide power supply to E-ink by controlling IO14. | / |
|DCVCC-SUFVCC    | Deep sleep (GPIO Wake-up enabled) | 20 uA  | The measurement is performed when the device enters deep sleep and waits to be woken up by GPIO (no other actions). | 5 uA (Reset Protect chip) + 5 uA (ESP32-WROOM-32) + DC-DC conversion (10 uA) |
|DCVCC-SUFVCC    | Deep sleep (ULP Wake-up enabled)  | 3.4 mA | The measurement is performed when the device enters deep sleep while the ULP stays awake and reads the sensor data. | / |
|CHVBA-VBA       | Both the battery and USB port are connected| The power consumption changes according to the battery power | Please measure directly. | / |
|EXT5V-CH5V      | Both the battery and USB port are connected| The power consumption changes according to the battery power | Please measure directly. | / |

The power consumption of the module is 1.8 mA when the ULP stays awake. However, the power consumption could be further lowered with the timed sleep function. In this demo, please set the time interval of ULP reading sensor data using the `Read sensor interval(ms)` option in menuconfig. Note that, the longer the interval, the lower the average power consumption and the longer the battery life. Correspondingly, the data readings by the ULP are also less frequent. You may determine a reasonable time interval according to your specific requirements. The figure below demonstrates the module's current waveform when the ULP performs data readings. Also you may find that the data reading interval is 200 ms, the average current during ULP sensor reading is 1.8 mA, and the average current in Sleep mode after ULP sensor reading is 5 uA.

<img src="../../documents/_static/lowpower_evb/ulp_read_sensor_current.png" width = "600">
