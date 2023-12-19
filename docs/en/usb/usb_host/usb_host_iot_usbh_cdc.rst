USB Host CDC 
=====================

:link_to_translation:`zh_CN:[中文]`

The ``iot_usbh_cdc`` component implements a simple version of the USB host CDC driver. The API is designed similar like `ESP-IDF UART driver <https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/uart.html>`_, which can be used to replace the original UART driver to realize the update from UART to USB.

User Guide
---------------

1. Using ``usbh_cdc_driver_install`` to configure, user can simply configure the bulk endpoint address and the size of the internal ringbuffer, user can also configure the hot plug related callback function ``conn_callback`` ``disconn_callback``.

.. code:: c

    /* install usbh cdc driver with bulk endpoint configs and size of internal ringbuffer */
    static usbh_cdc_config_t config = {
        /* use default endpoint descriptor with user address */
        .bulk_in_ep_addr = EXAMPLE_BULK_IN_EP_ADDR,
        .bulk_out_ep_addr = EXAMPLE_BULK_OUT_EP_ADDR,
        .rx_buffer_size = IN_RINGBUF_SIZE,
        .tx_buffer_size = OUT_RINGBUF_SIZE,
        .conn_callback = usb_connect_callback,
        .disconn_callback = usb_disconnect_callback,
    };
    /* if user want to use multiple interfaces, can configure like this */
    #if (EXAMPLE_BULK_ITF_NUM > 1)
    config.itf_num = 2;
    config.bulk_in_ep_addrs[1] = EXAMPLE_BULK_IN1_EP_ADDR;
    config.bulk_out_ep_addrs[1] = EXAMPLE_BULK_OUT1_EP_ADDR;
    config.rx_buffer_sizes[1] = IN_RINGBUF_SIZE;
    config.tx_buffer_sizes[1] = OUT_RINGBUF_SIZE;
    #endif

    /* install USB host CDC driver */
    usbh_cdc_driver_install(&config);

    /* Waiting for USB device connected */
    usbh_cdc_wait_connect(portMAX_DELAY);

2. After the driver initialization, the internal state machine will automatically handle the hot plug of the USB.
3. ``usbh_cdc_wait_connect`` can be used to block task until USB CDC Device is connected or timeout.
4. After successfully connected, the host will automatically receive USB data from CDC device to the internal ``ringbuffer``, user can poll ``usbh_cdc_get_buffered_data_len`` to read buffered data size or register a receive callback to get notified when data is ready. Then ``usbh_cdc_read_bytes`` can be used to read buffered data out.
5. ``usbh_cdc_write_bytes`` can be used to send data to USB Device. The data is first written to the internal transmit ``ringbuffer``，then will be sent out during USB bus free.
6. ``usbh_cdc_driver_delete`` can uninstall the USB driver completely to release all resources.
7. If config multiple CDC interfaces, each interface contains an IN and OUT endpoint. Users can communicate with the specified interfaces using ``usbh_cdc_itf_read_bytes`` and ``usbh_cdc_itf_write_bytes``.

Examples
-------------------------------

:example:`usb/host/usb_cdc_basic`

API Reference
-------------------------------

.. include-build-file:: inc/iot_usbh_cdc.inc
