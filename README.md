# Project Description of IoT Solution

---
## <h2 id="preparation">Preparation</h2>

* To clone this repository by 

    ```
    git clone --recursive https://github.com/espressif/esp-iot-solution
    ```

* To initialize the submodules

    ```
    git submodule update --init --recursive
    ```
    
* Set IOT_SOLUTION_PATH, for example:

    ```
    export IOT_SOLUTION_PATH=~/esp/esp-iot-solution
    ```
* esp-iot-solution project will over `write` the `IDF_PATH` to the submodule in makefile by default
* You can refer to the Makefile in example if you want to build your own application.
* To run the example project, please refer to:

    * [example](#example)
    * [unit-test](#unit-test)

---

## Build system and dependency

* We can regard IoT solution project as a platform that contains different device drivers and features
* `Add-on project`: If you want to use those drivers and build your project base on the framework, you need to include the IoT components into your project.

	```
PROJECT_NAME := empty_project
#If IOT_SOLUTION_PATH is not defined, use relative path as default value
IOT_SOLUTION_PATH ?= $(abspath $(shell pwd)/../../)
include $(IOT_SOLUTION_PATH)/Makefile
include $(IDF_PATH)/make/project.mk
```
  As we can see, in the Makefile of your project, you need to include the Makefile under $(IOT_SOLUTION_PATH) directory so that the build system can involve all the components and drivers you need.

  `Note: In this way, the build system will replace the IDF_PATH with the directory of idf repository in submodule during the build process.`
 
  If you don't want the replacement to happen(which means to use the ESP_IDF value from your system environment), you can modify as the following Makefile does:
  
  	```
PROJECT_NAME := empty_project
#If IOT_SOLUTION_PATH is not defined, use relative path as default value
IOT_SOLUTION_PATH ?= $(abspath $(shell pwd)/../../)
include $(IOT_SOLUTION_PATH)/components/component_conf.mk
include $(IDF_PATH)/make/project.mk
```
* `Stand-alone component`: if you just want to pick one of the component and put it into your existing project, you just need to copy the single component to your project components directory. But you can also append the component list in your project Makefile like this:

   ```
   EXTRA_COMPONENT_DIRS += $(IOT_SOLUTION_PATH)/components/the_one_you_want_to_use
   ```
* `Components control`: Usually we don't need all the device drivers to be compiled into our project, we can choose the necessary ones in menuconfig:

    ```
    make menuconfig --> IoT Solution settings --> IoT Components Management 
    ```
    Those components that not enabled, will not be compiled into the project, which alos means, you need to enable the components you would like to use.

---

## Framework structure
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
* `Submodule`:
    * esp-idf works as submodule here
* `tools`:
    * different tools and scripts
    * unit-test project
    

```
├── Makefile
├── README.md
├── components
├── documents
├── examples
│   └── check_pedestrian_flow
│   └── empty_project
│   └── eth2wifi
│   └── oled_screen_module
│   └── smart_device
│   └── touch_pad_evb
│   └── ulp_rtc_gpio
│   └── ulp_watering_device
├── submodule
│   └── esp-idf
└── tools
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

## <h2 id="unit-test">Unit-test</h2>
[back to top](#preparation)

* To use uint-test, follow these steps:
	* Change to the directory of unit-test-app
	
	    ```
	    cd YOUR_IOT_SOLUTION_PATH/tools/unit-test-app
	    ```
	
	* Use the default sdkconfig and compile unit-test-app by `make IOT_TEST_ALL=1 -j8`
	
	    ```
	    make defconfig
	    make IOT_TEST_ALL=1
	    ```
	
	* Flash the images by `make flash`
	
	    ```
	    make IOT_TEST_ALL=1 flash
	    ```
	
	* Reset the chip and see the uart log using an uart tool such as minicom
	* All kinds of components will be shown by uart
	
	    ```
	    Here's the test menu, pick your combo:
            (1)     "Sensor BH1750 test" [bh1750][iot][sensor]
            (2)     "Button test" [button][iot]
            (3)     "Button cpp test" [button_cpp][iot]
            (4)     "Dac audio test" [dac_audio][iot][audio]
            (5)     "Debug cmd test" [debug][iot]
            (6)     "Deep sleep gpio test" [deep_sleep][rtc_gpio][current][iot]
            (7)     "Deep sleep touch test" [deep_sleep][touch][current][iot]
            ......
	    ```
	
	* You need to send the index of the unit you want to test by uart. Test code of the unit you select will be run


---

## <h2 id="example">Example</h2>
[back to top](#preparation)

* To run the Examples projects, follow the steps below:
    * Change the directory to example 
    * choose one example project you want to run, we take smart_device here.
    * Change the directory to the example project under example directory, take smart_device example as example here:
        
        ```
        cd YOUR_IOT_SOLUTION_PATH/example/smart_device
        ```
       
    * Run `make menuconfig` to set the project parameters in 
    
        ```
        make menuconfig --> IoT Example - smart_device
        ```
    
    * Run `make` to compile the project, or `make flash` to compile and flash the module.

---

## <h2 id="empty_project">Empty project</h2>
[back to top](#preparation)

* You can start your own appliction code base on the empty-project.
* By default, you just need to run `make` under the example/smart_device directory. The makefile will set all the path by default.
* Meanwhile, you can copy the example project to anywhere you want, in this case, you will have to set the `IOT_SOLUTION_PATH` so that the build system shall know where to find the code and link the files.
* Set iot path:(replace 'YOUR_PATH' below), you can also add `IOT_SOLUTION_PATH ` to your environment PATH 
        
    ```
        export IOT_SOLUTION_PATH = "YOUR_PATH/esp-iot-solution"
    ```

---



## Solution list:

* [Low power / deep sleep / co-processor solution](documents/low_power_solution)
* [Touch sensor solution and application note](documents/touch_pad_solution)
* [Security solution and factory flow](documents/security_solution)

## Example list:
* [check_pedestrian_flow](examples/check_pedestrian_flow)
* [empty_project](examples/empty_project)
* [eth2wifi](examples/eth2wifi)
* [oled_screen_module](examples/oled_screen_module)
* [smart_device](examples/smart_device)
* [touch_pad_evb](examples/touch_pad_evb)
* [ulp_rtc_gpio](examples/ulp_rtc_gpio)
* [ulp_watering_device](examples/ulp_watering_device)



