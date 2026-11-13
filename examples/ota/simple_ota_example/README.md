| Supported Targets | ESP32-C2 | ESP32-C3 | ESP32-C5 | ESP32-C6 | ESP32-C61 | ESP32 | ESP32-S3 | ESP32-H2 | ESP32-P4 | ESP32-S31 |
| ----------------- | ----- | -------- | -------- | -------- | -------- | -------- |-------- | -------- | -------- | --------- |

# Simple OTA example
## Overview

ESP32-P4 requires ESP-IDF v5.5.5 or newer and chip revision 3 or newer. `CONFIG_ESP32P4_SELECTS_REV_LESS_V3` is not supported.

ESP32-S31 requires ESP-IDF v6.1 or newer. On IDF 6.1 it is a preview target, so select it with `idf.py --preview set-target esp32s31`.

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

Differential compressed OTA uses three app slots. `ota_0` and `ota_1` take turns as the running firmware and the patched firmware. The last slot, `ota_2`, stores the packed patch. Shown here with the device running from `ota_0`:

```
    ************                        *************
    *bootloader* -- decompress+ddelta ->*   ota_1   *
    ************                        *************
         ^    ^                              new
         |    |
         |    | read base
         |    +--------------*************
         |                   *   ota_0   *
         |                   *************
         |
         | read patch
         |
         v
    *************
    *   ota_2   *
    *************
```

When the device is running from `ota_1`, the bootloader reads that slot as the base and writes the new firmware to `ota_0`. The patch stays in `ota_2`.

The example uses a 4MB flash for both compressed and differential updates. Build with [sdkconfig.defaults.diff_ota](sdkconfig.defaults.diff_ota) to select [partition_for_diff_ota.csv](partition_for_diff_ota.csv): `phy_init`, `nvs`, and `otadata` match the compressed table, `ota_0` and `ota_1` are 1408KB each, and `ota_2` is 640KB (enough for a ~600KB packed compressed image or a ddelta patch). Leave that defaults file out to keep `partition_for_compressed_ota.csv`.

1. Build a ddelta patch from the firmware on the device and the new firmware, then xz-compress and pack it.
2. The device downloads the packed patch into `ota_2` and restarts.
3. The bootloader checks the v3 header and the base CRC, then stream-decompresses and applies the patch, writing the new firmware to the other app slot.
4. Boot the new firmware.

## How to use the example

