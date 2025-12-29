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

    static esp_err_t drv_init(iot_eth_driver_t *driver)
    {
        ESP_LOGI(TAG, "network interface init success");
        return ESP_OK;
    }

    static esp_err_t drv_set_mediator(iot_eth_driver_t *driver, iot_eth_mediator_t *mediator)
    {
        net_ctx_t *net = __containerof(driver, net_ctx_t, base);
        net->mediator = mediator;
        ESP_LOGI(TAG, "network interface set mediator success");
        return ESP_OK;
    }

    static esp_err_t usbh_rndis_transmit(iot_eth_driver_t *h, uint8_t *buffer, size_t buflen)
    {
        ESP_LOGI(TAG, "network interface transmit success");
        return ESP_OK;
    }

    static esp_err_t drv_get_addr(iot_eth_driver_t *driver, uint8_t *mac)
    {
        ESP_LOGI(TAG, "network interface get mac success");
        return ESP_OK;
    }

    static esp_err_t drv_deinit(iot_eth_driver_t *driver)
    {
        ESP_LOGI(TAG, "network interface deinit success");
        free(driver);
        return ESP_OK;
    }

    static iot_eth_driver_t * new_rndis_eth_driver()
    {
        net_ctx_t *net = calloc(1, sizeof(net_ctx_t));
        net->base.name = "test drv";
        net->base.init = drv_init;
        net->base.deinit = drv_deinit;
        net->base.set_mediator = drv_set_mediator;
        net->base.get_addr = drv_get_addr;
        net->base.transmit = usbh_rndis_transmit;
        return &net->base;
    }

2. 初始化 `iot_eth` 层

.. code:: c

    iot_eth_config_t eth_cfg = {
        .driver = rndis_handle,
        .stack_input = NULL,
    };

    iot_eth_handle_t eth_handle = NULL;
    ret = iot_eth_install(&eth_cfg, &eth_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install eth driver");
        return;
    }

3. 启动 `iot_eth` 层

.. code:: c

    ret = iot_eth_start(eth_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start eth driver");
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
