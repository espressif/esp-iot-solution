BLE 连接管理
==============================
:link_to_translation:`en:[English]`

==================  ===========  ===============  =============== =============
支持的芯片            ESP32        ESP32-C2          ESP32-C3        ESP32-S3    
==================  ===========  ===============  =============== =============

BLE 连接管理为访问常用 BLE 功能提供了简化的 API 接口。它支持外围、中心等常见场景。 

应用示例
---------------------------

 .. code::

    esp_ble_conn_config_t config = {
        .device_name = CONFIG_EXAMPLE_BLE_ADV_NAME,
        .broadcast_data = CONFIG_EXAMPLE_BLE_SUB_ADV
    };

    ESP_ERROR_CHECK(esp_ble_conn_init(&config));
    ESP_ERROR_CHECK(esp_ble_conn_start());

示例
--------------

1. BLE 周期广告示例: :example:`bluetooth/ble_conn_mgr/ble_periodic_adv`.
2. BLE 周期同步示例: :example:`bluetooth/ble_conn_mgr/ble_periodic_sync`.
3. BLE 串口配置文件示例: :example:`bluetooth/ble_conn_mgr/ble_spp`.

API 参考
-----------------

.. include-build-file:: inc/esp_ble_conn_mgr.inc