IOT ETH 以太网
===============

:link_to_translation:`en:[English]`

`iot_eth` 提供了一整套 4G 模组连接以太网的解决方案，提供底层硬件的抽象层，默认提供基于 LWIP 的网络接口。也支持用户自定义网络接口。

特性
-----

- 高级工厂模式，支持底层多种连接方式
- 默认支持 LWIP 网络接口, 也支持用户自定义网络接口

用户指南
----------

1. 先初始化 `driver` 层，提供下层硬件抽象

.. code:: c

    iot_usbh_rndis_config_t rndis_cfg = {
        .auto_detect = true,  // auto detect 4G module
        .auto_detect_timeout = pdMS_TO_TICKS(1000),  // auto detect timeout
    };

    iot_eth_driver_t *rndis_handle = NULL;
    esp_err_t ret = iot_eth_new_usb_rndis(&rndis_cfg, &rndis_handle);
    if (ret != ESP_OK || rndis_handle == NULL) {
        ESP_LOGE(TAG, "Failed to create USB RNDIS driver");
        return;
    }

2. 初始化 `iot_eth` 层

.. code:: c

    iot_eth_config_t eth_cfg = {
        .driver = rndis_handle,
        .stack_input = NULL,
        .user_data = NULL,
    };

    iot_eth_handle_t eth_handle = NULL;
    ret = iot_eth_install(&eth_cfg, &eth_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install USB RNDIS driver");
        return;
    }

3. 启动 `iot_eth` 层

.. code:: c

    ret = iot_eth_start(eth_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start USB RNDIS driver");
        return;
    }

使用其他网络接口
-----------------

1. 发送： 请调用 `iot_eth_transmit` 发送数据。

2. 接受： 请配置 `iot_eth_config_t` 中的 `stack_input` 回调函数，当有数据接收时，会调用该回调函数。

3. 获取 4G 模块的 MAC 地址： 请调用 `iot_eth_get_addr` 获取 4G 模块的 MAC 地址。

已支持的网络接口
------------------

- 基于 USB 接口的 RNDIS :doc:`/usb/usb_host/usb_rndis`

- 基于 USB 接口的 PPP :doc:`/usb/usb_host/usb_ppp`

- 基于 USB 接口的 ECM :doc:`/usb/usb_host/usb_ecm`

API 参考
---------

.. include-build-file:: inc/iot_eth.inc
