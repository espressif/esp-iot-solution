USB ECM 主机驱动
====================

:link_to_translation:`en:[English]`

`iot_usbh_ecm <https://components.espressif.com/components/espressif/iot_usbh_ecm>`_ 是基于 USB 接口的 ECM 协议的主机驱动。

ECM 协议
------------

ECM(Ethernet Control Model) 通过将以太网帧封装在 USB 报文里，实现网络通信。

USB 描述符
---------------

ECM 接口一般由一个两个接口描述符和类特殊描述符组成。第一个接口描述符的 ``bInterfaceClass`` 为 ``0x02``， ``bInterfaceSubClass`` 为 ``0x06``。
ECM 功能描述符 ``usb_ecm_function_desc_t`` 中的 ``iMACAddress`` 为 MAC 地址的字符串索引， ``wMaxSegmentSize`` 为以太网帧的最大长度（默认为 1514）。

.. Note::
    一些 ECM 设备的配置描述符不为默认的 config 1，因此需要开启宏 :c:macro:`CONFIG_USB_HOST_ENABLE_ENUM_FILTER_CALLBACK` 并实现枚举过滤回调函数。

示例描述符：

.. code:: c

        *** Interface descriptor ***
        bLength 9
        bDescriptorType 4
        bInterfaceNumber 0
        bAlternateSetting 0
        bNumEndpoints 1
        bInterfaceClass 0x2
        bInterfaceSubClass 0x6
        bInterfaceProtocol 0x0
        iInterface 5
                *** Endpoint descriptor ***
                bLength 7
                bDescriptorType 5
                bEndpointAddress 0x81   EP 1 IN
                bmAttributes 0x3        INT
                wMaxPacketSize 64
                bInterval 8
        *** Interface descriptor ***
        bLength 9
        bDescriptorType 4
        bInterfaceNumber 1
        bAlternateSetting 0
        bNumEndpoints 0
        bInterfaceClass 0xa
        bInterfaceSubClass 0x0
        bInterfaceProtocol 0x0
        iInterface 0
        *** Interface descriptor ***
        bLength 9
        bDescriptorType 4
        bInterfaceNumber 1
        bAlternateSetting 1
        bNumEndpoints 2
        bInterfaceClass 0xa
        bInterfaceSubClass 0x0
        bInterfaceProtocol 0x0
        iInterface 0
                *** Endpoint descriptor ***
                bLength 7
                bDescriptorType 5
                bEndpointAddress 0x82   EP 2 IN
                bmAttributes 0x2        BULK
                wMaxPacketSize 64
                bInterval 0
                *** Endpoint descriptor ***
                bLength 7
                bDescriptorType 5
                bEndpointAddress 0x3    EP 3 OUT
                bmAttributes 0x2        BULK
                wMaxPacketSize 64
                bInterval 0

速率测试
----------

以太网转 USB 设备
^^^^^^^^^^^^^^^^^^

ESP32-S3/P4 连接 以太网转 USB 设备（CH397A），并开启 softAP，手机连接 softAP 进行网络测速:

+------------------------+------------+------------+
|          芯片          | 上行(Mbps) | 下行(Mbps) |
+========================+============+============+
| ESP32-S3               | 6.6        | 7.2        |
+------------------------+------------+------------+
| ESP32-P4 with ESP32-C6 | 11.7       | 14.7       |
+------------------------+------------+------------+

4G 模组
^^^^^^^^^^

暂无

资料下载
----------

`CDC1.2 协议文档 <https://usb.org/sites/default/files/CDC1.2_WMC1.1_012011.zip>`_

API 参考
---------

.. include-build-file:: inc/iot_usbh_ecm.inc
