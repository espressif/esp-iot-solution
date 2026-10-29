Immediate Alert Service
==============================
:link_to_translation:`zh_CN:[中文]`

The Immediate Alert Service (IAS) is a Bluetooth SIG GATT service that exposes the **Alert Level** characteristic so a connected peer can request an immediate alert on the device (for example, sound or vibration when finding a peripheral).

The Alert Level is a single octet: **no alert**, **mild alert**, or **high alert** (``ESP_BLE_IAS_ALERT_LEVEL_NO_ALERT``, ``ESP_BLE_IAS_ALERT_LEVEL_MILD_ALERT``, ``ESP_BLE_IAS_ALERT_LEVEL_HIGH_ALERT``, i.e. ``0x00``, ``0x01``, and ``0x02``).

To use this service in an application:

#. Enable ``CONFIG_BLE_IAS`` in menuconfig.
#. Initialize the BLE connection manager with ``esp_ble_conn_init()`` (and related setup such as the default event loop if your project requires it).
#. After the connection manager has been initialized, call ``esp_ble_ias_init()`` to register the IAS on the GATT server. Do this before ``esp_ble_conn_start()`` in the usual example flow.
#. Call ``esp_ble_ias_register_alert_cb()`` **after** ``esp_ble_ias_init()`` so the service exists before you install the handler that runs on valid Alert Level writes.

There is no dedicated example project for this service; follow the initialization pattern used in other :doc:`BLE service <ble_services>` examples.

API Reference
-----------------

.. include-build-file:: inc/esp_ias.inc
