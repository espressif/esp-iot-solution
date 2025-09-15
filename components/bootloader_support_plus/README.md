# bootloader support plus

[![Component Registry](https://components.espressif.com/components/espressif/bootloader_support_plus/badge.svg)](https://components.espressif.com/components/espressif/bootloader_support_plus)

* [中文版](https://github.com/espressif/esp-iot-solution/blob/master/components/bootloader_support_plus/README_CN.md)

## Overview
The `bootloader support plus` is an enhanced bootloader based on [custom_bootloader](https://github.com/espressif/esp-idf/tree/master/examples/custom_bootloader) . The firmware update function is supported in the bootloader stage by decompressing the compressed firmware. In this solution, you can directly use the original OTA APIs (such as [esp_ota](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/ota.html#api-reference)、[esp_https_ota](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/esp_https_ota.html#api-reference)). The following table shows the Espressif SoCs that are compatible with `bootloader support plus` and their corresponding ESP-IDF versions.

| Chip     | ESP-IDF Release/v5.0                                         | ESP-IDF Release/v5.1                                  | ESP-IDF Release/v5.4+                 | ESP-IDF Release/v5.5+                 |
| -------- | ------------------------------------------------------------ | ------------------------------------------------------------ |------------------------------ |------------------------------ |
| ESP32 | Supported | Supported | Supported | Supported |
| ESP32-S3 | N/A | Supported | Supported | Supported |
| ESP32-C2 | Supported | Supported | Supported | Supported |
| ESP32-C3 | Supported | Supported | Supported | Supported |
| ESP32-C5 | N/A | N/A | Supported | Supported |
| ESP32-C6 | N/A | Supported | Supported | Supported |
| ESP32-H2 | N/A | Supported | Supported | Supported |
| ESP32-C61 | N/A | N/A  | N/A | Supported |

## Compression ratio

| Chip     | Demo            | Original firmware size(Bytes) | Compressed firmware size(Bytes) | Compression ratio(%) |
| -------- | --------------- | ----------------------------- | ------------------------------- | -------------------- |
| ESP32-C3 | hello_world     | 156469                        | 75140                           | 51.98%               |
| ESP32-C3 | esp_http_client | 954000                        | 502368                          | 47.34%               |
| ESP32-C3 | wifi_prov_mgr   | 1026928                       | 512324                          | 50.11%               |

All of the above examples are compiled on ESP-IDF Tag/v5.0.  

The compression ratio is `(Original_firmware_size-Compressed_firmware_size)/Original_firmware_size `, and the larger the compression ratio, the smaller the size of the generated compressed firmware. In most cases, the compression ratio is 40%~60%. Therefore, the size of the partition storing the compressed firmware can be reserved as 60% of the size of the normal firmware partition.

## Advantages

The advantages of compressed OTA include:  

- Save flash space. The size of compressed firmware is smaller, therefore it requires less storage space. For example, the size of the original firmware is 1MB, if you use the full OTA, you have to use two partitions with the size of 1MB. In this case, you have to use a 4M flash. However, with compressed OTA, you can set up a partition with 1MB and a partition with 640KB. Now you can use the 2MB flash to complete your project.

    ```
        ****************               ****************
        * ota_0 (1M）  *               * ota_0 (1M)   *
        ****************               ****************
        * ota_1 (1M)   *               * ota_1 (640KB)*
        ****************               ****************
           (Full OTA)                  (Compressed OTA)
    ```

- Save transmission time and improve upgrade success rate. With small firmware size, compression upgrade can shorten the time required to transmit firmware. In this case, it can also improve the upgrade success rate for devices with limited bandwidth or poor network environment.   
- Save transmission traffic. The size of compressed firmware is smaller than the original firmware, so the traffic required for transmission is smaller. If your product is charged by data traffic, then this method can save your maintenance costs.
- Save OTA power consumption. Our test shows that during an OTA process, the power consumption used to transmit firmware accounts for 80% of the entire power consumption for OTA. Therefore, the smaller firmware size, the less power consumption.
- No new APIs are required. This scheme supports native APIs(such as [esp_ota](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/ota.html#api-reference)、[esp_https_ota](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/esp_https_ota.html#api-reference)) in the ESP-IDF.
- The scheme is fully compatible with Secure Boot and Flash Encryption.

## How to use bootloader support plus

In order to use compressed updates, you need to make the following changes:

1. Refer to [bootloader override example](https://github.com/espressif/esp-idf/tree/master/examples/custom_bootloader/bootloader_override) to add `bootloader_components` directory.  

2. Create the manifest idf_component.yml in the `bootloader_components` directory.

   ```yaml
   dependencies:
     espressif/bootloader_support_plus: ">=0.1.0"
   ```

3. Add the `bootloader_custom_ota.h` to `bootloader_components/main/bootloader_start.c`.

   ```c
   #if defined(CONFIG_BOOTLOADER_COMPRESSED_ENABLED)
   #include "bootloader_custom_ota.h"
   #endif
   ```

4. Call `bootloader_custom_ota_main()` in `bootloader_components/main/bootloader_start.c`.

   ```c
   ...
   bootloader_state_t bs = {0};
   int boot_index = select_partition_number(&bs);
   if (boot_index == INVALID_INDEX) {
       bootloader_reset();
   }
   #if defined(CONFIG_BOOTLOADER_COMPRESSED_ENABLED)
   // 2.1 Call custom OTA routine
    boot_index = bootloader_custom_ota_main(&bs, boot_index);
   #endif
   ```

5. Add `bootloader_support_plus` component to your project. You can add this component to your project via `idf.py add-dependency`, e.g.

   ```shell
   idf.py add-dependency "espressif/bootloader_support_plus=*"
   ```

   Alternatively, you can create the manifest idf_component.yml in the project's main component directory.

   ```yaml
   dependencies:
     espressif/bootloader_support_plus: ">=0.1.0"
   ```

   More is in [Espressif's documentation](https://docs.espressif.com/projects/idf-component-manager/en/latest/index.html).

6. Create a partition table that supports compressed updates. By default, the last partition of type `app` in the partition table is used as the partition for storing compressed firmware. Because the size of the compressed firmware will be smaller than that of the original firmware, the partition for storing the compressed firmware can be set smaller. The following is a typical partition table used in compressed updates:

   ```
   # Name,   Type, SubType, Offset,   Size, Flags
   phy_init, data, phy,       ,        4k
   nvs,      data, nvs,       ,        28k
   otadata,  data, ota,       ,        8k
   ota_0,    app,  ota_0,     ,        1216k,
   ota_1,    app,  ota_1,     ,        640k,
   ```

7. Enable compressed OTA support by running `idf.py menuconfig -> Bootloader config (Custom) -> Enable compressed OTA support` in the project directory.

## How to generate compressed firmware

The [cmake_utilites](https://components.espressif.com/components/espressif/cmake_utilities) component is used to generate compressed firmware. After adding this component, you can generate a compressed firmware by running:

```
idf.py gen_compressed_ota
```

Run the above command in your project directory will generate the `build/custom_ota_binaries/app_name.bin.xz.packed` in the current directory. The file named `app_name.bin.xz.packed` is the compressed firmware. For more information please refer to [Gen Compressed OTA](https://github.com/espressif/esp-iot-solution/blob/master/tools/cmake_utilities/docs/gen_compressed_ota.md) .

## Application Example

A full example using the `bootloader support plus` is available in esp-iot-solution: [ota/simple_ota_example](https://github.com/espressif/esp-iot-solution/tree/master/examples/ota/simple_ota_example).

You can create a project from this example by the following command:

```
idf.py create-project-from-example "espressif/bootloader_support_plus=*:simple_ota_example"
```

## Note
1. If your flash partition has only two partitions, one for storing app.bin and one for storing compressed firmware, then you cannot rollback to the old version if using compression upgrade. In this case, please ensure the availability and correctness of your compressed firmware before upgrading to it.    
2. When secure boot or flash encryption are enabled, note that the size of the partition storing compressed firmware > (size of compressed firmware + 8KB).

### Q&A

Q1. I encountered the following problems when using the package manager

```
Executing action: create-project-from-example
CMakeLists.txt not found in project directory /home/username
```

A1. This is because an older version package manager was used, please run `pip install -U idf-component-manager` in ESP-IDF environment to update.

Q2. Can a device that has used the esp-bootloader-plus solution use the bootloader-support-plus solution?

A2. Yes. You can use script [gen_custom_ota.py](https://github.com/espressif/esp-iot-solution/blob/master/tools/cmake_utilities/scripts/gen_custom_ota.py) to execute the following commands to generate compressed firmware suitable for the esp bootloader plus solution:

```
python3 gen_custom_ota.py -hv v2 -i simple_ota.bin
```

In addition, because the two solutions adopt different file formats and different mechanisms for triggering updates, please ensure that devices that have already used the esp bootloader plus solution continue to use the partition table that contains the storage partition with subtype 0x22:

```
# Name,   Type, SubType, Offset,   Size, Flags
phy_init, data, phy, 0xf000,        4k
nvs,      data, nvs,       ,        28k
otadata,  data, ota,       ,        8k
ota_0,    app,  ota_0,     ,        1216k,
storage,  data, 0x22,      ,        640k,
```

Then enable the `CONDIF_ENABLE_LEGACY_ESP_BOOTLOADER_PLUS_V2_SUPPORT` option in menuconfig.

In the end, you just need to download the compressed firmware to the storage partition and reboot the device to trigger the update.

## Contributing

We welcome contributions to this project in the form of bug reports, feature requests and pull requests.

Issue reports and feature requests can be submitted using Github Issues: https://github.com/espressif/esp-iot-solution/issues. Please check if the issue has already been reported before opening a new one.

Contributions in the form of pull requests should follow ESP-IDF project's [contribution guidelines](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/contribute/index.html). We kindly ask developers to start a discussion on an issue before proposing large changes to the project.
