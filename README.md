# Project Description of IoT Solution

---
## Preparation
* To clone this repository by **git clone --recurisve https://github.com/espressif/esp-iot-solution.git**
* Set IOT_SOLUTION_PATH, for example:

    ```
    export IOT_SOLUTION_PATH=~/esp/esp-iot-solution
    ```
* esp-iot-solution project will overwrite the IDF_PATH to the submodule in makefile by default
* You can refer to the Makefile in example if you want to build your own application.

---

## File structure
* `components`
    * small drivers of different divices like button and LED
    * drivers of sensors
    * drivers of different I2C devices
    * friendly APIs of WiFi and OTA
* `Documents`:
    * Documentations of some important features
    * Instruction of some different solutions
* `Examples`:
    * Example project using this framework
* `Platforms`:
    * Platforms
    * Cloud services 
* `Products`:
    * Product that combined by different components
    * For example, a Switch product consists of button, relay, etc.
* `Submodule`:
    * esp-idf works as submodule here
* `tools`:
    * different tools and scripts
    * unit-test project
    

```
├── Makefile
├── README.md
├── components
│   ├── Kconfig
│   ├── base_class
│   ├── bh1750
│   ├── button
│   ├── component.mk
│   ├── dac_audio
│   ├── debugs
│   ├── deep_sleep
│   ├── hts221
│   ├── http_request
│   ├── i2c_bus
│   ├── i2c_debug
│   ├── is31fl3xxx
│   ├── lcd
│   ├── led
│   ├── light
│   ├── lis2dh12
│   ├── ota
│   ├── param
│   ├── power_meter
│   ├── relay
│   ├── smart_config
│   ├── touchpad
│   ├── ulp_monitor
│   ├── weekly_timer
│   └── wifi
├── documents
│   ├── deep_sleep
│   ├── esp32_lowpower.md
│   ├── esp32_lowpower_EN.md
│   ├── esp32_secure_and_encrypt.md
│   ├── esp32_secure_encrypt
│   ├── esp32_touchpad.md
│   ├── esp32_touchpad_EN.md
│   └── touchpad
├── examples
│   ├── README.md
│   └── smart_device
├── platforms
│   ├── alink_v2.0
│   ├── cloud_objs
│   ├── joylink
├── products
│   ├── light
│   ├── light_device
│   ├── sensor
│   ├── socket
│   └── switch
├── readme.txt
├── sdkconfig
├── submodule
│   └── esp-idf
└── tools
    ├── default_config
    └── unit-test-app
```



---

## Usage
* This code is based on esp-idf project.
* This repository contains esp-idf code as submodule.
* To clone this repository by **git clone --recurisve https://github.com/espressif/esp-iot-solution.git**
* Change to the directory of examples (such as examples/smart_device) in esp-iot-solution, run `make menuconfig` to configure the project.
* Compiling the Project by `make all`
> ... will compile app, bootloader and generate a partition table based on the config.
*  Flashing the Project
* Flash the binaries by `make flash`
> This will flash the entire project (app, bootloader and partition table) to a new chip. The settings for serial port flashing can be configured with `make menuconfig`.
> You don't need to run `make all` before running `make flash`, `make flash` will automatically rebuild anything which needs it.
* Viewing Serial Output by `make monitor` target will use the already-installed [miniterm](http://pyserial.readthedocs.io/en/latest/tools.html#module-serial.tools.miniterm) (a part of pyserial) to display serial output from the ESP32 on the terminal console.
Exit miniterm by typing Ctrl-].
* To flash and monitor output in one pass, you can run: `make flash monitor`
* You can use unit-test in esp-iot-solution to test all the components.

--- 

## Unit-test

* To use uint-test, follow these steps:
	* Change to the directory of unit-test-app
	* Compile unit-test-app by `make IOT_TEST_ALL=1 -j8`
	* Flash the images by `make flash`
	* Reset the chip and see the uart log using an uart tool such as minicom
	* All kinds of components will be shown by uart and you need to send the index of the unit you want to test by uart
	* Test code of the unit you select will be run


---


## Encapsulated components

* button: Provide some useful functions of physical buttons, such as short press, long press, release and so on.
* LED: Provide different states including bright, dark, quick blink and slow blink of led. You could use this module to indicate different states of the program.
* light: Provide functions of a smart light. You could add different pwm channels to the light and set the channels to different states such as custom duty cycle, custom blink frequecy, breathing light effect and so on.
* OTA: Provide Over The Air Updates function.
* param: Provide data read and write functions, based on NVS.
* power_meter: Provide power, voltage and current measurement. The driver is based on BL0937 chip.
* relay: Provide two methods to control relays. One method uses a delay flip-flop and another uses RTC IO to control relay directly.
* smart_config: Provide a blocking function to perform smart config. User could set the time out of smart config.
* touchpad: Provide some useful functions of touchpad, such as single trigger, serial trigger, short press, long press and so on.
* ulp_monitor: Provide simple apis to use ULP coprocessor to collect and save data of temprature sensor and adc while deep sleep.
* wifi: Provide a blocking function to make esp32 connect to ap. User could set the time out of connecting.

---

## Solution list:

* Low power / deep sleep / co-processor solution
* Touch sensor solution and application note
* Security solution and factory flow
* AWS + Amazon Alexa custom skills service solution