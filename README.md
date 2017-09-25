# Project Description of IoT Solution

---
## <h2 id="preparation">Preparation</h2>

* To clone this repository by 

    ```
    git clone --recurisve https://github.com/espressif/esp-iot-solution
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
├── documents
├── examples
│   └── smart_device
├── platforms
├── products
├── sdkconfig
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
	    cp sdkconfig.default sdkconfig
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

* To run the Examples project, follow the steps below:
    * Change the directory to example 
    * choose one example project you want to run, we take smart_device here.
    * Change the directory to smart_device under example
        
        ```
        cd YOUR_IOT_SOLUTION_PATH/example/smart_device
        ```
       
    * Run `make menuconfig` to set the project parameters in 
    
        ```
        make menuconfig --> component config --> IoT Example - smart_device
        ```
    
    * Run `make` to compile the project, or `make flash` to compile and flash the module.
* Follow the instructions in `wiki/example_smart_device.md`

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


