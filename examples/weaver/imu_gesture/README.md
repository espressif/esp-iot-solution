| Supported Targets | ESP32 | ESP32-C2 | ESP32-C3 | ESP32-C5 | ESP32-C6 | ESP32-C61 | ESP32-S3 |
| ----------------- | ----- | -------- | -------- | -------- | --------- | -------- | -------- |

# IMU (Inertial Measurement Unit) Gesture Example Based on BMI270

[中文](README_CN.md)

This example demonstrates how to build an IMU gesture detection device with ESP RainMaker Local Control protocol support, enabling local network discovery and state reporting.

## Supported Gestures

| Gesture | Description |
|---------|-------------|
| toss | Device is thrown up |
| flip | Device is flipped over |
| shake | Device is shaken |
| rotation | Device is rotated |
| push | Device is pushed |
| circle | Device draws a circle |
| clap_single | Single clap detected |
| clap_double | Double clap detected |
| clap_triple | Triple clap detected |

## Hardware Required

* One development board (ESP32/ESP32-C2/ESP32-C3/ESP32-C5/ESP32-C6/ESP32-C61/ESP32-S3)
* BMI270 sensor
* USB cable for power supply and programming
* Wi-Fi router for network connectivity

## How to Use Example

Before project configuration and build, be sure to set the correct chip target using `idf.py set-target <chip_name>`.

### Configure the Project

Open the project configuration menu:

```bash
idf.py menuconfig
```

In the `Example Connection Configuration` menu:
* Set Wi-Fi SSID
* Set Wi-Fi Password

In the `ESP Weaver` menu (optional):
* Set Security version (SEC0 or SEC1)

### Build and Flash

Build the project and flash it to the board, then run monitor tool to view serial output:

```bash
idf.py -p PORT flash monitor
```

(To exit the serial monitor, type ``Ctrl-]``.)

See the [Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/get-started/index.html) for full steps to configure and use ESP-IDF to build projects.

### Get PoP Code

After the device starts, the Local Control PoP code will be displayed in the serial log:

```
I (10573) esp_weaver_local_ctrl: PoP read from NVS
I (10573) esp_weaver_local_ctrl: PoP for local control: e37356df
```

The PoP is auto-generated randomly and persisted to NVS. On first boot you'll see `No PoP in NVS. Generating a new one.`, and on subsequent reboots `PoP read from NVS` with the same value. This PoP code is required when adding the device in the ESP-Weaver integration in Home Assistant.

### Add Device to Home Assistant

1. Install [ESP-Weaver](https://github.com/espressif/esp_weaver) integration in Home Assistant
2. The device will be automatically discovered after connecting to WiFi
3. Enter the device's PoP code to complete pairing
4. Device entities (gesture type, confidence, orientation, angle) will appear in Home Assistant

## Example Output

When the program starts, you will see the following log:

```
I (417) main_task: Calling app_main()
I (427) app_driver: === BMI270 Shake Detection Configuration ===
I (427) app_driver: Selected Board: ESP SPOT C5
I (427) app_driver: GPIO Configuration:
I (427) app_driver:   - Interrupt GPIO: 3
I (437) app_driver:   - I2C SCL GPIO: 26
I (437) app_driver:   - I2C SDA GPIO: 25
I (447) app_driver:   - I2C Type: Hardware I2C
I (447) app_driver: ================================
I (667) BMI2_ESP32: Detected FreeRTOS tick rate: 100 Hz
I (1987) bmi270_api: BMI270 sensor created successfully
I (1997) app_driver: Move the sensor to get shake interrupt...
I (1997) esp_weaver: Weaver initialized: ESP Weaver Device (IMU Gesture), node_id: D0CF13E28F50
I (1997) app_main: Node ID: D0CF13E28F50
I (1997) esp_weaver: Device created: IMU Gesture Sensor (esp.device.imu-gesture)
I (2007) esp_weaver: Device added to node: IMU Gesture Sensor
I (2017) app_main: IMU gesture device created and added to node
I (2017) example_connect: Start example_connect.
I (14017) example_connect: Got IPv6 event: Interface "example_netif_sta" address: fe80:0000:0000:0000:d2cf:13ff:fee2:8f50, type: ESP_IP6_ADDR_IS_LINK_LOCAL
I (14457) esp_netif_handlers: example_netif_sta ip: 192.168.1.103, mask: 255.255.255.0, gw: 192.168.1.1
I (14457) example_connect: Got IPv4 event: Interface "example_netif_sta" address: 192.168.1.103
I (14457) example_common: Connected to example_netif_sta
I (14467) example_common: - IPv4 address: 192.168.1.103,
I (14467) example_common: - IPv6 address: fe80:0000:0000:0000:d2cf:13ff:fee2:8f50, type: ESP_IP6_ADDR_IS_LINK_LOCAL
I (14477) esp_weaver_local_ctrl: Starting local control with HTTP transport and security version: 1
I (14487) mdns_mem: mDNS task will be created from internal RAM
I (14497) esp_weaver_local_ctrl: PoP read from NVS
I (14497) esp_weaver_local_ctrl: PoP for local control: b1acbcdf
I (14507) esp_weaver_local_ctrl: Local control started on port 8080, node_id: D0CF13E28F50
I (14517) app_main: Local control started successfully
I (14517) main_task: Returned from app_main()
```

> 💡 **Node ID**: Auto-generated from the MAC address by default. Can also be customized via `esp_weaver_config_t.node_id`.
>
> 💡 **PoP (Proof of Possession)**: In SEC1 mode, a random PoP is auto-generated and persisted to NVS. On first boot you'll see `No PoP in NVS. Generating a new one.`, and on subsequent reboots `PoP read from NVS` with the same value. You can also set a custom PoP by calling `esp_weaver_local_ctrl_set_pop()` before starting local control.
>
> 💡 Security mode (SEC0/SEC1) can be configured via `idf.py menuconfig` → ESP Weaver.

### Gesture Report Log Example

When gesture events are detected, they will be reported via Local Control. The serial output will show logs like:

```
I (224603) app_driver: Heavy shake on: 
I (224603) app_driver: Y axis
I (224603) app_imu_gesture: Gesture event reported: shake (confidence: 0%)
I (224603) app_imu_gesture:   Orientation: normal

I (227603) app_driver: Slight shake on: 
I (227603) app_driver: X axis
I (227603) app_imu_gesture: Gesture event reported: shake (confidence: 0%)
I (227603) app_imu_gesture:   Orientation: normal

I (230703) app_driver: Slight shake on: 
I (230703) app_driver: Z axis
I (230703) app_imu_gesture: Gesture event reported: shake (confidence: 0%)
I (230703) app_imu_gesture:   Orientation: normal
```

## Note

This example only demonstrates the detection and reporting of the shake gesture. To recognize additional gestures, you need to implement the corresponding gesture detection algorithms.

## Troubleshooting

* **Wi-Fi Connection Failed**: Check Wi-Fi credentials in `menuconfig` and ensure the device is within Wi-Fi range
* **Device Not Discovered**: Ensure Home Assistant and the ESP device are on the same local network, and that the [ESP-Weaver](https://github.com/espressif/esp_weaver) integration is installed
* **PoP Code Rejected**: Verify the PoP code matches the one displayed in the serial log

For any technical queries, please open an [issue](https://github.com/espressif/esp-iot-solution/issues) on GitHub.

## Technical References

* [ESP Weaver Component](../../../components/esp_weaver/README.md)
* [ESP-Weaver Home Assistant Integration](https://github.com/espressif/esp_weaver)
* [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/index.html)
* [esp_local_ctrl Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/protocols/esp_local_ctrl.html)
