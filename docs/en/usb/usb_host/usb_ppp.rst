USB PPP
===========

:link_to_translation:`zh_CN:[中文]`

PPP (Point-to-Point Protocol) is one of the most fundamental protocols in networking. It is a data link layer protocol designed for simple links to transmit data packets between peer units. These links provide full-duplex operation and sequential packet delivery. PPP offers a common solution for simple connections based on various hosts, bridges, and routers.

The `iot_usbh_modem <https://components.espressif.com/components/espressif/iot_usbh_modem>`_ component implements the complete process of USB host PPP dial-up. It supports connecting 4G Cat.1/4 modules via USB interface to achieve PPP dial-up internet access. It also supports sharing internet connection to other devices through Wi-Fi softAP hotspot.

Features:
    * Quick start
    * Hot-plug support
    * Dual interface support for Modem+AT (module dependent)
    * PPP standard protocol support (supported by most 4G modules)
    * 4G to Wi-Fi hotspot conversion
    * NAPT network address translation support
    * Power management support
    * Automatic network recovery
    * SIM card detection and signal quality monitoring
    * Web configuration interface support

Supported Module Models:
-------------------------

The following table lists the supported 4G module models. They can be configured directly in menuconfig through the :c:macro:`MODEM_TARGET` macro. If the configuration is not effective, please select :c:macro:`MODEM_TARGET_USER` in menuconfig and manually configure the ITF interface.

.. note::

    The same module may have multiple functional firmwares. Please consult the module manufacturer for PPP dial-up support.

+-----------------+
|  Module Model   |
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

Setting up PPP Interface
-------------------------

The PPP interface is typically a USB CDC interface that must have a CDC data interface containing two bulk endpoints for input and output data. It may have a CDC notify interface with an interrupt endpoint, which is not used in this application code.

The PPP interface can be used for both AT command transmission and PPP data transfer, switching between them by sending "+++" data.

Example Descriptor
~~~~~~~~~~~~~~~~~~~~

Most PPP interfaces have a bInterfaceClass of 0xFF (Vendor Specific Class). In the example below, the bInterfaceNumber of the PPP interface is 0x03.

Example USB descriptor:

.. note::

    The following descriptor is an example. Not all descriptors follow this format exactly - some may include IAD interface descriptors or may not have interrupt endpoints.

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

After identifying the PPP interface, you can configure :c:macro:`MODEM_TARGET` as :c:macro:`MODEM_TARGET_USER` and set :c:macro:`MODEM_USB_ITF` to the bInterfaceNumber of the PPP interface.

Dual PPP Interface
~~~~~~~~~~~~~~~~~~~~

To enable AT command transmission while transferring data, you can use two PPP interfaces - one for data transfer and another for AT commands. This requires additional configuration of :c:macro:`MODEM_USB_ITF2`.

.. note::

    The availability of a second AT command interface depends on the device.
