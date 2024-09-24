# Cmake utilities

[![Component Registry](https://components.espressif.com/components/espressif/cmake_utilities/badge.svg)](https://components.espressif.com/components/espressif/cmake_utilities)

This component is aiming to provide some useful CMake utilities outside of ESP-IDF.

**Supported features:**

- `project_include.cmake`: add additional features like `DIAGNOSTICS_COLOR` to the project. The file will be automatically parsed, for details, please refer [project-include-cmake](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-guides/build-system.html#project-include-cmake>).
- `package_manager.cmake`: provides functions to manager components' versions, etc.
- `gcc.cmake`: manager the GCC compiler options like `LTO` through menuconfig.
- `relinker.cmake` provides a way to move IRAM functions to flash to save RAM space.
- `gen_compressed_ota.cmake`: add new command `idf.py gen_compressed_ota` to generate `xz` compressed OTA binary. please refer [xz](https://github.com/espressif/esp-iot-solution/tree/master/components/utilities/xz).
- `gen_single_bin.cmake`: add new command `idf.py gen_single_bin` to generate single combined bin file (combine app, bootloader, partition table, etc).

## User Guide

[cmake_utilities user guide](https://docs.espressif.com/projects/esp-iot-solution/zh_CN/latest/basic/cmake_utilities.html)
