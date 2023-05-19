## Description

[![Component Registry](https://components.espressif.com/components/espressif/gprof/badge.svg)](https://components.espressif.com/components/espressif/gprof)

The Espressif GNU Profiler is used to evaluate the time and proportion consumed by various parts of the running program, enabling developers to optimize the software based on this information.

This software supports the main functionality of the GNU Profiler. Additionally, it also supports the following features:

Gathering information about calling OS APIs, such as sleep and read, these are specifically for Xtensa CPU.
Application to user-specified components in ESP-IDF, without the need to apply it to all components.
Storing information in a partition with a type of data and a subtype of 0x3a.

However, this software also has the following usage limitations:

It cannot be applied to kernel components like FreeRTOS, libc, hardware drivers, and so on.
It cannot gather information on functions that reside in IRAM.
It does not support multi-thread mode; it can only gather information from the thread that calls esp_gprof_init.

### Usage

Add a dependency on this component in your component or project's idf_component.yml file.

    ```yml
    dependencies:
      espressif/gprof: "0.*"
    ```

To use this feature, including the required CMake file in your project's CMakeLists.txt file after the line project(XXXX).

```cmake
project(XXXX)

include(gprof)
```

Enable GNU Profiler in the menuconfig and select target components in ESP-IDF as follows:

```
Component config  --->
    Espressif GNU Profiler(GProf)  --->
        [*] Enable Espressif GNU Profiler(GProf)
        (component_a;component_b) Select components for GProf
```

Each component must be separated by a ";" and cannot contain irrelevant symbols such as spaces.

Use your partition table that includes the following partition (type is data, and subtype is 0x3a):

```
# Name,   Type, SubType, Offset,  Size, Flags
gprof,    data, 0x3a,           , 0x1000,
```

You can increase the size if your selected components are large or numerous.

5. Add API calls in your project as follows:

```c
#include "gprof.h"

    ESP_ERROR_CHECK(esp_gprof_init());
    
    // Your Code

    ESP_ERROR_CHECK(esp_gprof_save());
    esp_gprof_deinit();
```

6. Build and run your project, and wait until the terminal shows the following information:

```
esp_gprof: GProf data is saved successfully
```

7. Obtain information from GNU Profiler using the following command:

```sh
idf.py gprof
```

If you want to generate a GNU Profiler picture in PNG format(Only support Ubuntu and MacOS), you need to install the required tools first using the following commands:

- Ubuntu:

```sh
sudo apt install graphviz
pip3 install gprof2dot
```

- MacOS:

```sh
brew install graphviz
pip3 install gprof2dot
```

Then, use the following command to generate the picture of your project. The picture will be located at build/{PROJECT_NAME}.gprof.png

```sh
idf.py gprof-graphic
```

### Adding the Component to Your Project

Please use the component manager command add-dependency to add the gprof component as a dependency in your project. During the CMake step, the component will be downloaded automatically.

```
idf.py add-dependency "espressif/gprof=*"
```

### Examples

Please use the component manager command create-project-from-example to create a project from the example template.

```
idf.py create-project-from-example "espressif/gprof=*:gprof_simple"
```

This command will download the example gprof_simple into the current folder. You can navigate into it to build and flash the example.

Alternatively, you can download examples from the esp-iot-solution repository:
1. [gprof_simple](https://github.com/espressif/esp-iot-solution/tree/master/examples/gprof/gprof_simple)
