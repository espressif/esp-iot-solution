USB Host Solution
------------------

:link_to_translation:`zh_CN:[中文]`

ESP32-S2/S3/P4 chips have built-in USB-OTG peripherals, which support USB host mode. Based on the USB host stack and various USB host class drivers provided by ESP-IDF, they can connect to a variety of USB devices through the USB interface. The following describes the USB Host solutions supported by ESP32-S2/S3/P4 chips.

ESP USB Camera Video Solution
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Supports the connection of a camera module through the USB interface, enabling the acquisition and transmission of MJPEG format video streams, with a maximum resolution of 480*800@15fps（Full-Speed，ESP32-S2/S3） and 1080*1920@60fps（High-Speed，ESP32-P4）. Ideal for applications such as cat's eye doorbells, smart door locks, endoscopes, rearview cameras, and other scenarios.

Features:
~~~~~~~~~~


* Quick Start
* Hot Plug Support
* Cameras that support UVC1.1/1.5 Specifications
* Automatic Descriptor Parsing
* Dynamic Resolution Configuration
* MJPEG Video Stream Transmission
* Bulk or Isochronous Transfer Modes

Hardware:
~~~~~~~~~~


* Chips: ESP32-S2, ESP32-S3
* Peripherals: USB-OTG
* USB Camera: Supports MJPEG format, with a bulk transfer mode of 800*480@15fps, or an isochronous transfer mode of 480*320@15fps. For camera limitations, refer to the `usb_stream API documentation <https://docs.espressif.com/projects/esp-iot-solution/en/latest/usb/usb_host/usb_stream.html>`_.

For ESP32-P4, please use the `usb_host_uvc` component, which supports MJPEG, YUY2, H264, and H265 formats.

links:
~~~~~~~

* `usb_stream component <https://components.espressif.com/components/espressif/usb_stream>`_
* `usb_host_uvc component <https://components.espressif.com/components/espressif/usb_host_uvc>`_
* `usb_stream API reference <https://docs.espressif.com/projects/esp-iot-solution/en/latest/usb/usb_host/usb_stream.html>`_
* `USB Camera Demo video <https://www.bilibili.com/video/BV18841137qT>`_
* Example Code: USB Camera + WiFi Image Transmission: :example:`usb/host/usb_camera_mic_spk`
* Example Code: USB Camera + Local Screen Display with LCD: :example:`usb/host/usb_camera_lcd_display`


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

* Chips: ESP32-S2, ESP32-S3, ESP32-P4
* Peripherals: USB-OTG
* USB Audio Devices: Supports PCM format

Links:
~~~~~~~~

* `usb_stream component <https://components.espressif.com/components/espressif/usb_stream>`_
* `usb_host_uac component <https://components.espressif.com/components/espressif/usb_host_uac>`_
* `usb_stream API reference <https://docs.espressif.com/projects/esp-iot-solution/en/latest/usb/usb_host/usb_stream.html>`_
* `USB Audio Demo video <https://www.bilibili.com/video/BV1LP411975W>`_
* Example Code: MP3 Music Player + USB Headphones: :example:`usb/host/usb_audio_player`

ESP USB 4G Networking Solutions
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Supports connecting 4G Cat.1 or Cat.4 modules via the USB interface, enabling PPP dial-up for internet access. It also supports sharing the internet via Wi-Fi SoftAP hotspot for other devices. Suitable for IoT gateways, MiFi mobile hotspots, smart energy storage, advertising lightboxes, and other scenarios.

Features:
~~~~~~~~~~

* Quick Start
* Hot Plug
* Modem+AT Dual Interface
* PPP Standard Protocol
* 4G to Wi-Fi Hotspot Support
* NAPT (Network Address and Port Translation) Support
* Power Management Support
* Automatic Network Recovery
* SIM Card Detection and Signal Quality Monitoring
* Web-based Configuration Interface

Hardware:
~~~~~~~~~~

* Chips: ESP32-S2, ESP32-S3, ESP32-P4
* Peripherals: USB-OTG
* 4G Modules: Supports Cat.1, Cat.4, and other network standard 4G modules, requiring module support for the PPP/ECM/RNDIS protocol.

Links:
~~~~~~~

* `USB 4G Demo video <https://www.bilibili.com/video/BV1fj411K7bW>`_
* `iot_usbh_modem component <https://components.espressif.com/components/espressif/iot_usbh_modem>`_
* `iot_usbh_ecm 组件 <https://components.espressif.com/components/espressif/iot_usbh_ecm>`_
* `iot_usbh_rndis 组件 <https://components.espressif.com/components/espressif/iot_usbh_rndis>`_
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

* Chips: ESP32-S2, ESP32-S3, ESP32-P4
* Peripherals: USB-OTG
* USB Flash Drive: Formatted as Fat32 by default, with support for USB drives up to 32GB. Drives larger than 32GB require exFAT file system support.

Links:
~~~~~~~

* `usb_host_msc component <https://components.espressif.com/components/espressif/usb_host_msc>`_
* `USB Flash Drive OTA component <https://github.com/espressif/esp-iot-solution/tree/master/components/usb/esp_msc_ota>`_
* `Mount USB Flash Drive + File System Access Example <https://github.com/espressif/esp-idf/tree/master/examples/peripherals/usb/host/msc>`_

ESP USB Hub Solution
^^^^^^^^^^^^^^^^^^^^^

Supports connecting multiple USB devices through USB Hub, enabling simultaneous work of multiple devices. Suitable for multiple USB devices working together, such as dual camera video acquisition, audio and video synchronization processing, peripheral expansion and data storage.

Features:
~~~~~~~~~

* Supports connecting multiple USB devices through USB Hub
* Supports hot plug

Hardware:
~~~~~~~~~

* Chips: ESP32-S2, ESP32-S3, ESP32-P4
* Peripherals: USB-OTG

Links:
~~~~~~~~~~

* `USB Hub Dual Camera Demo <https://github.com/espressif/esp-iot-solution/tree/master/examples/usb/host/usb_hub_dual_camera>`_
