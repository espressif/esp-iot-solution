Electronic Shelf Label Profile
==============================
:link_to_translation:`zh_CN:[中文]`

The Electronic Shelf Label Profile (ESLP) implements ESL Tag and Access Point roles on top of ``ble_conn_mgr`` and the ESL Service.

Tag role (``CONFIG_BLE_ESL_PROFILE``):

- ESL Service registration
- States: Unassociated, Configuring, Associated, Updating, Synchronized
- Association characteristic writes and lifecycle ECP commands
- Optional PAST / PAwR synchronized operation

AP role (``CONFIG_BLE_ESL_PROFILE_AP``):

- GATT association helper for connected Tags
- PAwR advertising control, PAST sync transfer, and subevent command queue

To use the Tag profile:

#. Enable ``CONFIG_BLE_ESL_PROFILE`` in menuconfig.
#. Call ``esp_ble_conn_init()``, then ``esp_ble_eslp_init()``, then ``esp_ble_conn_start()``.

To use the AP profile:

#. Enable ``CONFIG_BLE_ESL_PROFILE_AP`` and the required ``ble_conn_mgr`` options.
#. Call ``esp_ble_conn_init()``, then ``esp_ble_eslp_ap_init()``, then ``esp_ble_conn_start()``.
#. After discovery, call ``esp_ble_eslp_ap_configure_tag()``.

Examples
--------------

:example:`bluetooth/ble_profiles/ble_esl/ble_esl_tag`.

:example:`bluetooth/ble_profiles/ble_esl/ble_esl_ap`.

API Reference
-----------------

.. include-build-file:: inc/esp_eslp_tag.inc

.. include-build-file:: inc/esp_eslp_ap.inc
