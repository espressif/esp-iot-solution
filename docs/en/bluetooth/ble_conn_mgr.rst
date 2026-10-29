BLE Connection Management
==============================
:link_to_translation:`zh_CN:[中文]`

==================  ===========  ===============  =============== =============
 Supported Socs       ESP32        ESP32-C2          ESP32-C3        ESP32-S3    
==================  ===========  ===============  =============== =============

The BLE connection management provides a simplified API interface for accessing the commonly used BLE functionality. It supports common scenarios like peripheral, central among others. 

Application Example
---------------------------

 .. code::

    esp_ble_conn_config_t config = {
        .device_name = CONFIG_EXAMPLE_BLE_ADV_NAME,
        .broadcast_data = CONFIG_EXAMPLE_BLE_SUB_ADV
    };

    ESP_ERROR_CHECK(esp_ble_conn_init(&config));
    ESP_ERROR_CHECK(esp_ble_conn_start());

Examples
--------------

1. BLE periodic advertiser example: :example:`bluetooth/ble_conn_mgr/ble_periodic_adv`.
2. BLE periodic sync example: :example:`bluetooth/ble_conn_mgr/ble_periodic_sync`.
3. BLE extended advertising example: :example:`bluetooth/ble_conn_mgr/ble_ext_adv`.
4. BLE extended passive scan example: :example:`bluetooth/ble_conn_mgr/ble_ext_scan`.
5. BLE serial port profile example: :example:`bluetooth/ble_conn_mgr/ble_spp`.
6. BLE L2CAP CoC Central example: :example:`bluetooth/ble_l2cap_coc/l2cap_coc_central`.
7. BLE L2CAP CoC Peripheral example: :example:`bluetooth/ble_l2cap_coc/l2cap_coc_peripheral`.

Extended advertising demo pair (``ble_ext_adv`` / ``ble_ext_scan``)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

These two projects demonstrate **BLE 5 extended advertising** with a **non-connectable, non-scannable** auxiliary chain (primary **1M**, secondary **2M**), **Advertising SID 2**, and a fixed **1000-byte** AD payload. Flash ``ble_ext_adv`` on one board and ``ble_ext_scan`` on another; compare the **FNV-1a** hash printed on both serial consoles to confirm the scanner reassembled the same octets as the advertiser.

This pair does **not** use periodic advertising. For periodic train + sync, use ``ble_periodic_adv`` and ``ble_periodic_sync`` instead.

API Reference
-----------------

.. include-build-file:: inc/esp_ble_conn_mgr.inc