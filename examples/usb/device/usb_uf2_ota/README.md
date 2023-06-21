## UF2 OTA Example

UF2 is a file format developed by Microsoft for [PXT](https://github.com/Microsoft/pxt), that is particularly suitable for flashing microcontrollers over MSC (Mass Storage Class). For a more friendly explanation, check out [this blog post](https://makecode.com/blog/one-chip-to-flash-them-all).

Source project: https://github.com/adafruit/tinyuf2

## How to use example

## Build and Flash

Run `idf.py -p PORT flash monitor` to build and flash the project.

(To exit the serial monitor, type Ctrl-].)

See the [Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/get-started/index.html) for full steps to configure and use ESP-IDF to build projects.

## Convert firmware to UF2 Format

To create your own UF2 file, simply use the [Python conversion script](../../../../components/usb/esp_tinyuf2/utils/uf2conv.py) on a .bin file, specifying the family id as `ESP32S2`, `ESP32S3` or their magic number as follows. Note you must specify application address of 0x00 with the -b switch, the bootloader will use it as offset to write to ota partition.

1. add convert script to path
   ```
   export PATH=$PATH:./utils
   ```

2. convert as follow

    ```
    uf2conv.py firmware.bin -c -b 0x00 -f ESP32S2
    uf2conv.py firmware.bin -c -b 0x00 -f 0xbfdd4eee

    uf2conv.py firmware.bin -c -b 0x00 -f ESP32S3
    uf2conv.py firmware.bin -c -b 0x00 -f 0xc47e5767
    ```

## Drag and drop to upgrade

* Build and flash first UF2 supported firmware
* Convert subsequent firmware upgrades to UF2 format
* Drag and drop UF2 format firmware to the disk to upgrade

![UF2 Disk](./uf2_disk.png)

## Example Output

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