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

1. BLE 周期广播示例: :example:`bluetooth/ble_conn_mgr/ble_periodic_adv`.
2. BLE 周期同步示例: :example:`bluetooth/ble_conn_mgr/ble_periodic_sync`.
3. BLE 扩展广播示例: :example:`bluetooth/ble_conn_mgr/ble_ext_adv`.
4. BLE 扩展被动扫描示例: :example:`bluetooth/ble_conn_mgr/ble_ext_scan`.
5. BLE 串口配置文件示例: :example:`bluetooth/ble_conn_mgr/ble_spp`.
6. BLE L2CAP CoC 中心设备示例: :example:`bluetooth/ble_l2cap_coc/l2cap_coc_central`.
7. BLE L2CAP CoC 外围设备示例: :example:`bluetooth/ble_l2cap_coc/l2cap_coc_peripheral`.

扩展广播演示对（``ble_ext_adv`` / ``ble_ext_scan``）
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

这两个工程演示 **BLE 5 扩展广播**：**不可连接、不可扫描**的辅信道链路，主信道 **1M**、辅信道 **2M**，**Advertising SID = 2**，以及固定的 **1000 字节** AD 负载。一块板烧录 ``ble_ext_adv``，另一块烧录 ``ble_ext_scan``；对照两边串口日志中的 **FNV-1a** 校验值，可确认扫描端重组的数据与广播端一致。

该演示对 **不涉及** 周期广播；若需周期广播与周期同步，请使用 ``ble_periodic_adv`` 与 ``ble_periodic_sync``。

API 参考
-----------------

.. include-build-file:: inc/esp_ble_conn_mgr.inc