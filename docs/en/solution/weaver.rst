ESP Weaver
============
:link_to_translation:`zh_CN:[中文]`

==================  ===========  ===============  ===============  ===============  ===============  ================  =============
Supported chips     ESP32        ESP32-C2         ESP32-C3         ESP32-C5         ESP32-C6         ESP32-C61         ESP32-S3
==================  ===========  ===============  ===============  ===============  ===============  ================  =============

ESP Weaver is a device-side SDK for building smart home devices with Home Assistant local discovery and control support. Based on ESP-IDF's esp_local_ctrl component and mDNS service discovery, it enables millisecond-level local network communication without relying on cloud services.

The SDK provides a simple device/parameter model, supporting device creation, parameter management, write callbacks, and real-time parameter push to connected clients. It supports Security1 protocol (Curve25519 + AES-CTR) with Proof of Possession (PoP) for secure communication.

Features
--------------

- Low-latency local control via mDNS + esp_local_ctrl
- Offline capable, no internet connection required
- End-to-end encryption with Proof of Possession (PoP)
- Zero-configuration device discovery
- Simple device/parameter model for smart home integration
- Compatible with `ESP-Weaver <https://github.com/espressif/esp_weaver>`_ Home Assistant custom integration

Architecture
--------------

.. code-block:: text

    +-------------------+    mDNS Discovery    +-------------------+
    |                   | <------------------- |                   |
    |  Home Assistant   |                      |    ESP Device     |
    |  (ESP-Weaver)     | -------------------> |   (ESP Weaver)    |
    |                   |    Local Control     |                   |
    +-------------------+                      +-------------------+
             |                                          |
             |               Local Network              |
             +------------------------------------------+

1. ESP device connects to WiFi and broadcasts service via mDNS
2. ESP-Weaver integration in Home Assistant discovers the device
3. User enters the device's PoP code to complete pairing
4. Bidirectional communication through Local Control protocol

Examples
--------------

1. LED Light Example: :example:`weaver/led_light`. RGB LED light control with power, brightness, HSV color, and color temperature support.

API Reference
-----------------

.. include-build-file:: inc/esp_weaver.inc

