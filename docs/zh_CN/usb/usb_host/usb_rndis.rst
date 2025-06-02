USB RNDIS 主机驱动
====================

:link_to_translation:`en:[English]`

`iot_usbh_rndis <https://components.espressif.com/components/espressif/iot_usbh_rndis>`_ 是基于 USB 接口的 RNDIS 协议的主机驱动。

RNDIS 协议
------------

RNDIS(Remote NDIS) 通过将 TCP/IP 封装在 USB 报文里，实现网络通信。


USB 描述符
---------------

RNDIS 设备默认自己为 USB 配置中的唯一功能。对于复合设备，RNDIS 希望自己是第一个 USB 配置, 即 ``bInterfaceNumber`` 为 0 的接口。

RNDIS 接口一般由一个 IAD 描述符和两个接口描述符组成。IAD 描述符的 ``bInterfaceCount`` 为 ``2``， ``bFunctionClass`` 为 ``0xe0``。

示例：

.. code:: c

    *** Interface Association Descriptor ***
    bLength 8
    bDescriptorType 11
    bFirstInterface 0
    bInterfaceCount 2
    bFunctionClass 0xe0
    bFunctionSubClass 0x1
    bFunctionProtocol 0x3
    iFunction 5
        *** Interface descriptor ***
        bLength 9
        bDescriptorType 4
        bInterfaceNumber 0
        bAlternateSetting 0
        bNumEndpoints 1
        bInterfaceClass 0xe0
        bInterfaceSubClass 0x1
        bInterfaceProtocol 0x3
        iInterface 5
                *** Endpoint descriptor ***
                bLength 7
                bDescriptorType 5
                bEndpointAddress 0x85   EP 5 IN
                bmAttributes 0x3        INT
                wMaxPacketSize 16
                bInterval 16
        *** Interface descriptor ***
        bLength 9
        bDescriptorType 4
        bInterfaceNumber 1
        bAlternateSetting 0
        bNumEndpoints 2
        bInterfaceClass 0xa
        bInterfaceSubClass 0x0
        bInterfaceProtocol 0x0
        iInterface 5
                *** Endpoint descriptor ***
                bLength 7
                bDescriptorType 5
                bEndpointAddress 0x87   EP 7 IN
                bmAttributes 0x2        BULK
                wMaxPacketSize 64
                bInterval 0
                *** Endpoint descriptor ***
                bLength 7
                bDescriptorType 5
                bEndpointAddress 0x6    EP 6 OUT
                bmAttributes 0x2        BULK
                wMaxPacketSize 64
                bInterval 0

通信流程
----------

1. 通过控制端点发送初始化消息 ``REMOTE_NDIS_INITIALIZE_MSG``，设备会返回 ``REMOTE_NDIS_INITIALIZE_CMPLT_MSG`` 消息。

2. 接着主机需要发送 ``REMOTE_NDIS_QUERY_MSG`` 消息，用于查询对象描述符（OID），并根据 OID 来查询和支持设备。如拿到设备的 MAC 地址，连接状态，连接速度等。

3. 接着主机端就可以发起 DHCP 请求，获取 IP 地址。

帧格式
-------

RNDIS 协议的帧格式如下：

发送：RNDIS 数据包需要在以太网帧前添加 RNDIS 数据头 ``rndis_data_packet_t``，然后添加以太网帧报文。

接收：RNDIS 数据包在以太网帧头部添加 RNDIS 数据头 ``rndis_data_packet_t``。需要先解析 RNDIS 数据头，再拿到以太网帧。

速率测试
----------

ESP32-S3 连接 4G 网卡，并开始 softAP，手机连接 softAP 进行网络测速:

+----------+------------+------------+
|   芯片   | 上行(Mbps) | 下行(Mbps) |
+==========+============+============+
| ESP32-S3 | 5.8        | 7.9        |
+----------+------------+------------+

API 参考
---------

.. include-build-file:: inc/iot_usbh_rndis.inc
