USB 主机 CDC
=====================

:link_to_translation:`en:[English]`

``iot_usbh_cdc`` 组件实现了一个 USB 主机 CDC 驱动的简化版本，用户可以通过简单的配置来使用 USB CDC 设备。该组件支持热插拔，并且可以选择性地启用内部环形缓冲区以简化数据收发。

使用指南
---------------

1. 使用 ``usbh_cdc_driver_install`` 配置, 用户可以配置好 USB CDC 驱动程序，并通过设置 ``skip_init_usb_host_driver`` 配置项，在组件内部初始化好 USB HOST Driver 协议栈。当其他 USB 驱动程序也需要使用 USB Host Driver 协议栈时，可以将该配置项设置为 true，以避免重复初始化。

.. code:: c

    /* 安装 USB CDC 驱动，并由驱动内部初始化好 USB Host Driver 协议栈 */
    usbh_cdc_driver_config_t config = {
        .task_stack_size = 1024 * 4,
        .task_priority = 5,
        .task_coreid = 0,
        .skip_init_usb_host_driver = false,
    };
    usbh_cdc_driver_install(&config);

2. 使用 ``usbh_cdc_register_dev_event_cb`` 注册一个 CDC 设备事件，并配置好设备匹配列表 ``dev_match_id_list``，当有新的 USB 设备连接时，驱动程序将根据该列表来匹配设备。如果有任何一项匹配成功，驱动程序将调用该回调函数。用户可以在这个函数中使用 ``usbh_cdc_port_open`` 函数根据应用的需要选择打开设备的一个或者多个端口用于通信。

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

参数 ``dev_match_id_list`` 指针需要传入一个不在栈上的数组，并且必须以空项结尾。数组中的一项可以是用于匹配一个或多个设备的条件，例如上面的例子中，使用 ``USB_DEVICE_VENDOR_ANY`` 和 ``USB_DEVICE_PRODUCT_ANY`` 来匹配所有的 USB 设备。用户也可以指定特定的 ``idVendor`` 和 ``idProduct`` 来匹配特定的设备。还可以通过 ``match_flags`` 来指定更多的匹配条件，例如 ``USB_DEVICE_ID_MATCH_DEV_CLASS``， ``USB_DEVICE_ID_MATCH_DEV_SUBCLASS``， ``USB_DEVICE_ID_MATCH_DEV_PROTOCOL`` 等等。更多的匹配条件可以参考 :cpp:type:`usb_dev_match_flags_t`。

在 ``device_event_cb`` 回调函数中不允许调用 ``usbh_cdc_write_bytes`` 等对设备进行通信的函数，因为此回调函数的上下文是在 USB 任务中，调用这些函数会导致死锁。建议在 ``device_event_cb`` 回调中创建一个新的任务或者发送一个事件到应用任务中，在新的任务或者应用任务中进行设备通信。

3. 端口打开之后，主机将自动从 CDC 设备接收 USB 数据到 USB 缓冲区，用户可以轮询 ``usbh_cdc_get_rx_buffer_size`` 以读取缓冲数据大小，或者注册接收回调以在数据准备就绪时得到通知。然后 ``usbh_cdc_read_bytes`` 可以用于读取缓冲数据。当 ``in_ringbuf_size`` 大于 0 时，驱动程序将启用内部 ringbuffer 来缓存接收到的数据。
4. ``usbh_cdc_write_bytes`` 可以用于向 USB 设备发送数据。当 ``out_ringbuf_size`` 大于 0 时，数据会首先被写入内部传输 ``ringbuffer``，而不是直接写入到 USB 缓冲区。
5. ``usbh_cdc_port_close`` 可以关闭 USB CDC 端口，释放掉其资源。
6. ``usbh_cdc_driver_uninstall`` 可以完全卸载 USB 驱动程序以释放所有资源，当仍有设备连接时，无法直接卸载驱动程序。

.. note::
    由于 USB 设备的热插拔是异步的，因此 USB 设备可能会在运行时的任何时候被拔出。用户需要确保在调用 ``usbh_cdc_write_bytes`` 或 ``usbh_cdc_read_bytes`` 等函数时，设备仍然连接且端口已打开，否则这些函数将返回错误。

支持的 CDC 设备描述符
-----------------------

驱动支持如下的标准 CDC 设备描述符：

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

还支持部分厂商自定义的 CDC 设备描述符，例如：

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

    在使用 USB HUB 连接多个设备时，如果出现主机的 channel 已经不够使用的情况，可以尝试通过 :c:struct:`usbh_cdc_port_config_t` 中的 flags 打开 :c:type:`USBH_CDC_FLAGS_DISABLE_NOTIFICATION` 标志位，强制禁用中断端点减少一个 channel 的使用，如果设备本身就没有中断端点则无效。

示例代码
-------------------------------

:example:`usb/host/usb_cdc_basic`

API 参考
-------------------------------

.. include-build-file:: inc/iot_usbh_cdc.inc

.. include-build-file:: inc/usbh_helper.inc
