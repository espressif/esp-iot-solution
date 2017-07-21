`Project Description of IoT Solution`

Read < ==Wiki== >  to find how to run IoT examples

## IDEAS
* We want to make this repository be a code version 'Lego blocks', so the users can easily pick the blocks/components that they need in their projects.
* We make each single feature/function a small block/components, all the blocks are based on esp-idf.
* We integrate the most popular IoT server platforms like `alink`, `joylink`, `AWS`, `esp-server` etc...
* With those **blocks**, we can build all kinds of IoT applications.
* We will provide some examples of several kinds of typical IoT products with detailed features, like smart socket/light/sensor.
* If you can not find the exact block for your project here, please raise your requirements and give us the function description of the block, we will make it. 




### Usage
* This code is based on esp-idf project.
* This repository doesn't contain esp-idf code, you should first clone the esp-idf code by **git clone --recurisve  https://github.com/espressif/esp-idf.git**
* To clone the this repository by **git clone https://github.com/espressif/esp-iot-solution.git**
* Change to the directory of this project, run `make menuconfig` to configure the project.
* Compiling the Project by `make all`
> ... will compile app, bootloader and generate a partition table based on the config.
*  Flashing the Project
* Flash the binaries by `make flash`
> This will flash the entire project (app, bootloader and partition table) to a new chip. The settings for serial port flashing can be configured with `make menuconfig`.
> You don't need to run `make all` before running `make flash`, `make flash` will automatically rebuild anything which needs it.
* Viewing Serial Output by `make monitor` target will use the already-installed [miniterm](http://pyserial.readthedocs.io/en/latest/tools.html#module-serial.tools.miniterm) (a part of pyserial) to display serial output from the ESP32 on the terminal console.
Exit miniterm by typing Ctrl-].
* To flash and monitor output in one pass, you can run: `make flash monitor`


### WHY?
* `IoT solution` needs be reliable at scale. Today’s market requirements demand that a platform can both scale up to support millions of devices with different usage and technology characteristics as well as scale down to support limited-deployment, single application pilot projects. A qualified IoT Solution has architected itself allow organizations to deploy devices cost-effectively and quickly with some integration and modification of submodules. 

* At the same time, as platforms allow varying scales of deployments, the reliability of smallest modules and platforms must remain high. Use of redundant and fault-tolerant architecture is a must to eliminate outages and data loss for all but the most extraordinary of circumstances. Building such IoT-ready infrastructure is necessary as it provides a reliable and cost-effective platform upon which various projects can grow.

* Enterprises will make modifications to existing solutions over time and create completely new IoT solutions. And it’s necessary to create a clear and logical separation between the different layers of the IoT technology stack. This enables businesses to adapt to changing business needs and reduces the engineering costs of future IoT deployments and incremental solution changes.

### HOW?


* For `Espressif IoT Solution`, we would build from the three perspectives, modules, platforms and products.

* Within `modules`, we have ESP-Mesh, Light, OTA, Relay, Security boot, Wi-Fi handler, OLED, LED,  Power Meter, LCD GUI, Smart Configure, Touch Pad, BLE pairing, button and etc.

* Within `platforms`, we will build, including but not limited to, AWS, Amazon alexa echo, `Joylink`, We-chat connection and among others.

* Within `products`, we will develop ESP `socket` products, smoker detector, LED light, temperature controller, button products and so on.

* We aim to develop IoT solution from the very basis, i.e. from the smallest modules, so that we could combine different modules with distinct platforms, and get various products.


### Well encapsulated modules

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
* 

# Solution list:

* Low power / deep sleep / co-processor solution
* Touch sensor solution and application note
* Security solution and factory flow
* AWS + Amazon Alexa custom skills service solution