# Bootloader TinyUF2 Example

UF2 is a file format developed by Microsoft for [PXT](https://github.com/Microsoft/pxt). It is particularly suitable for flashing microcontrollers through Mass Storage Class devices. For a more accessible explanation, check out [this blog post](https://makecode.com/blog/one-chip-to-flash-them-all).

## How to Use

### Build and Flash

Run `idf.py build` to compile the example.

(To exit the serial monitor, press Ctrl-])

Refer to the [Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/get-started/index.html) for detailed steps on configuring and using ESP-IDF to build projects.

### Drag and Drop for Upgrades

* Build and flash the initial firmware with Bootloader TinyUF2 support.
* By pulling the `CONFIG_BOOTLOADER_UF2_GPIO_NUM` pin to `CONFIG_BOOTLOADER_UF2_GPIO_LEVEL` for `CONFIG_BOOTLOADER_UF2_GPIO_PULL_TIME_SECONDS` seconds, the device will enter TinyUF2 mode.
* In TinyUF2 mode, the indicator LED connected to `BOOTLOADER_UF2_LED_INDICATOR_GPIO_NUM` will start blinking.
* Plug the `ESP32S3` into a computer via USB, and a new disk named `ESP32S3 - UF2` will appear in the file manager.
* Run `idf.py uf2-ota` to generate or convert subsequent firmware upgrade files into UF2 format. For more details, see [esp_tinyuf2](../../../../components/usb/esp_tinyuf2/).
* Drag and drop the UF2 firmware onto the disk to perform the upgrade.
* After the upgrade, the device will automatically restart and boot from the `Factory` partition.

![UF2 Disk](../../../../components/usb/esp_tinyuf2/uf2_disk.png)

### Modify NVS Configurations via `INI` File

Preparation:

* Build and flash the initial firmware with Bootloader TinyUF2 support.
* By pulling the `CONFIG_BOOTLOADER_UF2_GPIO_NUM` pin to `CONFIG_BOOTLOADER_UF2_GPIO_LEVEL` for `CONFIG_BOOTLOADER_UF2_GPIO_PULL_TIME_SECONDS` seconds, the device will enter TinyUF2 mode.
* Plug the `ESP32S3` into a computer via USB, and a new disk named `ESP32S3 - UF2` will appear in the file manager.
* Open the `CONFIG.INI` file, modify the configurations, and save the file.
* The saved file will be automatically parsed, and the new key-value pairs will be written to NVS.

### Known Issues

The following error message may appear:
```
uf2: No such namespace entry was found
```

- This occurs because the namespace `CONFIG_BOOTLOADER_UF2_NVS_NAMESPACE_NAME` (uf2_nvs) does not yet exist in the NVS partition. It does not affect normal functionality.

### Additional Documentation

[bootloader_uf2](https://docs.espressif.com/projects/esp-iot-solution/en/latest/usb/usb_device/esp_tinyuf2.html)

### Example Output

```
I (8456) esp_image: segment 0: paddr=00010020 vaddr=3f000020 size=0857ch ( 34172) map
I (8466) esp_image: segment 1: paddr=000185a4 vaddr=3ffbd8c0 size=01d98h (  7576)
I (8466) esp_image: segment 2: paddr=0001a344 vaddr=40022000 size=05cd4h ( 23764)
I (8476) esp_image: segment 3: paddr=00020020 vaddr=40080020 size=18248h ( 98888) map
I (8496) esp_image: segment 4: paddr=00038270 vaddr=40027cd4 size=05be4h ( 23524)
I (8506) esp_image: segment 5: paddr=0003de5c vaddr=50000000 size=00010h (    16)
I (8556) uf2_example: Firmware update complete
I (8556) uf2_example: Restarting in 5 seconds...
I (9556) uf2_example: Restarting in 4 seconds...
I (10556) uf2_example: Restarting in 3 seconds...
I (11556) uf2_example: Restarting in 2 seconds...
I (12556) uf2_example: Restarting in 1 seconds...
I (13556) uf2_example: Restarting in 0 seconds...
I (14556) uf2_example: Restarting now
```
