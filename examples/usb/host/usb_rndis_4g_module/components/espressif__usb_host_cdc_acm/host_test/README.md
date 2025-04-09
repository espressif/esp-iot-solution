| Supported Targets | Linux |
| ----------------- | ----- |

# Description

This directory contains test code for `USB Host CDC-ACM` driver. Namely:
* Descriptor parsing
* Simple public API call with mocked USB component to test Linux build and Cmock run for this class driver

Tests are written using [Catch2](https://github.com/catchorg/Catch2) test framework, use CMock, so you must install Ruby on your machine to run them.

# Build

Tests build regularly like an idf project. Currently only working on Linux machines. 

```
idf.py --preview set-target linux
idf.py build
```

# Run

The build produces an executable in the build folder. 

Just run:

```
./build/host_test_usb_cdc.elf
```

The test executable have some options provided by the test framework. 
