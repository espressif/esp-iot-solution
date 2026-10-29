Electronic Shelf Label Service
==============================
:link_to_translation:`zh_CN:[中文]`

The Electronic Shelf Label Service (ESL) is a Bluetooth SIG GATT service (UUID ``0x1857``) used by ESL Tags. It exposes configuration characteristics (address, key material, absolute time), optional capability information (display / image / sensor / LED), and the ESL Control Point for command and response exchange over ACL.

To use this service in an application:

#. Enable ``CONFIG_BLE_ESL`` in menuconfig (optional characteristics are controlled by related Kconfig options).
#. Initialize the BLE connection manager with ``esp_ble_conn_init()``.
#. Call ``esp_ble_esl_init()`` to register the ESL service before ``esp_ble_conn_start()``.
#. Optionally set local capability values with ``esp_ble_esl_set_display_info()``, ``esp_ble_esl_set_image_info()``, and related setters.
#. Subscribe to ``BLE_ESL_EVENTS`` (event id is the written characteristic UUID) to handle writes such as address, keys, absolute time, and ECP commands.
#. Send ECP responses with ``esp_ble_esl_notify_ecp_response()``.

There is no dedicated example project for this service yet; follow the initialization pattern used in other :doc:`BLE service <ble_services>` examples.

API Reference
-----------------

.. include-build-file:: inc/esp_esl.inc
