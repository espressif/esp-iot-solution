USB PPP
=============

:link_to_translation:`zh_CN:[中文]`

PPP is one of the most fundamental protocols in networking. The Point-to-Point Protocol (PPP) is a data link layer protocol designed for simple links that transmit packets between peers. Such links provide full-duplex operation and deliver packets in order. PPP provides a common solution for simple connections among hosts, bridges, and routers.

`iot_usbh_modem <https://components.espressif.com/components/espressif/iot_usbh_modem>`_ implements the complete process of PPP dialing on the USB host. It supports connecting 4G Cat.1/4 modules via the USB interface to access the Internet using PPP.

Features:
    * Fast startup
    * Hot-plug support
    * Supports Modem+AT dual interfaces (requires module support)
    * Supports the PPP standard (most 4G modules support it)
    * Supports NAPT network address translation

Supported module models:
-----------------------------

The table below lists the 4G modules already supported. You can add support for your own module following :ref:`example-label`.

.. note::

    The same module may have multiple firmware variants; please consult the vendor to confirm whether PPP dialing is supported.

+-----------+---------------------------------------------+----------+----------+----------+
| Model     | Firmware Version                            | ESP32-S2 | ESP32-S3 | ESP32-P4 |
+===========+=============================================+==========+==========+==========+
| ML302     | ML302-CNLM_MBRH0S00                         | ✅       | ✅       | ✅       |
+-----------+---------------------------------------------+----------+----------+----------+
| Air780E   | AirM2M_780EVT_V2010_LTE_AT                  | ✅       | ✅       | ✅       |
+-----------+---------------------------------------------+----------+----------+----------+
| EC600N-CN | EC600NCNLDR03A03M16_OCP                     | ✅       | ✅       | ✅       |
+-----------+---------------------------------------------+----------+----------+----------+
| EC20      | EC20CEFHLGR06A07M1G                         | ✅       | ✅       | ✅       |
+-----------+---------------------------------------------+----------+----------+----------+
| YM310     | YM310.X09S_AT.A60_R2.1.3.241121             | ✅       | ✅       | ✅       |
+-----------+---------------------------------------------+----------+----------+----------+
| A7600C1   | Model: A7600C1-LNAS\                        | ✅       | ✅       | ✅       |
|           | Revision: A7600M6_V8.18.1                   |          |          |          |
+-----------+---------------------------------------------+----------+----------+----------+
| A7670E    | Model: A7670E-FASE\                         | ✅       | ✅       | ✅       |
|           | Revision: A7670M7_V1.11.1                   |          |          |          |
+-----------+---------------------------------------------+----------+----------+----------+
| SIM7080G  | Revision: 1951B17SIM7080                    | ✅       | ✅       | ✅       |
+-----------+---------------------------------------------+----------+----------+----------+
| LE270-CN  | 12007.6005.00.02.03.04                      | ✅       | ✅       | ✅       |
+-----------+---------------------------------------------+----------+----------+----------+
| MC610-EU  | 16000.1000.00.97.20.10                      | ✅       | ✅       | ✅       |
+-----------+---------------------------------------------+----------+----------+----------+


Add support for a new 4G module
------------------------------------

A 4G module's USB descriptor typically has multiple interfaces, such as a Modem interface, an AT command interface, and a Debug interface. The host can communicate with the cellular communication module via the virtual Modem interface, for example, by sending AT commands for control operations or sending and receiving network data. Normally, the Modem interface switches to data mode to transmit PPP data after a successful dial-up connection, and can return to command mode by sending "+++". Some modules also have a separate AT command interface, allowing data communication and AT command interaction to occur simultaneously. Below are the steps to add support for a new 4G module:

1. Confirm the 4G module **supports USB PPP dialing**;
2. Confirm the **4G SIM card is activated** and network access is enabled;
3. Identify the module's USB descriptor information: ``Vendor ID``, ``Product ID``, the Modem interface number, and the optional AT command interface number. You can use `usbtreeview <https://www.uwe-sieber.de/usbtreeview_e.html>`_ to view them;
4. Fill the above descriptor information into the :c:struct:`usbh_modem_config_t` structure;

.. note::

    The basic AT commands supported by different 4G modules are largely similar, but some special commands may need to be implemented by the user.

.. note::

    Some modules expose the PPP interface not as a standard CDC interface but as a Vendor Specific Class interface; the driver also supports some of these modules.

.. note::

    Some 4G modules can automatically switch from data mode to command mode via PPP frames without sending "+++". In this case, disable the ``MODEM_EXIT_PPP_WITH_AT_CMD`` configuration option.

.. _example-label:

Example Code
-------------------------------

:example:`usb/host/usb_cdc_4g_module`

API Reference
-------------------

.. include-build-file:: inc/iot_usbh_modem.inc
