USB ECM 主机驱动
====================

:link_to_translation:`en:[English]`

`iot_usbh_ecm <https://components.espressif.com/components/espressif/iot_usbh_ecm>`_ 是基于 USB 接口的 ECM 协议的主机驱动。

ECM 协议
------------

ECM(Ethernet Control Model) 用于在 USB 设备和主机之间交换以太网帧数据，使用 CDC 类接口来配置和管理各种以太网功能，通过将以太网帧封装在 USB 报文里，实现网络通信。

USB 描述符
---------------

根据 USB 规范规定，ECM 接口一般由一个两个接口描述符和类特殊描述符组成。第一个接口描述符的 ``bInterfaceClass`` 为 ``0x02``， ``bInterfaceSubClass`` 为 ``0x06``。ECM 功能描述符 ``usb_ecm_function_desc_t`` 中的 ``iMACAddress`` 为 MAC 地址的字符串索引， ``wMaxSegmentSize`` 为以太网帧的最大长度（一般为 1514）。

ECM 驱动安装后会自动扫描新插入 USB 设备的接口描述符，找到符合 ECM 协议的接口并进行解析和初始化。设备在插入后一般会通过通知 notify 端点发送网络连接状态的改变，驱动会根据状态进行相应的处理。
当设备不发送通知时，并且 ``ECM_AUTO_LINK_UP_AFTER_NO_NOTIFICATION`` 宏被开启，驱动会在 ``ECM_AUTO_LINK_UP_WAIT_TIME_MS`` 毫秒后自动将网络连接状态置为已连接，以便兼容一些不发送通知的 ECM 设备，此选项默认是开启状态。

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

已测试 ECM 设备
-------------------

USB 转以太网设备
^^^^^^^^^^^^^^^^^^

+-----------+------------------------+------------------------+-----------------------------+
|  芯片     |          ESP32-S2      |          ESP32-S3      |      ESP32-P4               |
+===========+========================+========================+=============================+
| CH397A    |     ✅                 |     ✅                 |       ✅                    |
+-----------+------------------------+------------------------+-----------------------------+
| RTL8152B  |     ✅                 |     ✅                 |       ✅                    |
+-----------+------------------------+------------------------+-----------------------------+
| NX7202D   |     ✅                 |     ✅                 |       ✅                    |
+-----------+------------------------+------------------------+-----------------------------+

4G 模组
^^^^^^^^^^

+-------------+--------------------------------+----------+----------+----------+---------------------------------------------------------+
| 型号        | 固件版本                       | ESP32-S2 | ESP32-S3 | ESP32-P4 | USB 功能切换 AT 指令（仅供参考，以实际模组手册为准）    |
+=============+================================+==========+==========+==========+=========================================================+
| ML307R      | ML307R-DC-MBRH0S00             | ❌       | ❌       | ✅       | - AT+MDIALUPCFG="mode",1                                |
|             |                                |          |          |          | - 0:RNDIS 1:ECM                                         |
+-------------+--------------------------------+----------+----------+----------+---------------------------------------------------------+
| ML302       | ML302-CNLM_MBRH0S00            | ✅       | ✅       | ✅       | - AT+SYSNV=1,"usbmode",5                                |
|             |                                |          |          |          | - 3:RNDIS 5:ECM                                         |
+-------------+--------------------------------+----------+----------+----------+---------------------------------------------------------+
| AIR720 SL   | AirM2M_720SL_V545_LTE_AT       | ❌       | ❌       | ✅       | - AT+SetUSB=2                                           |
|             |                                |          |          |          | - 1:RNDIS 2:ECM                                         |
+-------------+--------------------------------+----------+----------+----------+---------------------------------------------------------+
| EC800E-CN   | EC800ECNLCR01A21M04            | ✅       | ✅       | ✅       | - AT+QCFG="usbnet",1                                    |
|             |                                |          |          |          | - 3:RNDIS 1:ECM                                         |
+-------------+--------------------------------+----------+----------+----------+---------------------------------------------------------+
| EC801E-CN   | EC801ECNCGR03A01M04_BETA_0521A | ✅       | ✅       | ✅       | - AT+QCFG="usbnet",1                                    |
|             |                                |          |          |          | - 3:RNDIS 1:ECM                                         |
+-------------+--------------------------------+----------+----------+----------+---------------------------------------------------------+
| YM310       | YM310.X09S_AT.A60_R2.1.3.241121| ✅       | ✅       | ✅       | - AT+ECPCFG="usbNet",1                                  |
|             |                                |          |          |          | - 0:RNDIS 1:ECM                                         |
+-------------+--------------------------------+----------+----------+----------+---------------------------------------------------------+
| MC610-EU    | 16000.1000.00.97.20.10         | ✅       | ✅       | ✅       | - AT+GTUSBMODE=32                                       |
|             |                                |          |          |          | - 33:RNDIS 32:ECM                                       |
|             |                                |          |          |          | - 需 AT+GTRNDIS=1,1 激活拨号                            |
+-------------+--------------------------------+----------+----------+----------+---------------------------------------------------------+
| Lierda NT26 | NT26FCNB30WNA-Q01010950        | ✅       | ✅       | ✅       | - AT+ECPCFG="usbNet",1                                  |
|             |                                |          |          |          | - 0:RNDIS 1:ECM                                         |
|             |                                |          |          |          | - 需 AT+ECNETDEVCTL=3,1 激活自动拨号                    |
+-------------+--------------------------------+----------+----------+----------+---------------------------------------------------------+

.. note::
   ✅ 表示支持，❌ 表示端点大小不支持（因为这部分 4G 模组在 Full Speed 下端点 mps 超出标准的 64 字节）。

.. note::

    部分 4G 模组需要使用 AT 指令预先设置 ECM 模式下自动拨号才能连上网络，具体请参考对应模组的文档手册。

FAQ
-----------------

1. 4G 模组获取 IP 之后 ping 超时，如何解决？

    网络未连接通，检查是否已经激活 ECM 模式下的自动拨号，具体请参考对应模组的文档手册；检查 SIM 卡是否正常插入，是否正常注册上网络。

2. 4G 模组连接上 USB 后打印 "No ECM interface found for device VID: xxxx, PID: xxxx, ignore it"

    未识别到 ECM 接口，检查模组是否已经正确设置为 ECM 模式，具体请参考对应模组的文档手册。

3. 4G 模组连接上 USB 后打印 "E (887) HCD DWC: EP MPS (512) exceeds supported limit (408)"

    这个模组的 USB 端点 MPS 超出该款 ESP 芯片硬件允许的大小，请尝试使用 :ref:`RNDIS 示例 <label_usb_rndis>` 或者更换另一个 4G 模组。

示例代码
-------------------------------

:example:`usb/host/usb_ecm_4g_module`

资料下载
----------

`CDC1.2 协议文档 <https://usb.org/sites/default/files/CDC1.2_WMC1.1_012011.zip>`_

API 参考
---------

.. include-build-file:: inc/iot_usbh_ecm.inc
