USB Host Solution
------------------

:link_to_translation:`zh_CN:[中文]`

ESP32 series chips with built-in USB-OTG peripherals (see :ref:`USB Peripheral Support <usb_peripheral-section>`) support USB host mode. Based on the USB host stack and various USB host class drivers provided by ESP-IDF, these solutions can quickly enable peripheral expansion, data acquisition, audio/video transmission, network expansion, storage read/write, and other functions through the USB interface. They are widely applicable to smart home, industrial IoT, in-vehicle terminal, smart security, portable device, and other product scenarios. The following sections describe standardized USB Host solutions by category.

ESP USB Camera Video Solution
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Supports connecting camera modules through the USB interface and acquiring video streams in multiple formats, with a maximum resolution of 480*800 @15fps (Full-Speed) and 3840*2160 @30fps (High-Speed). Ideal for applications such as cat's eye doorbells, smart door locks, endoscopes, rearview cameras, and other scenarios.

Features:
~~~~~~~~~~


* Quick Start
* Hot Plug Support
* Cameras that support UVC1.1/1.5 Specifications
* Automatic Descriptor Parsing
* Dynamic Resolution Configuration
* MJPEG/YUV/H.264 Video Stream Transmission
* Bulk or Isochronous Transfer Modes

Hardware:
~~~~~~~~~~


* Chips: ESP32-S2, ESP32-S3, ESP32-S31, ESP32-P4
* Peripherals: USB-OTG
* USB Camera: Compatible with mainstream USB camera modules. Full-Speed interfaces must match corresponding Full-Speed camera devices.

links:
~~~~~~~

* `USB Camera Demo video <https://www.bilibili.com/video/BV18841137qT>`_
* Example Code: USB Camera + WiFi Image Transmission: :example:`usb/host/usb_hub_dual_camera`
* Example Code: USB Camera + Local Screen Display with LCD: :example:`usb/host/usb_camera_lcd_display`
* `usb_host_uvc component <https://components.espressif.com/components/espressif/usb_host_uvc>`_

ESP USB Audio Solution
^^^^^^^^^^^^^^^^^^^^^^^^

Supports connecting USB audio devices through the USB interface, enabling PCM format audio stream acquisition and transmission. It can simultaneously support multiple channels of 48KHz 16bit speakers and multiple channels of 48KHz 16bit microphones. Also supports Type-C interface headphones, suitable for audio playback scenarios. It can operate simultaneously with UVC, making it suitable for scenarios such as doorbell intercoms.

Features:
~~~~~~~~~~

* Quick Start
* Hot Swap
* Automatic Parsing Descriptors
* PCM Audio Format
* Dynamic Modification of Sampling Rate
* Multi-Channel Speakers
* Multi-Channel Microphone
* Support Volume and Mute Control
* Support Simultaneous Work with USB Camera

Hardware:
~~~~~~~~~~

* Chips: ESP32-S2, ESP32-S3, ESP32-S31, ESP32-P4
* Peripherals: USB-OTG
* USB Audio Devices: Supports PCM format

Links:
~~~~~~~~

* `USB Audio Demo video <https://www.bilibili.com/video/BV1LP411975W>`_
* Example Code: MP3 Music Player + USB Headphones: :example:`usb/host/usb_audio_player`
* Example Code: Web demo with microphone, speaker, and camera: :example:`usb/host/usb_camera_mic_spk`
* `usb_host_uac component <https://components.espressif.com/components/espressif/usb_host_uac>`_

ESP USB 4G Networking Solutions
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Supports connecting 4G/5G modules through the USB interface for internet access. It also supports sharing the internet via Wi-Fi SoftAP hotspot for other devices. Suitable for IoT gateways, MiFi mobile hotspots, smart energy storage, advertising lightboxes, and other scenarios.

Features:
~~~~~~~~~~

* Quick Start
* Hot Plug
* Modem + AT Dual Interface (requires module support)
* PPP/ECM/RNDIS Standard Protocols
* 4G to Wi-Fi Hotspot Support
* NAPT (Network Address and Port Translation) Support
* Power Management Support
* SIM Card Detection and Signal Quality Monitoring
* Web-based Configuration Interface

Hardware:
~~~~~~~~~~

* Chips: ESP32-S2, ESP32-S3, ESP32-S31, ESP32-P4
* Peripherals: USB-OTG
* 4G/5G Module: Supports general modules, requiring module support for the PPP/ECM/RNDIS protocol.

Links:
~~~~~~~

