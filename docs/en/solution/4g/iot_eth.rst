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

2. Initialize the `iot_eth` layer

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

3. Start the `iot_eth` layer

.. code:: c

    ret = iot_eth_start(eth_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start USB RNDIS driver");
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
