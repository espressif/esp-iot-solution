# bootloader support plus

[![Component Registry](https://components.espressif.com/components/espressif/bootloader_support_plus/badge.svg)](https://components.espressif.com/components/espressif/bootloader_support_plus)

* [中文版](https://github.com/espressif/esp-iot-solution/blob/master/components/bootloader_support_plus/README_CN.md)

## Overview

`bootloader support plus` is an enhanced bootloader based on [custom_bootloader](https://github.com/espressif/esp-idf/tree/master/examples/custom_bootloader). Its main features are:

- Decompress a compressed firmware in the bootloader and load the restored image.
- Decompress and decode a differentially compressed (ddelta) firmware in the bootloader and load the restored image.
- Use native ESP-IDF OTA APIs directly, such as [esp_ota](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/ota.html#api-reference) and [esp_https_ota](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/esp_https_ota.html#api-reference).

The following table shows the Espressif SoCs that are compatible with `bootloader support plus` and their corresponding ESP-IDF versions:

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
| ESP32-P4 | N/A | N/A | N/A | v5.5.5+, rev ≥ 3 |
| ESP32-S31 | N/A | N/A | N/A | v6.1+ (preview: `idf.py --preview set-target esp32s31`) |

## Compressed update

### Compression ratio

| Chip     | Demo            | Original firmware size(Bytes) | Compressed firmware size(Bytes) | Compression ratio(%) |
| -------- | --------------- | ----------------------------- | ------------------------------- | -------------------- |
| ESP32-C3 | hello_world     | 156469                        | 75140                           | 51.98%               |
| ESP32-C3 | esp_http_client | 954000                        | 502368                          | 47.34%               |
| ESP32-C3 | wifi_prov_mgr   | 1026928                       | 512324                          | 50.11%               |

All of the above examples are compiled on ESP-IDF Tag/v5.0.

The compression ratio is `(Original_firmware_size - Compressed_firmware_size) / Original_firmware_size`. The larger the compression ratio, the smaller the compressed firmware. In most cases the ratio is 40%–60%, so the partition that stores the compressed firmware can be reserved as about 60% of a normal firmware partition.

### Advantages

- Save flash space. The compressed firmware is smaller, so it needs less storage. For example, if the original firmware is 1MB, a full OTA needs two 1MB partitions and typically a 4MB flash. With compressed OTA you can use a 1MB partition and a 640KB partition, and finish the project on a 2MB flash.

    ```
        ****************               ****************
        * ota_0 (1M）  *               * ota_0 (1M)   *
        ****************               ****************
        * ota_1 (1M)   *               * ota_1 (640KB)*
        ****************               ****************
           (Full OTA)                  (Compressed OTA)
    ```

- Save transmission time and improve upgrade success rate. On bandwidth-limited or unreliable links, a smaller image shortens the transfer and raises the chance of a successful upgrade.
- Save transmission traffic. A smaller image uses less data. Products billed by traffic can cut maintenance cost.
- Save OTA power. Tests show that transferring the firmware accounts for more than 80% of OTA energy use, so less data means less power.
- No new APIs are required. The scheme supports native ESP-IDF OTA APIs such as [esp_ota](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/ota.html#api-reference) and [esp_https_ota](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/esp_https_ota.html#api-reference).
- Fully compatible with Secure Boot and flash encryption.

### How to generate compressed firmware

Use the [cmake_utilities](https://components.espressif.com/components/espressif/cmake_utilities) component. After adding it, run:

```
idf.py gen_compressed_ota
```

This creates `build/custom_ota_binaries/app_name.bin.xz.packed`, which is the compressed firmware to transfer. More details are in [Gen Compressed OTA](https://github.com/espressif/esp-iot-solution/blob/master/tools/cmake_utilities/docs/gen_compressed_ota.md).

## Differential update

Differential update builds on compressed OTA. The host packs an xz-compressed ddelta patch from the firmware currently running on the device (the base) and the new firmware. After download, the bootloader streams xz decompression into ddelta and writes the full new image.

Use at least three `app` partitions so OTA can roll back: the first two hold the running firmware and the patched firmware; the last one stores the packed patch. Native ESP-IDF OTA APIs (such as `esp_ota` and `esp_https_ota`) still download into that last partition.

### Advantages

- Smaller patches. Relative to app-layer delta schemes, a similar change set is typically about one third the size. Small code-only updates can shrink to roughly 300–400 bytes, which suits low-bandwidth links such as BLE and Zigbee.
- Less transfer time, traffic, and OTA power. The device downloads a patch instead of a full or fully compressed image.
- No new application OTA APIs. Decompression and patch apply stay in the bootloader; the app keeps using `esp_ota` / `esp_https_ota`.
- A single diff engine. App-layer delta OTA usually embeds the engine in both app slots. Here the engine lives only in the bootloader, so neither app needs to carry it.
- Rollback with dual app slots. With three `app` partitions, a failed patch or a failed new-image check can return to the previously running slot.

### Partition table and configuration

The [simple_ota_example](https://github.com/espressif/esp-iot-solution/tree/master/examples/ota/simple_ota_example) ships [partition_for_diff_ota.csv](../../examples/ota/simple_ota_example/partition_for_diff_ota.csv) for a 4MB flash: `ota_0` and `ota_1` are 1408KB, `ota_2` is 640KB. 1.4MiB is not a multiple of the 64KB app alignment, so 1408KB is the largest aligned size within 1.4MiB. 640KB fits a packed compressed image of about 600KB, or a ddelta patch.

```
# Name,   Type, SubType, Offset,   Size, Flags
phy_init, data, phy,       ,        4k
nvs,      data, nvs,       ,        28k
otadata,  data, ota,       ,        8k
ota_0,    app,  ota_0,     ,        1408k,
ota_1,    app,  ota_1,     ,        1408k,
ota_2,    app,  ota_2,     ,        640k,
```

Also enable `Enable compressed OTA support` and `Enable ddelta patch apply` under `Bootloader config (Custom)`. This option cannot be combined with legacy esp-bootloader-plus v2.

Building the patch requires [cmake_utilities](https://components.espressif.com/components/espressif/cmake_utilities) >=1.4.1 (`gen_diff_ota` and `esp_ddelta`). On reboot the bootloader checks the v3 header CRC of the whole base file (`base_len` and `base_crc32`). A mismatch returns to the previously running slot and does not erase the other firmware. It then streams xz decompression into ddelta and writes the new firmware to the app slot that is neither the base nor the last partition. Set `Skip full base image validation in ddelta OTA` to n to also run `esp_image_verify()` on the base before applying the patch. That checks image structure and, with secure boot, the signature. The file CRC still runs. `Skip full new image validation in ddelta OTA` defaults to y: otadata is updated after the patch is written, and the following load checks the new app. Set it to n to run `esp_image_verify()` on the rebuilt app before switching otadata. A failed check restores otadata to the previously running slot. With flash encryption and Secure Boot this is a second signature check and makes that boot longer.

Leave ddelta disabled for a pure compressed update. That path is unchanged.

### How to generate a patch

You need [cmake_utilities](https://components.espressif.com/components/espressif/cmake_utilities) >=1.4.1 and `esp_ddelta` installed in the Python used by ESP-IDF. Keep each release in `version.txt` and save that release's binary. The patch base (`-b`, or the firmware `gen_diff_ota` just built) must be the image running on the device. A later release cannot stand in for it.

After `include(gen_diff_ota)` in the project `CMakeLists.txt`, run:

```
idf.py -DDIFF_OTA_NEW_FILE=backup/app_name.bin gen_diff_ota
```

The command builds the project, uses `build/app_name.bin` as the base, and packs a patch against `DIFF_OTA_NEW_FILE` into `build/custom_ota_binaries/patch.xz.packed`. Copy the new firmware aside first: the build overwrites `build/app_name.bin`. Serve `patch.xz.packed`, not the unpacked `patch.xz`.

If both binaries already exist, call the script directly (`--add_app_header` is required for native OTA APIs):

```
python3 /path/to/scripts/gen_custom_ota.py \
    -hv v3 -c xz -d ddelta -b current_app.bin -i new_app.bin --add_app_header
```

If the packed patch is shorter than 1024 bytes and you use `esp_https_ota`, enable `CMake Utilities -> Pad packed OTA image to 1024 bytes`, or pass `--pad_https_ota`. More details are in [gen_compressed_ota.md](https://github.com/espressif/esp-iot-solution/blob/master/tools/cmake_utilities/docs/gen_compressed_ota.md) and [esp_ddelta/README.md](https://github.com/espressif/esp-iot-solution/blob/master/tools/cmake_utilities/scripts/esp_ddelta/README.md).

## How to use bootloader support plus

In order to use compressed updates, you need to make the following changes:

1. Refer to [bootloader override example](https://github.com/espressif/esp-idf/tree/master/examples/custom_bootloader/bootloader_override) to add a `bootloader_components` directory.

2. Create the manifest `idf_component.yml` in the `bootloader_components` directory.

   ```yaml
   dependencies:
     espressif/bootloader_support_plus: ">=0.1.0"
   ```

3. Add `bootloader_custom_ota.h` to `bootloader_components/main/bootloader_start.c`.

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

5. Add the `bootloader_support_plus` component to your project, for example:

   ```shell
   idf.py add-dependency "espressif/bootloader_support_plus=*"
   ```

   Alternatively, create `idf_component.yml` in the project's main component directory:

   ```yaml
   dependencies:
     espressif/bootloader_support_plus: ">=0.1.0"
   ```

   More is in [Espressif's documentation](https://docs.espressif.com/projects/idf-component-manager/en/latest/index.html).

6. Create a partition table that supports compressed updates. By default, the last partition of type `app` stores the compressed firmware. Because that image is smaller than the original firmware, the partition can be smaller. A typical compressed-update table:

   ```
   # Name,   Type, SubType, Offset,   Size, Flags
   phy_init, data, phy,       ,        4k
   nvs,      data, nvs,       ,        28k
   otadata,  data, ota,       ,        8k
   ota_0,    app,  ota_0,     ,        1216k,
   ota_1,    app,  ota_1,     ,        640k,
   ```

7. Enable compressed OTA with `idf.py menuconfig -> Bootloader config (Custom) -> Enable compressed OTA support`. For differential update, also enable `Enable ddelta patch apply` and use at least three `app` partitions; see **Differential update** above.

## Application Example

A full example is available in esp-iot-solution: [ota/simple_ota_example](https://github.com/espressif/esp-iot-solution/tree/master/examples/ota/simple_ota_example).

Create a project from this example with:

```
idf.py create-project-from-example "espressif/bootloader_support_plus=*:simple_ota_example"
```

## Note

1. With only two partitions—one for the app and one for the compressed firmware—compressed OTA cannot roll back. Validate the compressed image before upgrading.
2. When Secure Boot or flash encryption is enabled, the partition that stores the compressed firmware (or packed patch) must be larger than `compressed_size + 8KB`.
3. For differential update, keep versioned binaries of every release so the next patch can use the running image as its base.
4. After enabling differential updates, compressed updates are still downloaded to the last partition by default and decompressed to ota_0 (ota_index=0) in the bootloader, consistent with the original compressed updates.

### Q&A

Q1. I encountered the following problems when using the package manager:

```
Executing action: create-project-from-example
CMakeLists.txt not found in project directory /home/username
```

A1. This is because an older package manager was used. Run `pip install -U idf-component-manager` in the ESP-IDF environment to update.

Q2. Can a device that has used the esp-bootloader-plus solution use the bootloader-support-plus solution?

A2. Yes. You can use [gen_custom_ota.py](https://github.com/espressif/esp-iot-solution/blob/master/tools/cmake_utilities/scripts/gen_custom_ota.py) to generate compressed firmware for the esp-bootloader-plus solution:

```
python3 gen_custom_ota.py -hv v2 -i simple_ota.bin
```

Because the two solutions use different file formats and update triggers, devices that already use esp-bootloader-plus should keep a partition table with a storage partition of subtype `0x22`:

```
# Name,   Type, SubType, Offset,   Size, Flags
phy_init, data, phy, 0xf000,        4k
nvs,      data, nvs,       ,        28k
otadata,  data, ota,       ,        8k
ota_0,    app,  ota_0,     ,        1216k,
storage,  data, 0x22,      ,        640k,
```

Then enable `CONFIG_ENABLE_LEGACY_ESP_BOOTLOADER_PLUS_V2_SUPPORT` in menuconfig.

Download the compressed firmware to the storage partition and reboot to trigger the update.

## Contributing

We welcome contributions to this project in the form of bug reports, feature requests and pull requests.

Issue reports and feature requests can be submitted using Github Issues: https://github.com/espressif/esp-iot-solution/issues. Please check if the issue has already been reported before opening a new one.

Contributions in the form of pull requests should follow ESP-IDF project's [contribution guidelines](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/contribute/index.html). We kindly ask developers to start a discussion on an issue before proposing large changes to the project.
