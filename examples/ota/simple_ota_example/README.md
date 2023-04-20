| Supported Targets | ESP32-C2 | ESP32-C3 |
| ----------------- | ----- | -------- | 

# Simple OTA example
## Overview

This example demonstrates how to use compressed OTA by the [bootloader support plus](https://components.espressif.com/components/espressif/bootloader_support_plus) component. 

The basic flow of compressed OTA:

```
    ************                        *************
    *bootloader* ---------------------->*   ota_0   *  <-----
    ************                        *************       |
                                                            |decompress
                                        *************       |
                                        *   ota_1   *  ------
                                        *************
```

1. Compress the new firmware. 
2. The device to be updated downloads the compressed firmware and restarts.  
3. After restarting, in the bootloader stage, decompress the compressed firmware and write the decompressed data to the specified partition.  
4. Boot the new firmware.  

## How to use the example

The example is a copy of [simple_ota_example](https://github.com/espressif/esp-idf/tree/master/examples/system/ota/simple_ota_example). Please refer the [README.md](https://github.com/espressif/esp-idf/tree/master/examples/system/ota) for setup details.  

You can also use the other samples in [esp-idf/examples/system/ota](https://github.com/espressif/esp-idf/tree/master/examples/system/ota) directory to verify compressed OTA.  

## How to generate compressed firmware

Generate the compressed app firmware in an ESP-IDF "project" directory by running:

```plaintext
idf.py gen_compressed_ota
```

This command will compile the project first, then it will generate the compressed app firmware. If there are no errors, the `build/custom_ota_binaries` directory may contain the following file:

```plaintext
simple_ota.bin.xz  
simple_ota.bin.xz.packed
```

The file named `simple_ota.bin.xz.packed` is the actual compressed app binary file to be transferred.

In addition, if [secure boot](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/security/secure-boot-v2.html) is enabled, the command will generate the signed compressed app binary file:

```plaintext
simple_ota.bin.xz.packed.signed
```

you can also use the script [gen_custom_ota.py](https://github.com/espressif/esp-iot-solution/blob/master/tools/cmake_utilities/scripts/gen_custom_ota.py) to compress the specified app:

```plaintext
python3 gen_custom_ota.py -hv v3 -i simple_ota.bin --add_app_header
```