The example is a copy of [simple_ota_example](https://github.com/espressif/esp-idf/tree/master/examples/system/ota/simple_ota_example). Please refer the [README.md](https://github.com/espressif/esp-idf/tree/master/examples/system/ota) for the HTTPS server and Wi-Fi setup.

`version.txt` in this directory is the application version. The boot log prints it as `App version`. The two tests below use version `1` for the firmware on the device and version `2` for the firmware the update produces.

Wi-Fi credentials are under `Example Connection Configuration`. Set `Example Configuration -> firmware upgrade url endpoint` to the file served by the OTA server. When that URL uses an IP address and the server certificate CN does not match, enable `Skip server certificate CN field check`.

Running `gen_compressed_ota` or `gen_diff_ota` recreates `build/custom_ota_binaries`.

## Compressed OTA

Leave `Example Configuration -> Support differential compressed OTA` off. The example then keeps [partition_for_compressed_ota.csv](partition_for_compressed_ota.csv): the compressed image is stored in the last app slot, and the bootloader decompresses it into `ota_0`.

1. Set `version.txt` to `1`.
2. Flash and run this firmware. The download fails until the server has an image; the device stays on version `1`.

    ```plaintext
    idf.py flash monitor
    ```

3. Set `version.txt` to `2`. Do not flash after this change.
4. Build version `2` and pack it. This command compiles the project first:

    ```plaintext
    idf.py gen_compressed_ota
    ```

    `build/custom_ota_binaries/simple_ota.bin.xz.packed` is the file to transfer. `simple_ota.bin.xz` is the compressed payload without the custom header. With [secure boot](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/security/secure-boot-v2.html) enabled, the signed file is `simple_ota.bin.xz.packed.signed`.

    To compress an existing binary instead:

    ```plaintext
    python3 gen_custom_ota.py -hv v3 -i simple_ota.bin --add_app_header
    ```

5. Copy `simple_ota.bin.xz.packed` to the path named in the firmware upgrade URL.
6. Reset the device. It downloads the package, reboots, and the bootloader decompresses it. The next boot log shows `App version: 2`.

## Differential compressed OTA

Build with [sdkconfig.defaults.diff_ota](sdkconfig.defaults.diff_ota) to select [partition_for_diff_ota.csv](partition_for_diff_ota.csv) and enable `Example Configuration -> Support differential compressed OTA`. `ota_0` and `ota_1` alternate as the running firmware and the patched firmware. `ota_2` stores the packed patch.

```plaintext
idf.py '-DSDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.defaults.diff_ota' set-target esp32c3
```

Alternatively, `export SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.diff_ota"` before `set-target`. Reconfigure with the same value if you switch between compressed and differential builds.

This path needs `bootloader_support_plus` >=0.8.0 and `cmake_utilities` >=1.4.1. `include(gen_diff_ota)` is already in this example. `idf.py gen_diff_ota` compiles the current project and uses `build/simple_ota.bin` as the base. Pass the new firmware with `-DDIFF_OTA_NEW_FILE`. Copy that file aside first: the command overwrites `build/simple_ota.bin`. `esp_ddelta` must be installed in the ESP-IDF Python. See [esp_ddelta/README.md](../../../tools/cmake_utilities/scripts/esp_ddelta/README.md).

Use `version.txt` as the application version and keep the binary of every release. The patch base (`-b`, or the firmware `gen_diff_ota` just built) has to be the image running on the device. The packed header stores a CRC32 of that whole file. The bootloader checks it before erasing the other app slot. `Skip full base image validation in ddelta OTA` stays y unless you also want `esp_image_verify()` on the base. `Skip full new image validation in ddelta OTA` stays y unless a longer boot is acceptable: set it to n to verify the rebuilt app before switching otadata. Flash encryption and secure boot repeat that signature check when the image is loaded.

`esp_https_ota` reads 1024 bytes of image header. If `gen_diff_ota` writes a packed patch shorter than that, the build prints a message. Pack again with `--pad_https_ota`, as described in [gen_compressed_ota.md](../../../tools/cmake_utilities/docs/gen_compressed_ota.md).

1. Apply the build settings above. Point the firmware upgrade URL at the packed patch, for example `https://192.168.0.3:8070/patch.xz.packed`.
2. Set `version.txt` to `2`, build the new app, and copy it out:

    ```plaintext
    idf.py build
    mkdir backup
    cp build/simple_ota.bin backup/simple_ota.bin
    ```

    `backup/simple_ota.bin` is the new app (version `2`).

3. Set `version.txt` back to `1`. This is the base that will be flashed.
4. Build the base and the patch:

    ```plaintext
    idf.py -DDIFF_OTA_NEW_FILE=backup/simple_ota.bin gen_diff_ota
    ```

    The path is relative to the project directory. The command writes `build/custom_ota_binaries/patch.xz.packed`. Host that file, not `patch.xz`.

    If both binaries already exist, pack them with the same arguments that `gen_diff_ota` uses. `-b` is the old firmware on the device (version `1`, `build/simple_ota.bin` after this step). `-i` is the new firmware (`backup/simple_ota.bin`). `--add_app_header` is required. `esp_ddelta` must be installed in that Python. See [gen_compressed_ota.md](../../../tools/cmake_utilities/docs/gen_compressed_ota.md).

    ```plaintext
    python3 /path/to/scripts/gen_custom_ota.py \
        -hv v3 -c xz -d ddelta -b build/simple_ota.bin -i backup/simple_ota.bin --add_app_header
    ```

    Run it from the example directory. It writes `custom_ota_binaries/patch.xz.packed` in the current directory. Add `--pad_https_ota` when that file is shorter than 1024 bytes.

5. Copy `patch.xz.packed` to the path named in the firmware upgrade URL.
6. Flash and run the base (version `1`):

    ```plaintext
    idf.py flash monitor
    ```

    The device downloads the patch into `ota_2` and restarts. The bootloader checks the base, applies the patch, and boots version `2` from the other app slot. The next boot log shows `App version: 2`.

## Compressed OTA Example Output

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

## Diff OTA Example Output
```
I (4399) simple_ota_example: Starting OTA example task
I (4399) simple_ota_example: Attempting to download update from https://192.168.47.103:8070/diff_ota.xz.packed
I (4409) main_task: Returned from app_main()
W (4459) esp-tls-mbedtls: skip_common_name=true: hostname matching and SNI disabled. This disables ALL server-name authentication (CN/SAN/SNI), not just CN. Only intended for loopback / debug clients.
I (4829) esp_https_ota: Starting OTA...
I (4829) custom_image: OTA to ota_2
I (4829) esp_https_ota: Writing to <ota_2> partition at offset 0x2e0000
I (4959) simple_ota_example: OTA Succeed, Rebooting...
I (4959) wifi:state: run -> init (0x0)
I (4959) wifi:pm stop, total sleep time: 762418 us / 1609640 us
I (37) boot: ESP-IDF v6.1-1012-g0e2b199153c 2nd stage bootloader
I (37) boot: compile time Oct 30 2026 17:12:03
I (37) boot: chip revision: v0.3
I (38) boot.esp32c3: SPI Speed      : 80MHz
I (42) boot.esp32c3: SPI Mode       : DIO
I (46) boot.esp32c3: SPI Flash Size : 4MB
I (50) boot: Partition Table:
I (52) boot: ## Label            Usage          Type ST Offset   Length
I (59) boot:  0 phy_init         RF data          01 01 0000c000 00001000
I (65) boot:  1 nvs              WiFi data        01 02 0000d000 00007000
I (72) boot:  2 otadata          OTA data         01 00 00014000 00002000
I (78) boot:  3 ota_0            OTA app          00 10 00020000 00160000
I (85) boot:  4 ota_1            OTA app          00 11 00180000 00160000
I (91) boot:  5 ota_2            OTA app          00 12 002e0000 000a0000
I (98) boot_custom: bootloader plus version: 0.6.0
I (104) boot_custom: ddelta base slot 0, write slot 1
I (107) boot_custom: start OTA to slot 1
I (11149) ddelta: ddelta OK
I (11215) boot_custom: OTA success, boot from slot 1
```

## Troubleshooting

### Common Issues

**1. `base app check error`**

**Solutions:**

- Verify that the app currently running on the device is the same version as the one used to create the patch.

**2. `Complete headers were not received`**

`esp_https_ota` reads 1024 bytes of image header before the HTTP response may end. Hosting `patch.xz` or `simple_ota.bin.xz` instead of the `.packed` file stops the download with:

```
E esp_https_ota: Complete headers were not received
```

**Solutions:**

- Host `build/custom_ota_binaries/patch.xz.packed` or `simple_ota.bin.xz.packed`. If the build warned that the packed file is shorter than 1024 bytes, remake it with `--pad_https_ota`. The extra bytes are zeros after the payload and are not covered by the custom header length or MD5.
