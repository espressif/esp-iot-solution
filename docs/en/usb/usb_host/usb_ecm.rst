USB ECM Host Driver
=======================

:link_to_translation:`zh_CN:[中文]`

`iot_usbh_ecm <https://components.espressif.com/components/espressif/iot_usbh_ecm>`_ is a host driver based on the ECM protocol over the USB interface.

ECM Protocol
--------------------

ECM (Ethernet Control Model) is used to exchange Ethernet frame data between a USB device and a host. It uses the CDC class interface to configure and manage various Ethernet features, and achieves network communication by encapsulating Ethernet frames in USB packets.

USB Descriptors
-------------------

According to the USB specification, an ECM interface generally consists of two interface descriptors and class-specific descriptors. In the first interface descriptor, ``bInterfaceClass`` is ``0x02``, and ``bInterfaceSubClass`` is ``0x06``. In the ECM functional descriptor ``usb_ecm_function_desc_t``, ``iMACAddress`` is the string index of the MAC address, and ``wMaxSegmentSize`` is the maximum Ethernet frame length (typically 1514).

After the ECM driver is installed, it automatically scans the interface descriptors of newly attached USB devices, finds the interface that conforms to the ECM protocol, and performs parsing and initialization. After insertion, devices typically send network link status changes through the notify endpoint, and the driver handles the status accordingly.
When a device does not send notifications, and the macro ``ECM_AUTO_LINK_UP_AFTER_NO_NOTIFICATION`` is enabled, the driver will automatically set the network link status to connected after ``ECM_AUTO_LINK_UP_WAIT_TIME_MS`` milliseconds to be compatible with some ECM devices that do not send notifications. This option is enabled by default.

.. Note::
    Some ECM devices do not use the default configuration descriptor config 1, so you need to enable the macro :c:macro:`CONFIG_USB_HOST_ENABLE_ENUM_FILTER_CALLBACK` and implement the enumeration filter callback function.

Example descriptor:

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

Tested ECM Devices
-----------------------

USB-to-Ethernet Adapters
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

+-----------+------------------------+------------------------+-----------------------------+
|  Chip     |          ESP32-S2      |          ESP32-S3      |      ESP32-P4               |
+===========+========================+========================+=============================+
| CH397A    |     ✅                 |     ✅                 |       ✅                    |
+-----------+------------------------+------------------------+-----------------------------+
| RTL8152B  |     ✅                 |     ✅                 |       ✅                    |
+-----------+------------------------+------------------------+-----------------------------+
| NX7202D   |     ✅                 |     ✅                 |       ✅                    |
+-----------+------------------------+------------------------+-----------------------------+

4G Modules
^^^^^^^^^^^^^^

+-------------+--------------------------------+----------+----------+----------+---------------------------------------------------------+
| Model       | Firmware Version               | ESP32-S2 | ESP32-S3 | ESP32-P4 |USB function switch AT cmds (ref only; see module manual)|
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
|             |                                |          |          |          | - Requires AT+GTRNDIS=1,1 to activate dialing           |
+-------------+--------------------------------+----------+----------+----------+---------------------------------------------------------+
| Lierda NT26 | NT26FCNB30WNA-Q01010950        | ✅       | ✅       | ✅       | - AT+ECPCFG="usbNet",1                                  |
|             |                                |          |          |          | - 0:RNDIS 1:ECM                                         |
|             |                                |          |          |          | - Requires AT+ECNETDEVCTL=3,1 to enable auto dialing    |
+-------------+--------------------------------+----------+----------+----------+---------------------------------------------------------+

.. note::
   ✅ indicates supported; ❌ indicates endpoint size not supported (because these 4G modules have endpoint MPS exceeding the standard 64 bytes at Full Speed).

.. note::

    Some 4G modules need to use AT commands to preconfigure auto dialing in ECM mode in order to connect to the network. Please refer to the corresponding module's manual.

FAQ
-----------------

1. Ping times out after the 4G module obtains an IP address. How to resolve this?

    The network is not connected. Check if automatic dialing in ECM mode is activated. Please refer to the corresponding module's documentation for details. Check if the SIM card is inserted correctly and registered with the network.

2. The 4G module prints "No ECM interface found for device VID: xxxx, PID: xxxx, ignore it" after connecting to USB.

    The ECM interface was not detected. Please check if the module has been correctly set to ECM mode. For details, please refer to the documentation manual for the corresponding module.

3. After the 4G module is connected to the USB, it prints "E (887) HCD DWC: EP MPS (512) exceeds supported limit (408)".

    The USB endpoint MPS of this module exceed the size allowed by the ESP chip hardware. Please try using the :ref:`RNDIS example <label_usb_rndis>` or replace it with another 4G module.

Example Code
-------------------------------

:example:`usb/host/usb_ecm_4g_module`

Downloads
--------------

`CDC 1.2 specification <https://usb.org/sites/default/files/CDC1.2_WMC1.1_012011.zip>`_

API Reference
-----------------

.. include-build-file:: inc/iot_usbh_ecm.inc
