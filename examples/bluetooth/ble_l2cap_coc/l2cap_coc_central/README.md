| Supported Targets | ESP32 | ESP32-C3 | ESP32-C2 | ESP32-S3 | ESP32-H2 |
| ----------------- | ----- | -------- | -------- | -------- | -------- |

# BLE L2CAP CoC Central Example

(See the README.md file in the upper level 'examples' directory for more information about examples.)

This example performs passive scan, prints scan results, connects to a peripheral that matches
the configured peer device name, and then creates an L2CAP CoC channel (PSM configurable via Kconfig,
default 0x00EF). It sends data periodically over the established channel.

## How to Use Example

Before project configuration and build, be sure to set the correct chip target using:

```bash
idf.py set-target <chip_name>
```

### Hardware Required

* A development board with ESP32/ESP32-C3/ESP32-C2/ESP32-S3/ESP32-H2 SoC
* A USB cable for Power supply and programming

See [Development Boards](https://www.espressif.com/en/products/devkits) for more information about it.

### Configure the Project

Open the project configuration menu:

```bash
idf.py menuconfig
```

In the `Example Configuration` menu:

* Set `Peer Device Name` to match the peripheral device name you want to connect to (default `BLE_L2CAP_COC_PERIPH`).
* Configure `L2CAP CoC PSM` value (default `0x00EF`). Valid range: 0x0001-0x00FF. Both central and peripheral must use the same PSM value.

In the `Component Config` menu:

* Change Component config → Bluetooth → NimBLE Options → L2CAP → Maximum number of connection oriented channels to a value greater than 0
* Change Component config → Bluetooth → NimBLE Options → Memory Settings → MSYS_1 Block Size to 512 (for maximum transmission of data)

### Build and Flash

Run `idf.py -p PORT flash monitor` to build, flash and monitor the project.

(To exit the serial monitor, type ``Ctrl-]``.)

See the [Getting Started Guide](https://idf.espressif.com/) for full steps to configure and use ESP-IDF to build projects.
