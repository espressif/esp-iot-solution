USB Host CDC
=====================

:link_to_translation:`zh_CN:[中文]`

The ``iot_usbh_cdc`` component implements a simple version of the USB host CDC driver. The API is designed similar like `ESP-IDF UART driver <https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/uart.html>`_, which can be used to replace the original UART driver to realize the update from UART to USB.

User Guide
---------------

1. Use ``usbh_cdc_driver_install`` to configure the USB CDC driver. Users can set up the driver and initialize the USB Host Driver protocol stack internally by setting the ``skip_init_usb_host_driver`` option.

.. code:: c

    /* Install the USB CDC driver and initialize the USB Host Driver protocol stack internally */
    usbh_cdc_driver_config_t config = {
        .driver_task_stack_size = 1024 * 4,
        .driver_task_priority = 5,
        .xCoreID = 0,
        .skip_init_usb_host_driver = false,
        .new_dev_cb = cdc_new_dev_cb,
    };

2. Use ``usbh_cdc_create`` to configure the interface number (``itf_num``) and the size of the internal ring buffer. Additionally, users can configure hot-plug callbacks such as ``connect``, ``disconnect``, and ``recv_data``:

.. code:: c

    /* Install the USB Host CDC driver and configure bulk endpoint addresses and internal ring buffer size */
    usbh_cdc_device_config_t dev_config = {
        .vid = 0,
        .pid = 0,
        .itf_num = 1,
        /* Set to 0 to use default values */
        .rx_buffer_size = 0,
        .tx_buffer_size = 0,
        .cbs = {
            .connect = cdc_connect_cb,
            .disconnect = cdc_disconnect_cb,
            .user_data = NULL
        },
    };

    usbh_cdc_handle_t handle = NULL;
    usbh_cdc_create(&dev_config, &handle);
    /* If multiple interfaces are required, configure them like this */
    #if (EXAMPLE_BULK_ITF_NUM > 1)
    config.itf_num = 3;
    usbh_cdc_handle_t handle2 = NULL;
    usbh_cdc_create(&dev_config, &handle2);
    #endif

3. After the driver is initialized, the internal state machine will automatically handle USB hot-plug events.
4. Once successfully connected, the host will automatically receive USB data from the CDC device into an internal ``ringbuffer``. Users can poll the buffer size using ``usbh_cdc_get_rx_buffer_size`` or register a callback to get notified when data is ready. Data can then be read using ``usbh_cdc_read_bytes``.
5. ``usbh_cdc_write_bytes`` can be used to send data to the USB device. The data is first written to an internal transmission ``ringbuffer`` and then sent over the USB bus when it is idle.
6. ``usbh_cdc_delete`` can be used to delete the USB CDC device and release the associated ring buffer and other resources.
7. ``usbh_cdc_driver_uninstall`` can completely uninstall the USB driver and release all resources.

Examples
-------------------------------

:example:`usb/host/usb_cdc_basic`

API Reference
-------------------------------

.. include-build-file:: inc/iot_usbh_cdc.inc
