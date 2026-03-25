| Supported Targets | ESP32 | ESP32-C2 | ESP32-C3 | ESP32-C5 | ESP32-C6 | ESP32-C61 | ESP32-S3 |
| ----------------- | ----- | -------- | -------- | -------- | --------- | -------- | -------- |

# LED Light Example

[中文](README_CN.md)

This example demonstrates how to build a smart light device with ESP Weaver local control protocol support, enabling local network discovery and control via Home Assistant.

## Features

* Power on/off control
* Brightness adjustment (0-100%)
* HSV (Hue, Saturation, Value) color control (Hue: 0-360°, Saturation: 0-100%)

## Hardware

* A development board with ESP32/ESP32-C2/ESP32-C3/ESP32-C5/ESP32-C6/ESP32-C61/ESP32-S3 SoC
* RGB LED (WS2812 or 3-pin RGB module)
* USB cable for power supply and programming
* Wi-Fi router for network connectivity
* Button uses the BOOT button on the development board

> 💡 LED type and GPIO configuration can be changed via `idf.py menuconfig` → Example Configuration.

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
I (10592) esp_weaver_local_ctrl: No PoP in NVS. Generating a new one.
I (10592) esp_weaver_local_ctrl: PoP for local control: 8f820513
```

The PoP is auto-generated randomly and persisted to NVS. On subsequent reboots, `PoP read from NVS` will be shown and the PoP value remains the same. This PoP code is required when adding the device in the ESP-Weaver integration in Home Assistant.

### Add Device to Home Assistant

1. Install [ESP-Weaver](https://github.com/espressif/esp_weaver) integration in Home Assistant
2. The device will be automatically discovered after connecting to WiFi
3. Enter the device's PoP code to complete pairing
4. Device entities (power, brightness, color) will appear in Home Assistant

## Example Output

When the program starts, you will see the following log:

```
I (10502) esp_netif_handlers: example_netif_sta ip: 192.168.30.11, mask: 255.255.255.0, gw: 192.168.30.1
I (10502) example_connect: Got IPv4 event: Interface "example_netif_sta" address: 192.168.30.11
I (10532) example_connect: Got IPv6 event: Interface "example_netif_sta" address: fe80:0000:0000:0000:6255:f9ff:fef9:19ec, type: ESP_IP6_ADDR_IS_LINK_LOCAL
I (10532) example_common: Connected to example_netif_sta
I (10532) example_common: - IPv4 address: 192.168.30.11,
I (10542) example_common: - IPv6 address: fe80:0000:0000:0000:6255:f9ff:fef9:19ec, type: ESP_IP6_ADDR_IS_LINK_LOCAL
I (10552) esp_weaver: Weaver initialized: ESP Weaver Device (Lightbulb), node_id: 6055F9F919EC
I (10562) app_main: Node ID: 6055F9F919EC
I (10562) esp_weaver: Device created: Light (esp.device.lightbulb)
I (10572) esp_weaver: Device added to node: Light
I (10572) esp_weaver_local_ctrl: Starting local control with HTTP transport and security version: 1
I (10582) mdns_mem: mDNS task will be created from internal RAM
I (10592) esp_weaver_local_ctrl: No PoP in NVS. Generating a new one.
I (10592) esp_weaver_local_ctrl: PoP for local control: 8f820513
I (10602) esp_weaver_local_ctrl: Local control started on port 8080, node_id: 6055F9F919EC
I (10612) app_main: Local control started successfully
```

> 💡 **Node ID**: Auto-generated from the MAC address by default. Can also be customized via `esp_weaver_config_t.node_id`.
>
> 💡 **PoP (Proof of Possession)**: In SEC1 mode, a random PoP is auto-generated and persisted to NVS. On first boot you'll see `No PoP in NVS. Generating a new one.`, and on subsequent reboots `PoP read from NVS` with the same value. You can also set a custom PoP by calling `esp_weaver_local_ctrl_set_pop()` before starting local control.
>
> 💡 Security mode (SEC0/SEC1) can be configured via `idf.py menuconfig` → ESP Weaver.

### Control Logs

When controlling the light via Local Control, the device serial output will show logs like:

```
I (37132) app_light: Light received 1 params in write
I (37132) app_light: Light.Power = false

I (38062) app_light: Light received 1 params in write
I (38062) app_light: Light.Power = true

I (40532) app_light: Light received 1 params in write
I (40532) app_light: Light.Brightness = 63

I (42272) app_light: Light received 1 params in write
I (42272) app_light: Light.Hue = 141

I (42342) app_light: Light received 1 params in write
I (42342) app_light: Light.Saturation = 38
```

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
