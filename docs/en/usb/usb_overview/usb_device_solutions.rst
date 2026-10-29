USB Device Solution
--------------------

:link_to_translation:`zh_CN:[中文]`

ESP32-S2/S3/P4/etc. has a USB-OTG peripheral that supports USB device mode, which can be connected to a PC or other USB host devices via USB. By combining the device TinyUSB protocol stack and device class drivers, it can be used to develop various USB devices, such as HID devices, CDC devices, and composite devices. The following describes the related solutions based on USB devices.

USB Audio Device Solution
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The USB audio device solution is based on the UAC 2.0 (USB Audio Class) protocol standard, which allows Espressif SoCs to function as audio devices, providing convenient and high-quality audio transmission capabilities. For example, it can be used as a microphone or speaker connected to a computer or other USB audio-enabled devices for audio input and output.

Features:
~~~~~~~~~

* Supports UAC 2.0
* Supports various audio formats and sampling rates
* Supports audio input and output

Hardware:
~~~~~~~~~

* Chip: ESP32-S2, ESP32-S3, ESP32-P4
* Peripheral: USB-OTG

Links:
~~~~~~

* `ESP-BOX USB Speaker with Display <https://github.com/espressif/esp-box/tree/master/examples/usb_headset>`_

USB Video Device Solution
^^^^^^^^^^^^^^^^^^^^^^^^^^

The USB Video device solution is based on the UVC (USB Video Class) protocol standard, which allows Espressif SoCs to function as video devices, providing convenient and high-quality video transmission capabilities. It can be applied to USB doorbell cameras or USB + Wi-Fi dual-mode network cameras.

Features:
~~~~~~~~~

* Supports UVC 1.5
* Supports synchronous and bulk transfer modes
* Supports functioning as a virtual camera device

Hardware:
~~~~~~~~~

* Chip: ESP32-S2, ESP32-S3, ESP32-P4
* Peripheral: USB-OTG

Links:
~~~~~~

* `USB Network Camera <https://github.com/espressif/esp-iot-solution/tree/master/examples/usb/device/usb_webcam>`_

USB Mass Storage Device Solution
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The USB mass storage device solution is based on the MSC (Mass Storage Class) protocol standard. It can also be combined with Wi-Fi functionality to build wireless shared storage devices such as USB wireless flash drives, card readers, digital music players, and digital media players.

Features:
~~~~~~~~~

* USB-Wi-Fi bidirectional access
* Multiple device connections
* Emulates a USB flash drive

Hardware:
~~~~~~~~~

* Chip: ESP32-S2, ESP32-S3, ESP32-P4
* Peripheral: USB-OTG

Links:
~~~~~~

* `USB + Wi-Fi Wireless Flash Drive <https://github.com/espressif/esp-iot-solution/tree/master/examples/usb/device/usb_msc_wireless_disk>`_

USB HID Device Solution
^^^^^^^^^^^^^^^^^^^^^^^^^^

The USB HID device solution is based on the HID (Human Interface Device) protocol standard. It can function as a USB keyboard, mouse, gamepad, and other devices to enable human-computer interaction. When combined with wireless features such as Wi-Fi, Bluetooth, and ESP-Now, it can also be used to build wireless HID devices.

Features:
~~~~~~~~~

* Supports various HID devices
* Supports custom HID devices
* Supports both USB HID and BLE HID modes

Hardware:
~~~~~~~~~

* Chip: ESP32-S2, ESP32-S3, ESP32-P4
* Peripheral: USB-OTG

Links:
~~~~~~

* `USB HID Keyboard and Mouse Example <https://github.com/espressif/esp-iot-solution/tree/master/examples/usb/device/usb_hid_device>`_
* `USB HID Surface Dial Example <https://github.com/espressif/esp-iot-solution/tree/master/examples/usb/device/usb_surface_dial>`_
* `USB Custom Keyboard Example <https://github.com/espressif/esp-iot-solution/tree/master/examples/keyboard>`_

USB Drag-and-Drop OTA Upgrade
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Based on esp-tinyuf2 as a virtual USB flash drive, this solution supports drag-and-drop UF2 firmware onto the flash drive for OTA updates. It also supports mapping NVS data to files on the flash drive, allowing modification of NVS by modifying the files.

Features:
~~~~~~~~~

* OTA updates by dragging and dropping UF2 firmware
* Modifying NVS through virtual file manipulation

Hardware:
~~~~~~~~~

* Chip: ESP32-S2, ESP32-S3
* Peripheral: USB-OTG

Links:
~~~~~~

* `Read/Write NVS with USB Flash Drive <https://github.com/espressif/esp-iot-solution/tree/master/examples/usb/device/usb_uf2_nvs>`_
* `Virtual USB Flash Drive UF2 OTA Upgrade <https://github.com/espressif/esp-iot-solution/tree/master/examples/usb/device/usb_uf2_ota>`_

USB Extended Screen Solution
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The USB extended screen solution allows a device to function as an additional display through a USB connection. It supports data transmission over a single USB cable, including audio, touch information, and video. This solution can be applied in various scenarios such as computer monitors, signature pads, and extended display setups.

Host-side Driver
~~~~~~~~~~~~~~~~~~~~~~~

Since USB 2.0 does not support HDMI transmission, the host must transmit image data to the device. Currently, this solution only supports the Windows platform, utilizing the Windows driver model, [`IDD (Indirect Display Driver)`](https://learn.microsoft.com/en-us/windows-hardware/drivers/display/indirect-display-driver-model-overview). This driver captures the desktop image from Windows. Due to USB's speed limitations, the image is first compressed into formats like JPEG, and then transmitted through the USB vendor interface. Each image frame is prepended with a 16-byte header containing details such as image width, height, format, length, and compression type.

Features
~~~~~~~~~~~~~~~~~

- Supports image transmission via USB
- Supports audio transmission via USB
- Supports touch data transmission via USB

Hardware
~~~~~~~~~~~~~~

- Chips: ESP32-S2, ESP32-S3, ESP32-P4
- Peripheral: USB-OTG

Links
~~~~~~~~~~

- `P4 USB Extended Screen Example <https://github.com/espressif/esp-iot-solution/tree/master/examples/usb/device/usb_extend_screen>`_
