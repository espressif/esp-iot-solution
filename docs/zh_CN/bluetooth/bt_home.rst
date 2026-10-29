BTHome 组件
==============================
:link_to_translation:`en:[English]`

BTHome 组件用于实现 BTHome V2 协议，支持传感器数据上报、二进制传感器数据上报、事件数据上报等功能。
该组件支持加密和非加密模式，可以与 Home Assistant 等智能家居平台无缝集成。

BTHome 使用方法
-----------------
1. 初始化 BTHome：

   - 使用 :cpp:func:`bthome_create` 创建 BTHome 实例
   - 使用 :cpp:func:`bthome_register_callbacks` 注册回调函数
   - 使用 :cpp:func:`bthome_set_encrypt_key` 设置加密密钥（可选）
   - 使用 :cpp:func:`bthome_set_peer_mac_addr` 设置对端 MAC 地址

2. 配置存储：

   - 使用 :cpp:func:`settings_store` 存储配置
   - 使用 :cpp:func:`settings_load` 加载配置

3. 解析广播数据：

   - 使用 :cpp:func:`bthome_parse_adv_data` 解析广播数据
   - 使用 :cpp:func:`bthome_free_reports` 释放报告数据

示例代码
-----------------
.. code-block:: c

    // 创建 BTHome 实例
    bthome_handle_t bthome_recv;
    ESP_ERROR_CHECK(bthome_create(&bthome_recv));

    // 注册回调函数
    bthome_callbacks_t callbacks = {
        .store = settings_store,
        .load = settings_load,
    };
    ESP_ERROR_CHECK(bthome_register_callbacks(bthome_recv, &callbacks));

    // 设置加密密钥
    static const uint8_t encrypt_key[] = {0x23, 0x1d, 0x39, 0xc1, 0xd7, 0xcc, 0x1a, 0xb1, 0xae, 0xe2, 0x24, 0xcd, 0x09, 0x6d, 0xb9, 0x32};
    ESP_ERROR_CHECK(bthome_set_encrypt_key(bthome_recv, encrypt_key));

    // 设置对端 MAC 地址
    static const uint8_t peer_mac[] = {0x54, 0x48, 0xE6, 0x8F, 0x80, 0xA5};
    ESP_ERROR_CHECK(bthome_set_peer_mac_addr(bthome_recv, peer_mac));

    // 解析广播数据
    bthome_reports_t *reports = bthome_parse_adv_data(bthome_recv, result.ble_adv, result.adv_data_len);
    if (reports != NULL) {
        // 处理报告数据
        for (int i = 0; i < reports->num_reports; i++) {
            // 处理每个报告
        }
        bthome_free_reports(reports);
    }

API 参考
---------------------------------------------
.. include-build-file:: inc/bthome_v2.inc 