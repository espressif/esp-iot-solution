USB RNDIS Host Driver
==========================

:link_to_translation:`zh_CN:[中文]`

``iot_usbh_rndis`` is a host driver for the RNDIS protocol based on USB interface.

RNDIS Protocol
---------------

RNDIS (Remote NDIS) enables network communication by encapsulating TCP/IP in USB packets.

USB Descriptors
-----------------

RNDIS devices default to being the only function in a USB configuration. For composite devices, RNDIS expects to be the first USB configuration, i.e. interface with ``bInterfaceNumber`` 0.

The RNDIS interface typically consists of one IAD descriptor and two interface descriptors. The IAD descriptor has ``bInterfaceCount`` of 2 and ``bFunctionClass`` of 0xe0.

Example:

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

Communication Flow
-------------------

1. Send initialization message ``REMOTE_NDIS_INITIALIZE_MSG`` through control endpoint, device will return ``REMOTE_NDIS_INITIALIZE_CMPLT_MSG`` message.

2. Then host needs to send ``REMOTE_NDIS_QUERY_MSG`` message to query Object Identifier (OID), and use OID to query and support the device. For example, getting device MAC address, connection status, connection speed etc.

3. Then host can initiate DHCP request to obtain IP address.

Frame Format
--------------

The RNDIS protocol frame format is as follows:

Send: RNDIS data packets need to add RNDIS data header ``rndis_data_packet_t`` before the Ethernet frame, then add the Ethernet frame message.

Receive: RNDIS data packets add RNDIS data header ``rndis_data_packet_t`` at the beginning of Ethernet frame. Need to parse RNDIS data header first, then get the Ethernet frame.

Speed Test
------------

ESP32-S3 connected to 4G network card and started softAP, mobile phone connected to softAP for network speed test:

+----------+--------------+----------------+
|   Chip   | Upload(Mbps) | Download(Mbps) |
+==========+==============+================+
| ESP32-S3 | 5.8          | 7.9            |
+----------+--------------+----------------+

API Reference
--------------

.. include-build-file:: inc/iot_usbh_rndis.inc
