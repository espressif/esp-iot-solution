## TinyUF2 OTA Example

UF2 is a file format developed by Microsoft for [PXT](https://github.com/Microsoft/pxt), that is particularly suitable for flashing microcontrollers over MSC (Mass Storage Class). For a more friendly explanation, check out [this blog post](https://makecode.com/blog/one-chip-to-flash-them-all).


## How to use the example

## Build and Flash

Run `idf.py -p PORT flash monitor` to build and flash the project.

(To exit the serial monitor, type Ctrl-].)

See the [Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/get-started/index.html) for full steps to configure and use ESP-IDF to build projects.

## Drag and drop to upgrade

* Build and flash the first TinyUF2 supported firmware
* Insert `ESP32S3` to PC through USB, New disk name with `ESP32S2-UF2` will be found in File Manager
* Run `idf.py uf2-ota` to generate or convert subsequent firmware upgrades to UF2 format, refer [esp_tinyuf2](../../../../components/usb/esp_tinyuf2/) for more details
* Drag and drop UF2 format firmware to the disk to upgrade

![UF2 Disk](../../../../components/usb/esp_tinyuf2/uf2_disk.png)

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