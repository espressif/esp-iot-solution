## ESP TinyUF2

The enhanced version of [TinyUF2](https://github.com/adafruit/tinyuf2) for ESP32Sx, which supports over-the-air (OTA) updates for APP. **and supports dumping NVS key-value pairs to the config file, users can modify and write back to NVS**.

UF2 is a file format developed by Microsoft for [PXT](https://github.com/Microsoft/pxt), that is particularly suitable for flashing microcontrollers over MSC (Mass Storage Class). For a more friendly explanation, check out [this blog post](https://makecode.com/blog/one-chip-to-flash-them-all).

## Support UF2 OTA/NVS in Your Project

1. Add the component to your project using `idf.py add_dependency` command.

    ```sh
    idf.py add-dependency "esp_tinyuf2"
    ```

2. Customer your partition table. Like other OTA solutions, you need to reserve at least two OTA app partitions. Please refer to [Partition Tables](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/partition-tables.html) and [usb_uf2_ota](../../../examples/usb/device/usb_uf2_ota/) example for details.

    ```
    # Partition Table Example
    # Name,   Type, SubType, Offset,  Size, Flags
    nvs,      data, nvs,     ,        0x4000,
    otadata,  data, ota,     ,        0x2000,
    phy_init, data, phy,     ,        0x1000,
    ota_0,    app,  ota_0,   ,        1500K,
    ota_1,    app,  ota_1,   ,        1500K,
    ```

3. Using `idf.py menuconfig` to config the component's behavior in `(Top) → Component config → TinyUF2 Config`

* `USB Virtual Disk size(MB)`: The size of the virtual U-disk shows in File Explorer, 8MB by default
* `Max APP size(MB)`: Maximum APP size, 4MB by default
* `Flash cache size(KB)`: Cache size used for writing Flash efficiently, 32KB by default
* `USB Device VID`: Espressif VID (0x303A) by default
* `USB Device PID`: Espressif test PID (0x8000) by default, refer [esp-usb-pid](https://github.com/espressif/usb-pids) to apply new.
* `USB Disk Name`:  The name of the virtual U-disk shows in File Explorer, "ESP32Sx-UF2" by default
* `USB Device Manufacture`: "Espressif" by default
* `Product Name`: "ESP TinyUF2" by default
* `Product ID`: 12345678 by default
* `Product URL`: A `index` file will be added to the U-disk, users can click to goto the webpage, "https://products.espressif.com/" by default
* `UF2 NVS ini file size`: The `ini` file size prepares for NVS function

4. Install tinyuf2 function like below, for more details, please refer example `usb_uf2_nvs` and `usb_uf2_ota`

    ```c
    /* install UF2 OTA */
    tinyuf2_ota_config_t ota_config = DEFAULT_TINYUF2_OTA_CONFIG();
    ota_config.complete_cb = uf2_update_complete_cb;
    /* disable auto restart, if false manual restart later */
    ota_config.if_restart = false;
    /* install UF2 NVS */
    tinyuf2_nvs_config_t nvs_config = DEFAULT_TINYUF2_NVS_CONFIG();
    nvs_config.part_name = "nvs";
    nvs_config.namespace_name = "myuf2";
    nvs_config.modified_cb = uf2_nvs_modified_cb;
    esp_tinyuf2_install(&ota_config, &nvs_config);
    ```

5. Run `idf.py build flash` for the initial download, later `idf.py uf2-ota` can be used to generate new `uf2` app bin

6. Drag and drop UF2 format file to the disk, to upgrade to new `uf2` app bin

![UF2 Disk](./uf2_disk.png)

## Enable UF2 USB Console

If enable UF2 USB console `(Top) → Component config → TinyUF2 Config → Enable USB Console For log`, the log will be output to the USB Serial port (Output to UART by default).


## Build APP to UF2 format

The new command `idf.py uf2-ota` is added by this component, which can be used to build the APP to UF2 format. After the build is complete, the UF2 file (`${PROJECT_NAME}.uf2`) will be generated in the current `project` directory.

```sh
idf.py uf2-ota
```

## Convert Existing APP to UF2 Format

To convert your existing APP binary to UF2 format, simply use the [uf2conv.py](./utils/uf2conv.py) on a `.bin` file, specifying the family id as `ESP32S2`, `ESP32S3` or their magic number as follows. And you must specify the address of 0x00 with the `-b` switch, the tinyuf2 will use it as offset to write to the OTA partition.

1. convert as follows

    using:

    ```sh
    $ENV{UF2_UTILS_PATH}/uf2conv.py your_firmware.bin -c -b 0x00 -f ESP32S3
    ```

    or:

    ```sh
    $ENV{UF2_UTILS_PATH}/uf2conv.py your_firmware.bin -c -b 0x00 -f 0xc47e5767
    ```

## Note

* To use the UF2 OTA function continuously, the TinyUF2 function must be enabled in the updated APP.