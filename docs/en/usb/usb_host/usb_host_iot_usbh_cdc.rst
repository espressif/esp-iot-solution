USB Host CDC
=====================

:link_to_translation:`zh_CN:[中文]`

The ``iot_usbh_cdc`` component implements a simplified USB host CDC driver. With simple configuration, users can use USB CDC devices. The component supports hot‑plug and optionally enables an internal ring buffer to simplify data sending and receiving.

Usage Guide
---------------

1. Use ``usbh_cdc_driver_install`` to configure the USB CDC driver. By setting the ``skip_init_usb_host_driver`` option, the component can initialize the USB Host Driver stack internally. When other USB drivers also need to use the USB Host Driver stack, set this option to true to avoid repeated initialization.

.. code:: c

    /* Install the USB CDC driver and initialize the USB Host Driver stack inside the driver */
    usbh_cdc_driver_config_t config = {
        .task_stack_size = 1024 * 4,
        .task_priority = 5,
        .task_coreid = 0,
        .skip_init_usb_host_driver = false,
    };
    usbh_cdc_driver_install(&config);

2. Use ``usbh_cdc_register_dev_event_cb`` to register a CDC device event, and configure the device match list ``dev_match_id_list``. When a new USB device is connected, the driver will match it against this list. If any item matches, the driver will invoke the event callback. In this callback, the user can call ``usbh_cdc_port_open`` to open one or more ports of the device for communication as needed by the application.

.. code:: c

    static void device_event_cb(usbh_cdc_device_event_t event, usbh_cdc_device_event_data_t *event_data, void *user_ctx)
    {
        switch (event) {
        case CDC_HOST_DEVICE_EVENT_CONNECTED: {
            ESP_LOGI(TAG, "Device connected: dev_addr=%d, matched_intf_num=%d",
                    event_data->new_dev.dev_addr, event_data->new_dev.matched_intf_num);

            usbh_cdc_port_handle_t cdc_port = NULL;
            usbh_cdc_port_config_t cdc_port_config = {
                .dev_addr = event_data->new_dev.dev_addr,
                .itf_num = 0,
                .in_transfer_buffer_size = 512,
                .out_transfer_buffer_size = 512,
                .cbs = {
                    .notif_cb = NULL,
                    .recv_data = NULL,
                    .closed = NULL,
                    .user_data = NULL,
                },
            };
            usbh_cdc_port_open(&cdc_port_config, &cdc_port);
            } break;

        case CDC_HOST_DEVICE_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "Device disconnected: dev_addr=%d, dev_hdl=%p",
                    event_data->dev_gone.dev_addr, event_data->dev_gone.dev_hdl);
            break;

        default:
            break;
        }
    }

    const static usb_device_match_id_t match_id_list[] = {
        {
            .match_flags = USB_DEVICE_ID_MATCH_VENDOR | USB_DEVICE_ID_MATCH_PRODUCT,
            .idVendor = USB_DEVICE_VENDOR_ANY,
            .idProduct = USB_DEVICE_PRODUCT_ANY,
        },
        { 0 }
    };
    usbh_cdc_register_dev_event_cb(match_id_list, device_event_cb, NULL);

The parameter ``dev_match_id_list`` pointer requires an array that is not on the stack and must be null-terminated. An array entry can specify conditions to match one or more devices. For example, the above example uses ``USB_DEVICE_VENDOR_ANY`` and ``USB_DEVICE_PRODUCT_ANY`` to match all USB devices. Users can also specify specific ``idVendor`` and ``idProduct`` to match specific devices. You can further specify conditions using ``match_flags``, such as ``USB_DEVICE_ID_MATCH_DEV_CLASS``, ``USB_DEVICE_ID_MATCH_DEV_SUBCLASS``, ``USB_DEVICE_ID_MATCH_DEV_PROTOCOL``, etc. For more matching conditions, see :cpp:type:`usb_dev_match_flags_t`.

Do not call communication functions such as ``usbh_cdc_write_bytes`` in the ``device_event_cb`` callback, because this callback runs in the USB task context and calling these functions can cause a deadlock. It is recommended to create a new task in the ``device_event_cb`` callback or send an event to the application task, and perform device communication in the new task or the application task.

