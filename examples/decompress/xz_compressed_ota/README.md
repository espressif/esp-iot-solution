# Compressed OTA example

This example is based on [app_update](https://github.com/espressif/esp-idf/tree/master/components/app_update) component's APIs and [esp-bootloader-plus](https://github.com/espressif/esp-bootloader-plus) component.

---
**NOTES**

- To run this example, it's recommended that you have an official ESP32-C3 development board.
---

## Project layout

```
bootloader_components        — Bootloader directory      
  + esp-xz                   - Esp xz decompress lib
  + main                     - Main source files of the bootloader
main 				         — Main source files of the application 
server_certs                 — The directory used to store certificate
Makefile/CMakeLists.txt      — Makefiles of the application project
...
```

To correctly generate the patch file or compressed firmware, please switch to the `bootloader_components\tools` directory and run the script `install_tools.sh` to install the necessary tools，Refer the [README.md](https://github.com/espressif/esp-idf/tree/master/examples/system/ota) in the `bootloader_components` directory for more information. 

## Configuration

Refer the [README.md](https://github.com/espressif/esp-idf/tree/master/examples/system/ota) in the `esp-idf\examples\system\ota` directory for the setup details. In addition, In order to use this example, you also need to know the following aspects.

### How to enable the AES entryption

In order to ensure the confidentiality of the transmission firmware, this example provides support for AES encryption. You can enable it in `Example Configuration-->Enable compressed app encryption` menu.  

Generate a random key by running:

```
espsecure.py generate_flash_encryption_key --keylen 128 encryption_key.bin
```

Besides, this example provides two methods to store  the AES encryption key:   
- Store the key in flash(Default).  
- Store the key in [efuse](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/efuse.html) .

 You can change it in `Example Configuration-->The location used to store encrypt key` menu. The command used to store the AES encryption key in the [efuse](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/efuse.html) with the help of [espefuse.py](https://github.com/espressif/esptool/wiki/espefuse) is just like:

```
  espefuse.py -p PORT burn_block_data --offset 16 BLOCK3 encryption_key.bin
```

### How to generate compressed app image

The script `custom_ota_gen.py` can be used to compress the specified app. For example, use the following command to generate a compressed firmware:

```
python3 custom_ota_gen.py -i compressed_ota.bin
```

If there are no errors, the `custom_ota_binaries` directory may contain the following file:

```
compressed_ota.bin.xz
compressed_ota.bin.xz.packed
```
The file named `compressed_ota.bin.xz.packed` is the actual compressed app binary file to be transferred.

In addition, if [secure boot](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/security/secure-boot-v2.html) is enabled, run the following command to  generate the signed compressed app binary file:
```
python3 custom_ota_gen.py -i compressed_ota.bin --sign_key secure_boot_signing_key.pem
```
Similarly, if the AES entryption is enabled, the command will generate the encrypted compressed app binary file: 
```
python3 custom_ota_gen.py -i compressed_ota.bin --encry_key encry_key.bin
```
If you are using differential update(diff OTA), you can use the following command to generate the patch file：

```
python3 custom_ota_gen.py -hv v2 -c xz -d ddelta -i new_compressed_ota.bin -b build/compressed_ota.bin
```

If there are no errors, the `custom_ota_binaries` directory will contain the patch file called `patch.xz.packed`.

For more information on `custom_ota_gen.py`, please run `python3 custom_ota_gen.py -h`.  

### Configure the project

Open the project configuration menu (`idf.py menuconfig`).

1. In the `Partition Table` menu:

  - Select `partition_for_compressed_ota.csv` if you want to use compressed OTA.
  - Select `partition_for_diff_ota.csv` if you want to use diff OTA.

2. In the `Example Connection Configuration` menu:
   The compressed firmware（or patch） can be downloaded to the device through the `esp_http_client` or `BLE`.

**Configuration for http transport**

  - Chose the `http download` in the `Download Mode` option.

  - Set the URL of the new firmware that you will download from in the `Firmware Upgrade URL` option, whose format should be `https://<host-ip-address>:<host-port>/<firmware-image-filename>`, e.g. `https://192.168.2.106:8070/compressed_ota.bin`

    **Notes:** The server part of this URL (e.g. `192.168.2.106`) must match the **CN** field used when [generating the certificate and key](https://github.com/espressif/esp-idf/tree/master/examples/system/ota#run-https-server).

**Configuration for BLE transport**

  - Chose the `BLE download` in the `Download Mode` option.
  - When choosing to use BLE to download firmware, please refer to the following steps:
      - Use the GATT client device to search for and connect to the device running this example, which broadcasts the names  `ESP_GATTS_OTA_42 ` and  `ESP_GATTS_OTA_50` .
    - Update and exchange the maximum MTU value supported by both client and server .
    - The GATT client writes the firmware or patch to be updated to `0xFF01` characteristic through `write request` .
    - The GATT client notifies the server that the firmware transfer is completed by writing any data of the `0xFF02` characteristic, and the server will verify the firmware and restart .
- If using a mobile APP as a GATT client, two Android APPs that can transfer files can be recommended: [BLE Debugger](https://www.xuetianli.com/azrj/4559.html) and [BLE Debug Assistant](https://www.xuetianli.com/azrj/28872.html) .


3. if `http download` is used，in the `Example Connection Configuration` menu:

- Choose the network interface in the `Connect using` option based on your board. Currently both Wi-Fi and Ethernet are supported
- If the Wi-Fi interface is used, provide the Wi-Fi SSID and password of the AP you wish to connect to
- If using the Ethernet interface, set the PHY model under `Ethernet PHY Device` option, e.g. `IP101`

4. In the `Bootloader config (Custom)`menu:

- Enable `Enable diff compressed ota support` if you want to use diff OTA.
