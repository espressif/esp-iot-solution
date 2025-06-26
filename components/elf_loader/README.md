## Description

[![Component Registry](https://components.espressif.com/components/espressif/elf_loader/badge.svg)](https://components.espressif.com/components/espressif/elf_loader)

Espressif ELF(Executable and Linkable Format) loader is a software development kit that is developed based on the ESP-IDF, mainly used to load ELF file compiled based on ESP32 series SoCs to the executable memory area, then link and execute it.

In this way, the application does not need compile into the whole firmware in advance. It is like running the compiled program through terminal input `./main.o` on the Ubuntu platform automatically, which realizes the separation of application and kernel.

This ELF loader supports following SoCs:

- ESP32
- ESP32-S2, support running ELF in PSRAM
- ESP32-S3, support running ELF in PSRAM
- ESP32-P4, support running ELF in PSRAM
- ESP32-C6
- ESP32-C61, support running ELF in PSRAM

### Usage

#### Firmware

Add a dependency on this component in your component or project's idf_component.yml file.

    ```yml
    dependencies:
      espressif/elf_loader: "1.*"
    ```

Enable ELF loader in the menuconfig:

```
Component config  --->
    ESP-ELFLoader Configuration  --->
        [*] Enable Espressif ELF Loader
```

Add API calls in your project as follows:

```c
#include "esp_elf.h"

    esp_elf_t elf;

    // Your Code

    esp_elf_init(&elf);
    esp_elf_relocate(&elf, elf_file_data_bytes);
    esp_elf_request(&elf, 0, argc, argv);
    esp_elf_deinit(&elf);
```

#### ELF APP

To use this feature to compile ELF file, including the required CMake file in your project's CMakeLists.txt file after the line project(XXXX).

```cmake
project(XXXX)

# Add
include(elf_loader)
project_elf(XXXX)
```

Build the project as an ordinary ESP-IDF project, and then the ELF file named `XXXX.app.elf` is in the build directory.

### ELF APP Fast Build

Users can enable ELF fast build functionality by configuring CMAKE's generator as Unit Makefile. The reference command is as follows:

```bash
idf.py -G 'Unix Makefiles' set-target <chip-name>
```

Then input the ELF APP build command as follows:

```
idf.py elf
```

The build system will only build ELF target components and show the following logs:

```
Building C object esp-idf/main/CMakeFiles/__idf_main.dir/main.c.obj
Linking C static library libmain.a
Build ELF: hello_world.app.elf
Built target elf
```

### Adding the Component to Your Project

Please use the component manager command add-dependency to add the elf_loader component as a dependency in your project. During the CMake step, the component will be downloaded automatically.

```
idf.py add-dependency "espressif/elf_loader=*"
```

### Examples

Please use the component manager command create-project-from-example to create a project from the example template.

```
idf.py create-project-from-example "espressif/elf_loader=*:elf_loader_example"
```

This command will download the example elf_loader_example into the current folder. You can navigate into it to build and flash the example.

Alternatively, you can download examples from the esp-iot-solution repository:
1. [build_elf_file_example](https://github.com/espressif/esp-iot-solution/tree/master/examples/elf_loader/build_elf_file_example)
2. [elf_loader_example](https://github.com/espressif/esp-iot-solution/tree/master/examples/elf_loader/elf_loader_example)
