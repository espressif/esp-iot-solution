## TinyUF2 NVS Example

The TinyUF2 supports dumping NVS key-value pairs to the config file, i.e., users can access and modify the NVS data and subsequently write the updated values back to the NVS.

This example demonstrates a new Wi-Fi Provisioning solution through TinyUF2.

## How to use the example

## Build and Flash

Run `idf.py -p PORT flash monitor` to build and flash the project.

(To exit the serial monitor, type Ctrl-].)

See the [Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/get-started/index.html) for full steps to configure and use ESP-IDF to build projects.

## Provisioning through Modify `INI` file

* Build and flash the firmware.
* Insert `ESP32S3` to PC through USB, New disk name with `ESP32S2-UF2` will be found in File Manager.
* Open the `CONFIG.INI`, edit `ssid` and `password` then save.
* The saved file will be parsed by ESP32S3, then write the key-value pairs to NVS.
* The new `ssid` and `password` will be used for Wi-Fi connect.

![UF2 Disk](../../../../components/usb/esp_tinyuf2/uf2_disk.png)

## Example Output

```
I (0) cpu_start: Starting scheduler on APP CPU.
I (448) uf2_nvs_example: no ssid, give a init value to nvs
I (448) uf2_nvs_example: no password, give a init value to nvs
I (448) TUF2: UF2 Updater install succeed, Version: 0.0.1
I (778) tud_mount_cb: 
I (27608) uf2_nvs_example: uf2 nvs modified

....
I (32888) uf2_nvs_example: wifi start ssid:ESP password:espressif
I (32908) wifi:new:<1,1>, old:<1,0>, ap:<255,255>, sta:<1,1>, prof:1
I (33948) wifi:state: init -> auth (b0)
I (33958) wifi:state: auth -> assoc (0)
I (33978) wifi:state: assoc -> run (10)
I (34098) wifi:connected with ESP, aid = 1, channel 1, 40U, bssid = bc:5f:f6:1b:28:5e
I (34098) wifi:security: WPA2-PSK, phy: bgn, rssi: -26
I (34108) wifi:pm start, type: 1
I (34108) wifi:set rx beacon pti, rx_bcn_pti: 0, bcn_timeout: 0, mt_pti: 25000, mt_time: 10000
I (34108) wifi:AP's beacon interval = 102400 us, DTIM period = 1
I (34128) wifi:<ba-add>idx:0 (ifx:0, bc:5f:f6:1b:28:5e), tid:0, ssn:3, winSize:64
I (35618) esp_netif_handlers: sta ip: 192.168.1.9, mask: 255.255.255.0, gw: 192.168.1.1
I (35618) uf2_nvs_example: got ip:192.168.1.9
```