USB ECM Host Driver
====================

:link_to_translation:`zh_CN:[中文]`

`iot_usbh_ecm <https://components.espressif.com/components/espressif/iot_usbh_ecm>`_ is a host driver for the ECM protocol based on USB interface.

ECM Protocol
--------------

ECM (Ethernet Control Model) enables network communication by encapsulating Ethernet frames in USB packets.

USB Descriptors
----------------

ECM interface typically consists of two interface descriptors and class-specific descriptors. The first interface descriptor has ``bInterfaceClass`` as ``0x02`` and ``bInterfaceSubClass`` as ``0x06``.
In the ECM function descriptor ``usb_ecm_function_desc_t``, ``iMACAddress`` is the string index for MAC address, and ``wMaxSegmentSize`` is the maximum Ethernet frame size (default 1514).

.. Note::
    Some ECM devices don't use the default config 1 descriptor, so you need to enable :c:macro:`CONFIG_USB_HOST_ENABLE_ENUM_FILTER_CALLBACK` and implement the enumeration filter callback function.

Example Descriptor:

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

Speed Test
------------

Ethernet to USB Device
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

ESP32-S3/P4 connected to Ethernet to USB device (CH397A), with softAP enabled and phone connected to softAP for network speed test:

+------------------------+--------------+----------------+
|          Chip          | Upload(Mbps) | Download(Mbps) |
+========================+==============+================+
| ESP32-S3               | 6.6          | 7.2            |
+------------------------+--------------+----------------+
| ESP32-P4 with ESP32-C6 | 11.7         | 14.7           |
+------------------------+--------------+----------------+

4G Module
^^^^^^^^^^^^^^

Not available yet

Resources
----------

`CDC1.2 Protocol Document <https://usb.org/sites/default/files/CDC1.2_WMC1.1_012011.zip>`_

API Reference
----------------

.. include-build-file:: inc/iot_usbh_ecm.inc
