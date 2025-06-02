USB PPP
=============

:link_to_translation:`en:[English]`

PPP 是一种网络中最为基础的协议。PPP 协议（Point-to-Point Protocol） 是一种数据链路层协议，它是为在同等单元之间传输数据包这样的简单链路而设计的。这种链路提供全双工操作，并按照顺序传递数据包。PPP 为基于各种主机、网桥和路由器的简单连接提供一种共通的解决方案。

`iot_usbh_modem <https://components.espressif.com/components/espressif/iot_usbh_modem>`_ 组件实现了 USB 主机 PPP 拨号的全流程。支持通过 USB 接口连接 4G Cat.1/4 模块，实现 PPP 拨号上网。支持通过 Wi-Fi softAP 热点共享互联网给其他设备。

特性：
    * 快速启动
    * 支持热插拔
    * 支持 Modem+AT 双接口（需要模组支持）
    * 支持 PPP 标准协议 （大部分 4G 模组均支持）
    * 支持 4G 转 Wi-Fi 热点
    * 支持 NAPT 网络地址转换
    * 支持电源管理
    * 支持网络自动恢复
    * 支持卡检测、信号质量检测
    * 支持网页配置界面

已支持的模组型号：
------------------

下表是已经支持的 4G 模组型号，可直接在 menuconfig 中通过宏 :c:macro:`MODEM_TARGET` 配置，如配置后不生效，请直接在 menuconfig 中选择 `MODEM_TARGET_USER` 并手动配置 ITF 接口。

.. note::

    同一个模组可能存在多个功能固件，具体请咨询模组厂商是否支持 PPP 拨号。

+-----------------+
|    模组型号     |
+=================+
| ML302-DNLM/CNLM |
+-----------------+
| NT26            |
+-----------------+
| EC600NCNLC-N06  |
+-----------------+
| AIR780E         |
+-----------------+
| MC610_EU        |
+-----------------+
| EC20_CE         |
+-----------------+
| EG25_GL         |
+-----------------+
| YM310_X09       |
+-----------------+
| SIM7600E        |
+-----------------+
| A7670E          |
+-----------------+
| SIM7070G        |
+-----------------+
| SIM7080G        |
+-----------------+

设置 PPP 接口
---------------

PPP 接口一般为 USB CDC 接口，一定具有一个 CDC data 接口，包含两个 bulk 端点，用于输入和输出数据。可能具有一个 CDC notify 接口，包含一个 interrupt 端点。本应用代码中并不使用该接口。

PPP 接口可用于传输 AT 指令和 PPP 数据传输，通过发送 "+++" 数据进行切换。

示例描述符
~~~~~~~~~~~~~

大部分的 PPP 接口的 bInterfaceClass 都为 0xFF (Vendor Specific Class)。如下图改 USB 设备的 PPP 接口的 bInterfaceNumber 为 0x03。

示例 USB 描述符：

.. note::

    以下描述符为示例，并非所有的描述符都如下，可能带有 IAD 接口描述符或者不具有 interrupt 端点。

.. code-block:: c

    *** Interface descriptor ***
    bLength 9
    bDescriptorType 4
    bInterfaceNumber 3
    bAlternateSetting 0
    bNumEndpoints 3
    bInterfaceClass 0xff
    bInterfaceSubClass 0x0
    bInterfaceProtocol 0x0
    iInterface 8
            *** Endpoint descriptor ***
            bLength 7
            bDescriptorType 5
            bEndpointAddress 0x8a   EP 10 IN
            bmAttributes 0x3        INT
            wMaxPacketSize 16
            bInterval 16
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
            bEndpointAddress 0x1    EP 1 OUT
            bmAttributes 0x2        BULK
            wMaxPacketSize 64
            bInterval 0

当我们找到了 PPP 接口后，可以配置 :c:macro:`MODEM_TARGET` 为 `MODEM_TARGET_USER`，并配置 :c:macro:`MODEM_USB_ITF` 为 PPP 接口的 bInterfaceNumber。

双 PPP 接口
~~~~~~~~~~~~~

为了保证数据传输时还能传输 AT 指令，可以使用两个 PPP 接口，一个用于传输数据，一个用于传输 AT 指令。需要额外的配置 :c:macro:`MODEM_USB_ITF2`。

.. note::

    是否具有第二个 AT 指令接口，要视设备而定。
