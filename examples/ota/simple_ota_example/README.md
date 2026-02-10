| Supported Targets | ESP32-C2 | ESP32-C3 | ESP32-C5 | ESP32-C6 | ESP32-C61 | ESP32 | ESP32-S3 | ESP32-H2 |
| ----------------- | ----- | -------- | -------- | -------- | -------- | -------- |-------- | -------- |   

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

## Example Output

```
I (4601) simple_ota_example: Starting OTA example task
I (4611) simple_ota_example: Attempting to download update from https://192.168.4.102:8070/simple_ota.bin.xz.packed
I (4621) main_task: Returned from app_main()
I (5071) esp_https_ota: Starting OTA...
I (5071) custom_image: OTA to ota_1
I (5071) esp_https_ota: Writing to partition subtype 17 at offset 0x150000
I (13131) simple_ota_example: OTA Succeed, Rebooting...
...
I (115) boot_custom: bootloader plus version: 0.2.6
I (252) boot_custom: start OTA to slot 0
I (10268) boot_custom: OTA success, boot from slot 0
I (10268) esp_image: segment 0: paddr=00020020 vaddr=3c0c0020 size=2a8cch (174284) map
I (10299) esp_image: segment 1: paddr=0004a8f4 vaddr=3fc92e00 size=02d88h ( 11656) load
I (10301) esp_image: segment 2: paddr=0004d684 vaddr=40380000 size=02994h ( 10644) load
I (10307) esp_image: segment 3: paddr=00050020 vaddr=42000020 size=b1408h (726024) map
I (10427) esp_image: segment 4: paddr=00101430 vaddr=40382994 size=102b0h ( 66224) load
I (10445) boot: Loaded app from partition at offset 0x20000
```