* `USB 4G Demo video <https://www.bilibili.com/video/BV1fj411K7bW>`_
* `iot_usbh_modem component <https://components.espressif.com/components/espressif/iot_usbh_modem>`_, :doc:`documentation <../usb_host/usb_ppp>`
* `iot_usbh_ecm component <https://components.espressif.com/components/espressif/iot_usbh_ecm>`_, :doc:`documentation <../usb_host/usb_ecm>`
* `iot_usbh_rndis component <https://components.espressif.com/components/espressif/iot_usbh_rndis>`_, :doc:`documentation <../usb_host/usb_rndis>`
* Example code: 4G PPP dial-up :example:`usb/host/usb_cdc_4g_module`
* Example code: 4G RNDIS :example:`usb/host/usb_rndis_4g_module`
* Example code: 4G ECM :example:`usb/host/usb_ecm_4g_module`

ESP USB Storage Solution
^^^^^^^^^^^^^^^^^^^^^^^^^

Supports connecting standard USB flash drives via the USB interface (compatible with USB 3.1/3.0/2.0 protocols), and can mount the USB flash drive to the FatFS file system for file read and write operations. Suitable for outdoor advertising billboards, attendance machines, mobile speakers, recorders, and other application scenarios.

Features:
~~~~~~~~~~

* Compatible with USB 3.1/3.0/2.0 Flash Drives
* Default Support for Up to 32GB
* Hot Plug
* Support for Fat32/exFAT Formats
* File System Read and Write
* USB Flash Drive Over-The-Air (OTA) Update

Hardware:
~~~~~~~~~~

* Chips: ESP32-S2, ESP32-S3, ESP32-S31, ESP32-P4
* Peripherals: USB-OTG
* USB Flash Drive: Formatted as Fat32 by default, with support for USB drives up to 32GB. Drives larger than 32GB require exFAT file system support.

Links:
~~~~~~~

* `usb_host_msc component <https://components.espressif.com/components/espressif/usb_host_msc>`_
* `USB Flash Drive OTA component <https://github.com/espressif/esp-iot-solution/tree/master/components/usb/esp_msc_ota>`_
* `Mount USB Flash Drive + File System Access Example <https://github.com/espressif/esp-idf/tree/master/examples/peripherals/usb/host/msc>`_


ESP USB HID Solution
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Supports connecting keyboards, mice, barcode scanners, game controllers, custom HID peripherals, and other devices compliant with HID Class through the USB interface, enabling input report reception, device information retrieval, and HID class request control. Suitable for local human-machine interaction, industrial control input, barcode data acquisition, UI pointer/key control, and other scenarios.

Features:
~~~~~~~~~

* Supports automatic HID device enumeration and hot-plug event notification
* Supports receiving HID Input Reports and passing RAW data to the application for parsing
* Supports Boot Protocol keyboard and mouse report structures and common key definitions
* Supports retrieving HID Report Descriptor, VID/PID, manufacturer, product, serial number, and other device information
* Supports HID class requests, including Get/Set Report, Get/Set Idle, and Get/Set Protocol
* Supports multi-interface HID devices, allowing applications to open, start, stop, and close interfaces individually

Hardware:
~~~~~~~~~

* Chips: ESP32-S2, ESP32-S3, ESP32-S31, ESP32-P4
* Peripherals: USB-OTG
* USB HID Device: Supports USB Full-Speed or High-Speed devices compliant with HID Class. For non-Boot Protocol devices, applications need to parse RAW Reports according to the Report Descriptor.

Links:
~~~~~~

* `usb_host_hid component <https://components.espressif.com/components/espressif/usb_host_hid>`_
* `USB HID Host example <https://github.com/espressif/esp-idf/tree/master/examples/peripherals/usb/host/hid>`_

ESP USB Hub Solution
^^^^^^^^^^^^^^^^^^^^^

Supports connecting multiple USB devices through USB Hub, enabling simultaneous work of multiple devices. Suitable for multiple USB devices working together, such as dual camera video acquisition, audio and video synchronization processing, peripheral expansion and data storage.

Features:
~~~~~~~~~

* Supports connecting multiple USB devices through USB Hub
* Supports hot plug

Hardware:
~~~~~~~~~

* Chips: ESP32-S2, ESP32-S3, ESP32-S31, ESP32-P4
* Peripherals: USB-OTG

Links:
~~~~~~~~~~

* `USB Hub Dual Camera Demo <https://github.com/espressif/esp-iot-solution/tree/master/examples/usb/host/usb_hub_dual_camera>`_