3. After a port is opened, the host will automatically receive USB data from the CDC device into the USB buffer. The user can poll ``usbh_cdc_get_rx_buffer_size`` to read the buffered data length, or register a receive callback to be notified when data is ready. Then ``usbh_cdc_read_bytes`` can be used to read the buffered data. When ``in_ringbuf_size`` is greater than 0, the driver enables an internal ring buffer to cache received data.
4. ``usbh_cdc_write_bytes`` can be used to send data to the USB device. When ``out_ringbuf_size`` is greater than 0, data is first written to the internal transmission ``ringbuffer`` instead of directly to the USB buffer.
5. ``usbh_cdc_port_close`` can delete the USB CDC device and release its resources.
6. ``usbh_cdc_driver_uninstall`` can fully uninstall the USB driver to release all resources; if a device is still connected, the driver cannot be uninstalled directly.

.. note::
    Since USB device hot‑plug is asynchronous, a USB device may be unplugged at any time during runtime. Users must ensure that when calling functions such as ``usbh_cdc_write_bytes`` or ``usbh_cdc_read_bytes``, the device is still connected and the port is open; otherwise these functions will return an error.

Supported CDC Device Descriptors
----------------------------------

The driver supports the following standard CDC device descriptors:

.. code:: 

            ------------------- IAD Descriptor --------------------
    bLength                  : 0x08 (8 bytes)
    bDescriptorType          : 0x0B (Interface Association Descriptor)
    bFirstInterface          : 0x00 (Interface 0)
    bInterfaceCount          : 0x02 (2 Interfaces)
    bFunctionClass           : 0x02 (Communications and CDC Control)
    bFunctionSubClass        : 0x06
    bFunctionProtocol        : 0x00
    iFunction                : 0x07 (String Descriptor 7)
    Language 0x0409         : ""

            ---------------- Interface Descriptor -----------------
    bLength                  : 0x09 (9 bytes)
    bDescriptorType          : 0x04 (Interface Descriptor)
    bInterfaceNumber         : 0x00 (Interface 0)
    bAlternateSetting        : 0x00
    bNumEndpoints            : 0x01 (1 Endpoint)
    bInterfaceClass          : 0x02 (Communications and CDC Control)
    bInterfaceSubClass       : 0x06 (Ethernet Networking Control Model)
    bInterfaceProtocol       : 0x00 (No class specific protocol required)
    iInterface               : 0x07 (String Descriptor 7)
    Language 0x0409         : ""

            ----------------- Endpoint Descriptor -----------------
    bLength                  : 0x07 (7 bytes)
    bDescriptorType          : 0x05 (Endpoint Descriptor)
    bEndpointAddress         : 0x87 (Direction=IN EndpointID=7)
    bmAttributes             : 0x03 (TransferType=Interrupt)
    wMaxPacketSize           : 0x0020
    Bits 15..13             : 0x00 (reserved, must be zero)
    Bits 12..11             : 0x00 (0 additional transactions per microframe -> allows 1..1024 bytes per packet)
    Bits 10..0              : 0x20 (32 bytes per packet)
    bInterval                : 0x09 (256 microframes -> 32 ms)

            ---------------- Interface Descriptor -----------------
    bLength                  : 0x09 (9 bytes)
    bDescriptorType          : 0x04 (Interface Descriptor)
    bInterfaceNumber         : 0x01 (Interface 1)
    bAlternateSetting        : 0x00
    bNumEndpoints            : 0x00 (Default Control Pipe only)
    bInterfaceClass          : 0x0A (CDC-Data)
    bInterfaceSubClass       : 0x00
    bInterfaceProtocol       : 0x00
    iInterface               : 0x00 (No String Descriptor)

            ---------------- Interface Descriptor -----------------
    bLength                  : 0x09 (9 bytes)
    bDescriptorType          : 0x04 (Interface Descriptor)
    bInterfaceNumber         : 0x01 (Interface 1)
    bAlternateSetting        : 0x01
    bNumEndpoints            : 0x02 (2 Endpoints)
    bInterfaceClass          : 0x0A (CDC-Data)
    bInterfaceSubClass       : 0x00
    bInterfaceProtocol       : 0x00
    iInterface               : 0x00 (No String Descriptor)

            ----------------- Endpoint Descriptor -----------------
    bLength                  : 0x07 (7 bytes)
    bDescriptorType          : 0x05 (Endpoint Descriptor)
    bEndpointAddress         : 0x0C (Direction=OUT EndpointID=12)
    bmAttributes             : 0x02 (TransferType=Bulk)
    wMaxPacketSize           : 0x0200 (max 512 bytes)
    bInterval                : 0x00 (never NAKs)

            ----------------- Endpoint Descriptor -----------------
    bLength                  : 0x07 (7 bytes)
    bDescriptorType          : 0x05 (Endpoint Descriptor)
    bEndpointAddress         : 0x83 (Direction=IN EndpointID=3)
    bmAttributes             : 0x02 (TransferType=Bulk)
    wMaxPacketSize           : 0x0200 (max 512 bytes)
    bInterval                : 0x00 (never NAKs)

