# Relinker

In ESP-IDF, some functions are put in SRAM when link stage, the reason is that some functions are critical, we need to put them in SRAM to speed up the program, or the functions will be executed when the cache is disabled. But actually, some functions can be put into Flash, here, we provide a script to let the user set the functions which are located in SRAM by default to put them into Flash, in order to save more SRAM which can be used as heap region later. This happens in the linker stage, so we call it as relinker.

Some SoCs(ESP32-C2 and ESP32-C3) support hardware auto flash-suspend, meaning when reading, writing, or erasing the flash stage, the CPU hardware can switch to run code laying in the flash automatically without any software assist, so that we can link more functions from IRAM to flash.

## Use

In order to use this feature, you need to include the needed CMake file in your project's CMakeLists.txt after `project(XXXX)`.

```cmake
project(XXXX)

include(relinker)
```

The relinker feature is disabled by default, in order to use it, you need to enable the option `CU_RELINKER_ENABLE` in menuconfig.

Here are the default configuration files in the folder `cmake_utilities/scripts/relinker/examples/flash_suspend/XXX (XXX=esp32c2, esp32c3...)` for specific SoC platform when flash_suspend and relinker feature are enabled, please note that the configuration files may be slightly different for different ESP-IDF versions, it's just used as a reference. If you would like to use your own configuration files, please enable option `CU_RELINKER_ENABLE_CUSTOMIZED_CONFIGURATION_FILES` and set the path of your configuration files as following, this path is evaluated relative to the project root directory:

```
[*]     Enable customized relinker configuration files
(path of your configuration files) Customized relinker configuration files path
```

If you would like to save more RAM, you can try flash-suspend feature, enable options as following:

```
Component config  --->
    SPI Flash driver  --->
        [ ] Use esp_flash implementation in ROM
        [*] Auto suspend long erase/write operations (READ DOCS FIRST)

CMake Utilities  --->
    [*] Enable relinker
    [*]     Link specific functions to IRAM regardless of ESP-IDF's behavior
```

> Note: Currently supported platform: esp32c2 and esp32c3.
> Note: Before enable flash-suspend, please check if ESP-IDF supports this flash's flash-suspend feature.

## Configuration Files

You can refer to the files in the directory of `cmake_utilities/scripts/relinker/examples/$ACTION/XXX ($ACTION=iram_strip, flash_suspend) (XXX=esp32c2, esp32c3...)`:

- library.csv
- object.csv
- function.csv

1. iram_strip

    For `example/iram_strip`, it includes the functions which you want to relink into Flash after enable the option `CU_RELINKER_ENABLE` and disable the option `CU_RELINKER_LINK_SPECIFIC_FUNCTIONS_TO_IRAM` in menuconfig. If you want to link function `__getreent` from SRAM to Flash, firstly you should add it to `function.csv` file as following:

    ```
    libfreertos.a,tasks.c.obj,__getreent,
    ```

2. flash_suspend

    For `example/flash_suspend`, it includes the functions which you want to relink into SRAM after enable the options: `SPI_FLASH_AUTO_SUSPEND`, `CU_RELINKER_ENABLE`, `CU_RELINKER_LINK_SPECIFIC_FUNCTIONS_TO_IRAM` in menuconfig. If you want to link function `__getreent` into SRAM no matter its original locations, firstly you should add it to `function.csv` file as following:

    ```
    libfreertos.a,tasks.c.obj,__getreent,
    ```

This means function `__getreent` is in object file `tasks.c.obj`, and object file `tasks.c.obj` is in library `libfreertos.a`.

If function `__getreent` depends on the option `FREERTOS_PLACE_FUNCTIONS_INTO_FLASH` in menuconfig, then it should be:

```
libfreertos.a,tasks.c.obj,__getreent,CONFIG_FREERTOS_PLACE_FUNCTIONS_INTO_FLASH
```

This means when only `FREERTOS_PLACE_FUNCTIONS_INTO_FLASH` is enabled in menuconfig, function `__getreent` will be linked from SRAM to Flash.

Next step you should add the path of the object file to `object.csv`:

```
libfreertos.a,tasks.c.obj,esp-idf/freertos/CMakeFiles/__idf_freertos.dir/FreeRTOS-Kernel/tasks.c.obj
```

This means the object file `tasks.c.obj` is in library `libfreertos.a` and its location is `esp-idf/freertos/CMakeFiles/__idf_freertos.dir/FreeRTOS-Kernel/tasks.c.obj` relative to directory of `build`.

Next step you should add path of library to `library.csv`:

```
libfreertos.a,./esp-idf/freertos/libfreertos.a
```

This means library `libfreertos.a`'s location is `./esp-idf/freertos/libfreertos.a` relative to `build`.

If above related data has exists in corresponding files, please don't add this repeatedly.

## Test Results Reference

| Chip     | example         |                   | SRAM used (Bytes) |                                    |
| -------- | --------------- | ----------------- | ----------------- | ---------------------------------  |
|          |                 | Default options   | Enable relinker   | Enable relinker and flash-suspend  |
| ESP32-C2 | power_save      | 101408            | 91728             | 51360                              |
| ESP32-C3 | power_save      | 118728            | 99864             | 58312                              |

All of the above examples are compiled on ESP-IDF Tag/v5.3.2 and version v1.1.0 of `cmake_utilities` component, you can view the memory type usage summary by using `idf.py size`, the above data is for reference only.