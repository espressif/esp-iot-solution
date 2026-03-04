# ESP Weaver

[![ESP-IDF Version](https://img.shields.io/badge/ESP--IDF-v5.5-blue)](https://github.com/espressif/esp-idf)
[![License: Apache-2.0](https://img.shields.io/badge/License-Apache%202.0-yellow.svg)](https://opensource.org/licenses/Apache-2.0)

**English** | [中文](README_CN.md)

ESP Weaver is a device-side SDK for building smart home devices with Home Assistant local discovery and control support. Built on [ESP-IDF](https://github.com/espressif/esp-idf), it provides a simple device/parameter model and leverages esp_local_ctrl + mDNS for local network communication, without relying on cloud services.

> **Note:** ESP Weaver is **not** a drop-in replacement for ESP RainMaker. While it uses a compatible local control protocol and a similar device/parameter model, the API namespace (`esp_weaver_*`), types, and feature set are different. Cloud features (MQTT, OTA, schedules, scenes) are intentionally excluded. Only a single weaver node may exist per process (singleton pattern).

[ESP-Weaver](https://github.com/espressif/esp_weaver) is the corresponding Home Assistant custom integration developed by Espressif, which discovers and controls ESP devices through the esp_local_ctrl protocol.

## 📋 Architecture & How It Works

```
┌─────────────────┐    mDNS Discovery    ┌─────────────────┐
│                 │ ◄─────────────────── │                 │
│  Home Assistant │                      │   ESP Device    │
│  (ESP-Weaver)   │ ───────────────────► │  (ESP Weaver)   │
│                 │    Local Control     │                 │
└─────────────────┘                      └─────────────────┘
         │                                        │
         │             Local Network              │
         └────────────────────────────────────────┘
```

1. ESP device connects to WiFi and broadcasts service via mDNS
2. ESP-Weaver integration in Home Assistant discovers the device
3. User enters the device's PoP (Proof of Possession) code to complete pairing
4. Bidirectional communication through Local Control protocol

## 🌟 Features

- **Low-Latency Local Control**: Millisecond-level response within LAN based on mDNS service discovery and esp_local_ctrl protocol
- **Offline Capable**: Direct communication between devices and Home Assistant within LAN, fully functional without internet connection
- **End-to-End Encryption**: Device authentication via Proof of Possession (PoP), Security1 protocol with Curve25519 key exchange and AES-CTR encryption
- **Zero-Configuration Discovery**: Devices automatically broadcast services via mDNS after connecting to the network, enabling Home Assistant discovery without manual configuration
- **Simple Device Model**: Intuitive device/parameter model for smart home integration with real-time parameter updates

## 📦 Installation

### Using ESP Component Registry (Recommended)

```bash
idf.py add-dependency "espressif/esp_weaver=*"
```

### Manual Installation

Clone the repository and add the component to your project:

```bash
git clone https://github.com/espressif/esp-iot-solution.git
cp -r esp-iot-solution/components/esp_weaver your_project/components/
```

## 🚀 Quick Start

```c
#include <esp_weaver.h>
#include <esp_weaver_standard_types.h>
#include <esp_weaver_standard_params.h>

void app_main(void)
{
    // Initialize weaver node (singleton)
    esp_weaver_config_t config = ESP_WEAVER_CONFIG_DEFAULT();
    esp_weaver_node_t *node = esp_weaver_node_init(&config, "My Device", "Lightbulb");

    // Create a lightbulb device
    esp_weaver_device_t *dev = esp_weaver_device_create("Light", ESP_WEAVER_DEVICE_LIGHTBULB, NULL);
    esp_weaver_device_add_bulk_cb(dev, my_write_cb);

    // Add parameters
    esp_weaver_param_t *power = esp_weaver_param_create(ESP_WEAVER_DEF_POWER_NAME, ESP_WEAVER_PARAM_POWER,
                                    esp_weaver_bool(true), PROP_FLAG_READ | PROP_FLAG_WRITE | PROP_FLAG_PERSIST);
    esp_weaver_device_add_param(dev, power);
    esp_weaver_device_assign_primary_param(dev, power);

    esp_weaver_param_t *brightness = esp_weaver_param_create(ESP_WEAVER_DEF_BRIGHTNESS_NAME, ESP_WEAVER_PARAM_BRIGHTNESS,
                                         esp_weaver_int(50), PROP_FLAG_READ | PROP_FLAG_WRITE | PROP_FLAG_PERSIST);
    esp_weaver_param_add_bounds(brightness, esp_weaver_int(0), esp_weaver_int(100), esp_weaver_int(1));
    esp_weaver_device_add_param(dev, brightness);

    esp_weaver_node_add_device(node, dev);

    // Start local control (all settings from Kconfig)
    esp_weaver_local_ctrl_start();
}
```

## 🔧 Core API

### Initialization

```c
// Initialize the weaver node (returns node handle, NULL on failure)
esp_weaver_node_t *esp_weaver_node_init(const esp_weaver_config_t *config, const char *name, const char *type);

// Deinitialize and free resources
esp_err_t esp_weaver_node_deinit(const esp_weaver_node_t *node);

// Get the node ID
const char *esp_weaver_get_node_id(void);
```

### Device Management

```c
// Create a new device
esp_weaver_device_t *esp_weaver_device_create(const char *dev_name, const char *type, void *priv_data);

// Delete a device (must be removed from node first)
esp_err_t esp_weaver_device_delete(esp_weaver_device_t *device);

// Add parameter to device
esp_err_t esp_weaver_device_add_param(esp_weaver_device_t *device, esp_weaver_param_t *param);

// Add bulk write callback (handles multiple parameters at once)
esp_err_t esp_weaver_device_add_bulk_cb(esp_weaver_device_t *device, esp_weaver_device_bulk_write_cb_t write_cb);

// Assign a primary parameter for the device
esp_err_t esp_weaver_device_assign_primary_param(esp_weaver_device_t *device, esp_weaver_param_t *param);

// Add device to node / remove device from node
esp_err_t esp_weaver_node_add_device(const esp_weaver_node_t *node, esp_weaver_device_t *device);
esp_err_t esp_weaver_node_remove_device(const esp_weaver_node_t *node, esp_weaver_device_t *device);

// Get device name / private data
const char *esp_weaver_device_get_name(const esp_weaver_device_t *device);
void *esp_weaver_device_get_priv_data(const esp_weaver_device_t *device);

// Get parameter by name or type
esp_weaver_param_t *esp_weaver_device_get_param_by_name(const esp_weaver_device_t *device, const char *param_name);
esp_weaver_param_t *esp_weaver_device_get_param_by_type(const esp_weaver_device_t *device, const char *param_type);
```

### Parameter Management

```c
// Create a new parameter
esp_weaver_param_t *esp_weaver_param_create(const char *param_name, const char *type, esp_weaver_param_val_t val, uint8_t properties);

// Delete a parameter (only if not added to a device)
esp_err_t esp_weaver_param_delete(esp_weaver_param_t *param);

// Add UI type for parameter
esp_err_t esp_weaver_param_add_ui_type(esp_weaver_param_t *param, const char *ui_type);

// Add value bounds
esp_err_t esp_weaver_param_add_bounds(esp_weaver_param_t *param, esp_weaver_param_val_t min, esp_weaver_param_val_t max, esp_weaver_param_val_t step);

// Get parameter name / value
const char *esp_weaver_param_get_name(const esp_weaver_param_t *param);
const esp_weaver_param_val_t *esp_weaver_param_get_val(const esp_weaver_param_t *param);

// Update parameter value (marks for reporting)
esp_err_t esp_weaver_param_update(esp_weaver_param_t *param, esp_weaver_param_val_t val);

// Update and push to clients
esp_err_t esp_weaver_param_update_and_report(esp_weaver_param_t *param, esp_weaver_param_val_t val);
```

### Local Control

```c
// Start local control (all settings from Kconfig)
esp_err_t esp_weaver_local_ctrl_start(void);

// Stop local control service
esp_err_t esp_weaver_local_ctrl_stop(void);

// Set custom Proof of Possession (optional, call before start)
esp_err_t esp_weaver_local_ctrl_set_pop(const char *pop);

// Get current Proof of Possession
const char *esp_weaver_local_ctrl_get_pop(void);

// Set a custom mDNS TXT record (call after start)
esp_err_t esp_weaver_local_ctrl_set_txt(const char *key, const char *value);

// Push parameter changes to connected clients
esp_err_t esp_weaver_local_ctrl_send_params(void);
```

## 📊 Standard Types

### Device Types

- `ESP_WEAVER_DEVICE_LIGHTBULB` - Smart light bulb

### Parameter Types

- `ESP_WEAVER_PARAM_NAME` - Device name
- `ESP_WEAVER_PARAM_POWER` - Power on/off
- `ESP_WEAVER_PARAM_BRIGHTNESS` - Brightness level
- `ESP_WEAVER_PARAM_HUE` - Color hue
- `ESP_WEAVER_PARAM_SATURATION` - Color saturation
- `ESP_WEAVER_PARAM_CCT` - Correlated Color Temperature

### UI Types

- `ESP_WEAVER_UI_TOGGLE` - Toggle switch
- `ESP_WEAVER_UI_SLIDER` - Slider control
- `ESP_WEAVER_UI_HUE_SLIDER` - Hue color slider
- `ESP_WEAVER_UI_TEXT` - Text display

### Value Helpers

```c
esp_weaver_bool(x)    // Create boolean value
esp_weaver_int(x)     // Create integer value
esp_weaver_float(x)   // Create float value
esp_weaver_str(x)     // Create string value
esp_weaver_obj(x)     // Create JSON object value
esp_weaver_array(x)   // Create JSON array value
```

## 🔒 Security & PoP

The component supports two security modes:

- **SEC0**: No security (for development/testing)
- **SEC1**: Curve25519 key exchange + AES-CTR encryption, with Proof of Possession (PoP)

After the device starts, the PoP code will be displayed in the serial log:

```
I (10592) esp_weaver_local_ctrl: No PoP in NVS. Generating a new one.
I (10592) esp_weaver_local_ctrl: PoP for local control: 8f820513
```

The PoP is auto-generated randomly and persisted to NVS. On subsequent reboots, the same PoP will be reused. You can also set a custom PoP programmatically via `esp_weaver_local_ctrl_set_pop()` before starting local control.

This PoP code is required when adding the device in the ESP-Weaver integration in Home Assistant.

### Configuration

Security can be configured via `idf.py menuconfig` → ESP Weaver:

- **Local control HTTP port**: Default 8080
- **Security version**: SEC0 (no security) or SEC1 (with PoP)

## 🏠 Home Assistant Configuration

1. Install [ESP-Weaver](https://github.com/espressif/esp_weaver) integration in Home Assistant
2. Devices will be automatically discovered after connecting to WiFi
3. Enter the device's PoP code to complete pairing
4. Device entities will appear in Home Assistant

## 📊 Examples

See the [examples/weaver](../../examples/weaver) directory for complete examples:

- [led_light](../../examples/weaver/led_light) - RGB LED light control

## 🧪 Testing

The component includes a comprehensive test suite in `test_apps/` with 18 test cases covering:

- Initialization / Deinitialization API
- Device API (create, delete, add params, callbacks)
- Parameter API (create, delete, set UI, set bounds, update)
- Integration tests (full workflow)
- Memory leak tests

```bash
cd components/esp_weaver/test_apps
idf.py set-target esp32c6
idf.py build flash monitor
```

See [test_apps/README.md](test_apps/README.md) for details.

## 📋 IDF Version Compatibility

| ESP-IDF Version | Status |
|:--------------- |:------:|
| v5.5 | ✅ Supported |

## 🔧 Hardware

The examples in this repository have been validated on the following official Espressif development boards:

| Chip | Development Board | Module | Onboard LED |
|------|-------------------|--------|-------------|
| ESP32 | [ESP32_DevKitc_V4](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32/esp32-devkitc/user_guide.html) | ESP32-WROVER-E | No (external required) |
| ESP32-C2 | [ESP8684-DevKitM-1 V1.1](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32c2/esp8684-devkitm-1/user_guide.html) | ESP8684-MINI-1 V1.0 | Yes |
| ESP32-C3 | [ESP32-C3-DevKitM-1 V1.0](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32c3/esp32-c3-devkitm-1/index.html) | ESP32-C3-MINI-1 V1.1 | Yes |
| ESP32-C5 | [ESP32-C5-DevKitC-1 V1.2](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32c5/esp32-c5-devkitc-1/user_guide.html) | ESP32-C5-WROOM-1 V1.3 | Yes |
| ESP32-C6 | [ESP32-C6-DevKitC-1 V1.2](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32c6/esp32-c6-devkitc-1/user_guide.html) | ESP32-C6-WROOM-1 | Yes |
| ESP32-C61 | [ESP32-C61-DevKitC-1 V1.0](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32c61/esp32-c61-devkitc-1/user_guide_v1.0.html) | ESP32-C61-WROOM-1 V1.1 | Yes |
| ESP32-S3 | [ESP32-S3-DevKitC-1 v1.1](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s3/esp32-s3-devkitc-1/user_guide_v1.1.html) | ESP32-S3-WROOM-1 V1.3 | Yes |

## 🤝 Contributing

Contributions are welcome! Please feel free to submit a Pull Request.

## ❓ Q&A

**Q1: How does ESP Weaver differ from ESP-RainMaker?**

**A1:** ESP Weaver uses a local control protocol compatible with ESP-RainMaker, but it is **not** a drop-in replacement. All cloud-related functionality (MQTT, OTA, schedules, scenes, claiming) is removed. The API namespace is `esp_weaver_*` (not `esp_rmaker_*`), and some features like per-parameter read callbacks are not included. ESP Weaver focuses exclusively on Home Assistant local discovery and control via esp_local_ctrl + mDNS.

**Q2: Do I need an internet connection?**

**A2:** No. ESP Weaver operates entirely within the local network. Both the ESP device and Home Assistant only need to be on the same LAN.

**Q3: How do I change the PoP code?**

**A3:** You can set a custom PoP programmatically via `esp_weaver_local_ctrl_set_pop()` before starting local control. If no custom PoP is set, a random one is auto-generated and persisted to NVS on first boot.

**Q4: Which Home Assistant integration should I use?**

**A4:** Install the [ESP-Weaver](https://github.com/espressif/esp_weaver) custom integration. It will automatically discover ESP Weaver devices on the local network via mDNS.

## 🔗 Related Links

- [ESP-Weaver](https://github.com/espressif/esp_weaver) - Home Assistant custom integration
- [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/index.html)
- [esp_local_ctrl Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/protocols/esp_local_ctrl.html)
- [Home Assistant Official Documentation](https://www.home-assistant.io/docs/)
- [ESP-IoT-Solution](https://github.com/espressif/esp-iot-solution)

## 📄 License

This project is licensed under the Apache License 2.0 - see the [LICENSE](license.txt) file for details.
