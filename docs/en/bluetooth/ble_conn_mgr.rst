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
3. BLE serial port profile example: :example:`bluetooth/ble_conn_mgr/ble_spp`.

API Reference
-----------------

.. include-build-file:: inc/esp_ble_conn_mgr.inc