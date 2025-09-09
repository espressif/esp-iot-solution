USB RNDIS Host Driver
=======================

:link_to_translation:`zh_CN:[中文]`

.. _label_usb_rndis:

`iot_usbh_rndis <https://components.espressif.com/components/espressif/iot_usbh_rndis>`_ is a host driver based on the RNDIS protocol over the USB interface.

RNDIS Protocol
------------------

RNDIS (Remote NDIS) achieves network communication by encapsulating TCP/IP in USB packets.

USB Descriptors
--------------------

By default, an RNDIS device is the only function in a USB configuration. For composite devices, RNDIS expects to be the first function in the USB configuration, i.e., the interface with ``bInterfaceNumber`` equal to 0.

An RNDIS interface typically consists of one IAD descriptor and two interface descriptors. The current driver supports the following two types of IAD descriptors:

1. ``bFunctionClass`` = 0xe0, ``bFunctionSubClass`` = 0x01, ``bFunctionProtocol`` = 0x03
2. ``bFunctionClass`` = 0xef, ``bFunctionSubClass`` = 0x04, ``bFunctionProtocol`` = 0x01

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
-----------------------

1. Send the initialization message ``REMOTE_NDIS_INITIALIZE_MSG`` via the control endpoint; the device will respond with ``REMOTE_NDIS_INITIALIZE_CMPLT_MSG``.

2. Then the host needs to send ``REMOTE_NDIS_QUERY_MSG`` to query Object Identifiers (OIDs), and use OIDs to query and support the device, such as obtaining the device MAC address, link status, link speed, etc.

3. Next, the host can initiate a DHCP request to obtain an IP address.

Frame Format
----------------

The frame format of the RNDIS protocol is as follows:

Transmit: An RNDIS packet needs the RNDIS data header ``rndis_data_packet_t`` prepended to the Ethernet frame, followed by the Ethernet frame payload.

Receive: An RNDIS packet has the RNDIS data header ``rndis_data_packet_t`` before the Ethernet frame header. Parse the RNDIS data header first, then obtain the Ethernet frame.

Tested 4G Modules
-------------------

+----------------+--------------------------------+----------+----------+----------+---------------------------------------------------------+
| Model          | Firmware Version               | ESP32-S2 | ESP32-S3 | ESP32-P4 | | USB function switch AT commands                       |
|                |                                |          |          |          | | (for reference only; refer to the module manual)      |
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
|                |                                |          |          |          | - Requires AT+GTRNDIS=1,1 to activate dialing           |
+----------------+--------------------------------+----------+----------+----------+---------------------------------------------------------+
| Lierda NT26    | NT26FCNB30WNA-Q01010950        | ✅       | ✅       | ✅       | - AT+ECPCFG="usbNet",0                                  |
|                |                                |          |          |          | - 0:RNDIS 1:ECM                                         |
|                |                                |          |          |          | - Requires AT+ECNETDEVCTL=3,1 to enable auto dialing    |
+----------------+--------------------------------+----------+----------+----------+---------------------------------------------------------+

.. note::

    Some 4G modules need to use AT commands to preconfigure auto dialing in RNDIS mode in order to connect to the network. Please refer to the corresponding module's manual.

Example Code
-------------------------------

:example:`usb/host/usb_rndis_4g_module`

Throughput Test
-------------------

ESP32-S3 connects to a 4G dongle and starts softAP; a phone connects to the softAP for network speed testing:

+----------+--------------+----------------+
|   Chip   | Uplink(Mbps) | Downlink(Mbps) |
+==========+==============+================+
| ESP32-S3 | 5.8          |  7.9           |
+----------+--------------+----------------+

API Reference
------------------

.. include-build-file:: inc/iot_usbh_rndis.inc
