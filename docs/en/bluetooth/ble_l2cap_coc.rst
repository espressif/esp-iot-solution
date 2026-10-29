BLE L2CAP Connection Oriented Channels (CoC)
=============================================
:link_to_translation:`zh_CN:[中文]`

==================  ===========  ===============  =============== =============
 Supported Socs       ESP32        ESP32-C2          ESP32-C3        ESP32-S3
==================  ===========  ===============  =============== =============

L2CAP Connection Oriented Channels (CoC) is a BLE feature that enables reliable,
stream-oriented data transfer between BLE devices. Unlike GATT, which is based
on attribute protocol, L2CAP CoC provides a connection-oriented channel model
similar to TCP, allowing for larger data transfers with flow control.

Key features of L2CAP CoC:

- **Connection-oriented**: Establishes dedicated channels between devices
- **Flow control**: Prevents data loss by controlling transmission rate
- **Larger MTU**: Supports MTU sizes up to 512 bytes (configurable)
- **Bidirectional**: Full-duplex communication on established channels
- **Multiple channels**: Support for multiple concurrent CoC channels

L2CAP CoC is particularly useful for applications that require:

- Large file transfers
- Audio/video streaming
- Bulk data synchronization
- Any application needing reliable, stream-oriented communication

Application Example
---------------------------

The PSM (Protocol/Service Multiplexer) value can be configured via Kconfig. In the example projects,
it can be set in ``Example Configuration → L2CAP CoC PSM`` menu. Valid range: 0x0001-0x00FF (1-255).

- **0x0001-0x007F**: Fixed Bluetooth SIG-defined services
- **0x0080-0x00FF**: Dynamic custom services

Both central and peripheral devices must use the same PSM value to establish a connection.

 .. code-block:: c

    // PSM value is configured via Kconfig (CONFIG_EXAMPLE_L2CAP_COC_PSM)
    // Default value: 0x00EF (239)
    #define L2CAP_COC_PSM CONFIG_EXAMPLE_L2CAP_COC_PSM
    #define L2CAP_COC_MTU CONFIG_BLE_CONN_MGR_L2CAP_COC_MTU  // Maximum Transmission Unit

    // Initialize L2CAP CoC memory pool
    ESP_ERROR_CHECK(esp_ble_conn_l2cap_coc_mem_init());

    // Create L2CAP CoC server
    // app_ble_conn_l2cap_coc_event_handler is your callback function
    esp_err_t rc = esp_ble_conn_l2cap_coc_create_server(
        L2CAP_COC_PSM, L2CAP_COC_MTU,
        app_ble_conn_l2cap_coc_event_handler, NULL);

Examples
--------------

1. BLE L2CAP CoC Central example: :example:`bluetooth/ble_l2cap_coc/l2cap_coc_central`.
2. BLE L2CAP CoC Peripheral example: :example:`bluetooth/ble_l2cap_coc/l2cap_coc_peripheral`.

API Reference
-----------------

See :doc:`BLE Connection Management <ble_conn_mgr>` for the L2CAP CoC API reference.
