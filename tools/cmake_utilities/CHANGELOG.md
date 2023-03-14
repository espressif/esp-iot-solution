## v0.4.0 - 2023-03-13

### Feature:

* Add command `idf.py gen_single_bin` to generate merged bin file
* Add command `idf.py flash_single_bin` to flash generated merged bin
* Add config `Color in diagnostics` to control the GCC color output

## v0.3.0 - 2023-03-10

* Add gen_compressed_ota functionality, please refer to [gen_compressed_ota.md](https://github.com/espressif/esp-iot-solution/tree/master/tools/cmake_utilities/docs/gen_compressed_ota.md)

## v0.2.1 - 2023-03-09

### Bugfix:

* package manager: Fix the compile issue when the name of the component has `-`, just like esp-xxx

## v0.2.0 - 2023-02-23

* Add relinker functionality, please refer to [relinker.md](https://github.com/espressif/esp-iot-solution/tree/master/tools/cmake_utilities/docs/relinker.md)

## v0.1.0 - 2023-01-12

### Feature:

* Add function cu_pkg_get_version
* Add macro cu_pkg_define_version
* Add cmake script to CMAKE_MODULE_PATH
