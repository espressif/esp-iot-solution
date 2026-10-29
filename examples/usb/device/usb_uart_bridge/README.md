## USB to UART Bridge

USB to serial bridge tool using the USB and UART capabilities of the ESP32-S2/S3 microcontroller. This tool serves as a debugging and downloading utility with the following features:

1. Bidirectional transparent data communication between USB and UART.
2. Configurable serial port settings (supports baud rates up to 3000000 bps).
3. Compatibility with automatic firmware download functionality for `esptool`, enabling firmware updates for other ESP SoCs.

```c
/*
 *                                │
 *                                │USB
 *                                │
 *                                ▼
 *                            ┌──────┐
 *                            │      │
 *                            │ O  O │
 *                            │      │
 *                        ┌───┴──────┴───┐
 *                        │              │
 * ┌───────────┐          │ ESP32-S3-OTG │
 * │           │          │              │
 * │           │ RX       │ ┌──┐         │
 * │           │◄─────────┼─┤  │expend IO│
 * │           │ TX       │ │  │         │
 * │Target_MCU │◄─────────┼─┤  │         │
 * │           │ IO0      │ │  │         │
 * │           │◄─────────┼─┤  │         │
 * │           │ EN       │ │  │         │
 * │           │◄─────────┼─┤  │         │
 * │           │          │ └──┘         │
 * └───────────┘          │              │
 *                        │              │
 *                        │              │
 *                        └───┬──────┬───┘
 *                            │ [  ] │
 *                            └──────┘
 */
```

### Default IO Configuration

The example using `ESP32-S3-OTG` board by default, the IO configuration is as follows:

| ESP32-Sx IO | Function  |             Remark             |
| :---------: | :-------: | :----------------------------: |
|     20      |  USB D+   |             Fixed              |
|     19      |  USB D-   |             Fixed              |
|     46      |  UART-RX  |    Link to target board TX     |
|     45      |  UART-TX  |    Link to target board RX     |
|     26      |   Boot    |    Link to target board Boot   |
|      3      | EN (RST)  |  Link to target board EN(RST)  |

If you are using `ESP32-P4-Function-EV-Board` board, the IO configuration is as follows:

| ESP32-P4 IO | Function  |             Remark             |
| :---------: | :-------: | :----------------------------: |
|   pin 50    |  USB D+   |             Fixed              |
|   pin 49    |  USB D-   |             Fixed              |
|     27      |  UART-RX  |    Link to target board TX     |
|     46      |  UART-TX  |    Link to target board RX     |
|      6      |   Boot    |    Link to target board Boot   |
|      5      | EN (RST)  |  Link to target board EN(RST)  |

### Build and Flash

1. Make sure `ESP-IDF` is setup successfully

2. Set up the `ESP-IDF` environment variables, please refer [Set-up the environment variables](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html#step-4-set-up-the-environment-variables), Linux can use:

    ```bash
    . $HOME/esp/esp-idf/export.sh
    ```

3. Set ESP-IDF build target to `esp32s2`, `esp32s3` or `esp32p4`

    ```bash
    idf.py set-target esp32s3
    ```

4. Build, Flash, output log

    ```bash
    idf.py build flash monitor
    ```

### Automatic Firmware Download with esptool

* The automatic download feature is enabled by default. You can disable this option in `menuconfig → USB UART Bridge Demo → Enable auto download`.
* When using the automatic download feature, connect the development board's `AUTODLD_EN_PIN` and `AUTODLD_BOOT_PIN` pins to the target MCU's IO0 (Boot) and EN (RST) pins respectively.
* The automatic download feature may not function correctly on certain development boards (due to delays caused by RC circuits). In such cases, please fine-tune the code with gpio_set_level software delays.

### Wireless USB UART Bridge

Please refer to [Wireless USB UART Bridge](https://github.com/espressif/esp-dev-kits/tree/master/examples/esp32-s3-usb-bridge/examples/usb_wireless_bridge).
