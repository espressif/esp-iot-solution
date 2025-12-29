IOT ETH Ethernet
=================

:link_to_translation:`zh_CN:[中文]`

`iot_eth` provides a complete solution for connecting 4G modules to Ethernet, offering an abstraction layer for the underlying hardware and a default network interface based on LWIP. It also supports user-defined network interfaces.

Features
---------

- Advanced factory mode, supporting various underlying connection methods
- Default support for LWIP network interface, also supports user-defined network interfaces

User Guide
-----------

1. First, initialize the `driver` layer to provide lower-level hardware abstraction

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

2. Initialize the `iot_eth` layer

.. code:: c

    iot_eth_config_t eth_cfg = {
        .driver = rndis_handle,
        .stack_input = NULL,
    };

    iot_eth_handle_t eth_handle = NULL;
    ret = iot_eth_install(&eth_cfg, &eth_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install USB RNDIS driver");
        return;
    }

3. Start the `iot_eth` layer

.. code:: c

    ret = iot_eth_start(eth_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start eth driver");
        return;
    }

Using Other Network Interfaces
-------------------------------

1. Transmission: Call `iot_eth_transmit` to send data.

2. Reception: Configure the `stack_input` callback function in `iot_eth_config_t`. This callback is called when data is received.

3. Get the MAC address of the 4G module: Call `iot_eth_get_addr` to retrieve the MAC address.

Supported Network Interfaces
-----------------------------

- USB interface-based RNDIS :doc:`/usb/usb_host/usb_rndis`

- USB interface-based PPP :doc:`/usb/usb_host/usb_ppp`

- USB interface-based ECM :doc:`/usb/usb_host/usb_ecm`

API Reference
-------------

.. include-build-file:: inc/iot_eth.inc
