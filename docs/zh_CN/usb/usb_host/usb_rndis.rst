USB RNDIS 主机驱动
====================

:link_to_translation:`en:[English]`

.. _label_usb_rndis:

`iot_usbh_rndis <https://components.espressif.com/components/espressif/iot_usbh_rndis>`_ 是基于 USB 接口的 RNDIS 协议的主机驱动。

RNDIS 协议
------------

RNDIS(Remote NDIS) 通过将 TCP/IP 封装在 USB 报文里，实现网络通信。

USB 描述符
---------------

RNDIS 设备默认自己为 USB 配置中的唯一功能。对于复合设备，RNDIS 希望自己是第一个 USB 配置, 即 ``bInterfaceNumber`` 为 0 的接口。

RNDIS 接口一般由一个 IAD 描述符和两个接口描述符组成。当前驱动支持以下两种 IAD 描述符：

1. ``bFunctionClass`` = 0xe0, ``bFunctionSubClass`` = 0x01, ``bFunctionProtocol`` = 0x03
2. ``bFunctionClass`` = 0xef, ``bFunctionSubClass`` = 0x04, ``bFunctionProtocol`` = 0x01

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

已测试 4G 模组
-------------------

+----------------+--------------------------------+----------+----------+----------+---------------------------------------------------------+
| 型号           | 固件版本                       | ESP32-S2 | ESP32-S3 | ESP32-P4 | USB 功能切换 AT 指令（仅供参考，以实际模组手册为准）    |
|                |                                |          |          |          |                                                         |
+================+================================+==========+==========+==========+=========================================================+
| ML307R         | ML307R-DC-MBRH0S00             | ✅       | ✅       | ✅       | - AT+MDIALUPCFG="mode",0                                |
|                |                                |          |          |          | - 0:RNDIS 1:ECM                                         |
+----------------+--------------------------------+----------+----------+----------+---------------------------------------------------------+
| ML302          | ML302-CNLM_MBRH0S00            | ✅       | ✅       | ✅       | - AT+SYSNV=1,"usbmode",3                                |
|                |                                |          |          |          | - 3:RNDIS+serials 5:ECM+serials                         |
+----------------+--------------------------------+----------+----------+----------+---------------------------------------------------------+
| AIR720 SL      | AirM2M_720SL_V545_LTE_AT       | ✅       | ✅       | ✅       | - AT+SETUSB=1                                           |
|                |                                |          |          |          | - 1:RNDIS  2:ECM                                        |
+----------------+--------------------------------+----------+----------+----------+---------------------------------------------------------+
| EC800E-CN      | EC800ECNLCR01A21M04            | ✅       | ✅       | ✅       | - AT+QCFG="usbnet",1                                    |
|                |                                |          |          |          | - 1:ECM 3:RNDIS                                         |
+----------------+--------------------------------+----------+----------+----------+---------------------------------------------------------+
| EC801E-CN      | EC801ECNCGR03A01M04_BETA_0521A | ✅       | ✅       | ✅       | - AT+QCFG="usbnet",1                                    |
|                |                                |          |          |          | - 1:ECM 3:RNDIS                                         |
+----------------+--------------------------------+----------+----------+----------+---------------------------------------------------------+
| YM310          | YM310.X09S_AT.A60_R2.1.3.241121| ✅       | ✅       | ✅       | - AT+ECPCFG="usbNet",0                                  |
|                |                                |          |          |          | - 0:RNDIS 1:ECM                                         |
+----------------+--------------------------------+----------+----------+----------+---------------------------------------------------------+
| MC610-EU       | 16000.1000.00.97.20.10         | ✅       | ✅       | ✅       | - AT+GTUSBMODE=33                                       |
|                |                                |          |          |          | - 33:RNDIS 32:ECM                                       |
|                |                                |          |          |          | - 需 AT+GTRNDIS=1,1 激活拨号                            |
+----------------+--------------------------------+----------+----------+----------+---------------------------------------------------------+
| Lierda NT26    | NT26FCNB30WNA-Q01010950        | ✅       | ✅       | ✅       | - AT+ECPCFG="usbNet",0                                  |
|                |                                |          |          |          | - 0:RNDIS 1:ECM                                         |
|                |                                |          |          |          | - 需 AT+ECNETDEVCTL=3,1 激活自动拨号                    |
+----------------+--------------------------------+----------+----------+----------+---------------------------------------------------------+

.. note::

    部分 4G 模组需要使用 AT 指令预先设置 RNDIS 模式下自动拨号才能连上网络，具体请参考对应模组的文档手册。

示例代码
-------------------------------

:example:`usb/host/usb_rndis_4g_module`

速率测试
--------------

ESP32-S3 连接 4G 网卡，并开始 softAP，手机连接 softAP 进行网络测速:

+----------+---------------+---------------+
|   芯片   | 上行(Mbps)    | 下行(Mbps)    |
+==========+===============+===============+
| ESP32-S3 | 5.8           | 7.9           |
+----------+---------------+---------------+

API 参考
---------

.. include-build-file:: inc/iot_usbh_rndis.inc