The driver also supports some vendor-specific CDC device descriptors, for example:

.. code:: 

            ---------------- Interface Descriptor -----------------
    bLength                  : 0x09 (9 bytes)
    bDescriptorType          : 0x04 (Interface Descriptor)
    bInterfaceNumber         : 0x03 (Interface 3)
    bAlternateSetting        : 0x00
    bNumEndpoints            : 0x03 (3 Endpoints)
    bInterfaceClass          : 0xFF (Vendor Specific)
    bInterfaceSubClass       : 0x00
    bInterfaceProtocol       : 0x00
    iInterface               : 0x09 (String Descriptor 9)
    Language 0x0409         : "Mobile AT Interface"

            ----------------- Endpoint Descriptor -----------------
    bLength                  : 0x07 (7 bytes)
    bDescriptorType          : 0x05 (Endpoint Descriptor)
    bEndpointAddress         : 0x85 (Direction=IN EndpointID=5)
    bmAttributes             : 0x03 (TransferType=Interrupt)
    wMaxPacketSize           : 0x0010
    Bits 15..13             : 0x00 (reserved, must be zero)
    Bits 12..11             : 0x00 (0 additional transactions per microframe -> allows 1..1024 bytes per packet)
    Bits 10..0              : 0x10 (16 bytes per packet)
    bInterval                : 0x09 (256 microframes -> 32 ms)

            ----------------- Endpoint Descriptor -----------------
    bLength                  : 0x07 (7 bytes)
    bDescriptorType          : 0x05 (Endpoint Descriptor)
    bEndpointAddress         : 0x82 (Direction=IN EndpointID=2)
    bmAttributes             : 0x02 (TransferType=Bulk)
    wMaxPacketSize           : 0x0200 (max 512 bytes)
    bInterval                : 0x00 (never NAKs)

            ----------------- Endpoint Descriptor -----------------
    bLength                  : 0x07 (7 bytes)
    bDescriptorType          : 0x05 (Endpoint Descriptor)
    bEndpointAddress         : 0x0B (Direction=OUT EndpointID=11)
    bmAttributes             : 0x02 (TransferType=Bulk)
    wMaxPacketSize           : 0x0200 (max 512 bytes)
    bInterval                : 0x00 (never NAKs)

.. note::

    When connecting multiple devices using a USB HUB, if the HOST's channels are insufficient, you can try enabling the :c:type:`USBH_CDC_FLAGS_DISABLE_NOTIFICATION` flag in :c:struct:`usbh_cdc_port_config_t` to forcibly disable interrupt endpoint and reduce the use of one channel. This will not work if the device itself does not have interrupt endpoint.

Example Code
-------------------------------

:example:`usb/host/usb_cdc_basic`

API Reference
-------------------------------

.. include-build-file:: inc/iot_usbh_cdc.inc

.. include-build-file:: inc/usbh_helper.inc
