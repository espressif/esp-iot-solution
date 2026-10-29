:link_to_translation:`zh_CN:[中文]`

TinyUSB Application Guide
==========================

This guide contains the following content:

.. contents:: Table of Contents
    :local:
    :depth: 2

Introduction to TinyUSB
------------------------

TinyUSB is an open-source embedded USB Host/Device stack library primarily designed to support USB functionalities on small microcontrollers. Developed and maintained by Adafruit, it aims to provide a lightweight, cross-platform, and easy-to-integrate USB protocol stack. TinyUSB supports various USB device types, including HID (Human Interface Device), MSC (Mass Storage Class), CDC (Communication Device Class), MIDI (Musical Instrument Digital Interface), and more, making it suitable for various embedded systems and IoT devices.Based on the native TinyUSB, the following components have been packaged.

.. figure:: https://dl.espressif.com/AE/esp-iot-solution/tinyusb_components.png
    :align: center
    :alt: TinyUSB Components

    TinyUSB Components

Chip Selection
~~~~~~~~~~~~~~~

.. list-table::
    :widths: 10 30 30
    :header-rows: 1

    * - SoC
      - USB1.1 Full Speed
      - USB2.0 High Speed
    * - ESP32-S2
      - |supported|
      -
    * - ESP32-S3
      - |supported|
      -
    * - ESP32-P4
      - |supported|
      - |supported|

.. |supported| image:: https://img.shields.io/badge/-Supported-green

TinyUSB Components
-------------------

esp_tinyusb Component
~~~~~~~~~~~~~~~~~~~~~~

The `esp_tinyusb <https://components.espressif.com/components/espressif/esp_tinyusb>`_ component wraps a series of TinyUSB APIs, making it easy to integrate USB CDC-ACM, MSC, MIDI, HID, DFU, ECM/NCM/RNDIS classes into your project.

How to Use

    * Run ``idf.py add-dependency esp_tinyusb~1.0.0`` in the project directory to add a dependency on the ``esp_tinyusb`` library.
    * Configure the desired USB class in ``menuconfig``.
    * Use the esp_tinyusb API in your project.

The `esp_tinyusb application examples <https://github.com/espressif/esp-idf/tree/master/examples/peripherals/usb/device>`_ provide examples for USB drives, serial ports, HID devices, composite devices, and more.

    - `USB Composite Device Example <https://github.com/espressif/esp-idf/tree/master/examples/peripherals/usb/device/tusb_composite_msc_serialdevice>`_
    - `USB Serial Device Example <https://github.com/espressif/esp-idf/tree/master/examples/peripherals/usb/device/tusb_serial_device>`_
    - `USB Log Output Example <https://github.com/espressif/esp-idf/tree/master/examples/peripherals/usb/device/tusb_serial_device>`_
    - `USB HID Device Example <https://github.com/espressif/esp-idf/blob/master/examples/peripherals/usb/device/tusb_hid/README.md>`_
    - `USB MIDI Music Device Example <https://github.com/espressif/esp-idf/tree/master/examples/peripherals/usb/device/tusb_midi>`_
    - `USB MSC Device Example <https://github.com/espressif/esp-idf/tree/master/examples/peripherals/usb/device/tusb_msc>`_
    - `USB Network Device Example <https://github.com/espressif/esp-idf/tree/master/examples/peripherals/usb/device/tusb_ncm>`_

.. note::
    The esp_tinyusb library encapsulates many USB classes, making it easy to develop USB classes supported by esp_tinyusb. It is worth noting that this also makes certain modifications difficult. It is suitable for simple USB applications.

.. _espressif/tinyusb:

espressif/tinyusb Component
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The `espressif/tinyusb <https://components.espressif.com/components/espressif/tinyusb>`__ component is based on the original `tinyusb <https://github.com/hathach/tinyusb>`_ repository, encapsulating the tinyusb repository code into a component for easier use in your projects.

.. note::
    This component requires ESP-IDF release/v5.0 or higher.

How to Use:

    * Run ``idf.py add-dependency "tinyusb~0.15.10"`` in the project directory to add a dependency on the ``espressif/tinyusb`` library.
    * Create a ``tusb_config.h`` file that defines a series of macros to configure TinyUSB and provide it to the espressif/tinyusb component. Also, add the following code in the main component's CMakeLists.txt:

        .. code:: C

            idf_component_get_property(tusb_lib espressif__tinyusb COMPONENT_LIB)
            target_include_directories(${tusb_lib} PRIVATE path_to_your_tusb_config)

    * Create a ``usb_descriptor.h`` file that defines the USB device descriptors, including device descriptors, configuration descriptors, interface descriptors, etc.
    * Create a ``usb_descriptors.c`` file that provides USB descriptor callbacks for tinyusb.

espressif/tinyusb application examples:

    - `USB Keyboard and Mouse Device Example <https://github.com/espressif/esp-iot-solution/tree/master/examples/usb/device/usb_hid_device>`_
    - `USB Wireless Drive Example <https://github.com/espressif/esp-iot-solution/tree/master/examples/usb/device/usb_msc_wireless_disk>`_
    - `Windows Surface Dial HID Example <https://github.com/espressif/esp-iot-solution/tree/master/examples/usb/device/usb_surface_dial>`_
    - `Serial to USB Example <https://github.com/espressif/esp-iot-solution/tree/master/examples/usb/device/usb_uart_bridge>`_

.. note::
    The espressif/tinyusb library offers more flexibility, allowing for easier customization of USB devices. It is suitable for complex USB applications. On IDF release/v4.4, you can use the leeebo/tinyusb_src <https://components.espressif.com/components/leeebo/tinyusb_src>_ component, which serves the same purpose as espressif/tinyusb. It mainly supplements the support for ESP-IDF release/v4.4 in espressif/tinyusb.

Introduction to USB Device
---------------------------

USB Audio (UAC)
~~~~~~~~~~~~~~~~

TinyUSB supports the USB `UAC2.0 <http://dl.project-voodoo.org/usb-audio-spec/USB%20Audio%20v2.0/Audio20%20final.pdf>`_ standard for transmitting audio data over USB. It has the following features:

- Supports up to 32-bit/384kHz audio streams
- Compatible with USB1.1 Full Speed and USB2.0 High Speed
- Lower latency

Transmission Methods:
^^^^^^^^^^^^^^^^^^^^^^

UAC only supports synchronous transmission in USB transfers, so UAC audio devices use synchronous endpoints. Because synchronous transmission does not involve retransmission, it has low latency. However, because the transmission between the host and the device is asynchronous, it may cause brief silences or pops. This results in three synchronization methods:

- SYNC Synchronization
    Synchronize the output clock with the SOF packet of each frame.

- Adaptive
    Adjust the output sampling rate according to the host's data transfer rate.

- ASYNC Asynchronous
    Unlike the other two methods, it adds a feedback endpoint. The device informs the host of the subsequent transmission rate based on the current rate, thereby completing data retransmission or under-transmission, without needing to adapt to the host's transmission frequency.

About the Feedback Endpoint for ASYNC Asynchronous Transmission
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Enable the macro ``CFG_TUD_AUDIO_ENABLE_FEEDBACK_EP`` to implement feedback rate calculation. TinyUSB provides multiple feedback data calculation methods, with the FIFO-based feedback calculation (``AUDIO_FEEDBACK_METHOD_FIFO_COUNT``) being the simplest and most practical. The following virtual function needs to be implemented to complete the setup.

.. code:: C

    void tud_audio_feedback_params_cb(uint8_t func_id, uint8_t alt_itf, audio_feedback_params_t* feedback_param)
    {
        (void)func_id;
        (void)alt_itf;
        // Set feedback method to FIFO counting
        feedback_param->method = AUDIO_FEEDBACK_METHOD_FIFO_COUNT;
        feedback_param->sample_freq = s_uac_device->current_sample_rate;

        ESP_LOGD(TAG, "Feedback method: %d, sample freq: %d", feedback_param->method, feedback_param->sample_freq);
    }

The working principle is that the UAC Class maintains a software FIFO with a size of ``CFG_TUD_AUDIO_FUNC_1_EP_OUT_SW_BUF_SZ``. By setting this memory size to 10ms of data, the UAC driver has a buffer area. The driver will maintain the FIFO's water level at half its size through the feedback endpoint. When data is missing, the host will send more data in one packet, and when data is lacking, the host will send less data.

In practical applications, it is recommended to buffer half the FIFO size of data in the UAC internally before starting playback at the beginning of each new audio transmission (e.g., if no data arrives for more than 100ms, it is considered a new audio transmission). This ensures that the I2S always has data to fetch, avoiding pops and noise. The software FIFO's size will remain stable based on the feedback endpoint.

Refer to :doc:`/usb/usb_device/usb_device_uac` for more information.

USB Video (UVC)
~~~~~~~~~~~~~~~~

TinyUSB supports the USB `UVC1.5 <https://www.usb.org/sites/default/files/USB_Video_Class_1_5.zip>`_ standard for transmitting video data over USB, capable of transmitting various video formats, including uncompressed YUV formats, compressed formats like MJPEG, H264, and H265.

Transmission Methods:
^^^^^^^^^^^^^^^^^^^^^^

- When the video streaming interface (USB Video streaming) transmits video, its transmission endpoints are isochronous or bulk endpoints.
- When the video streaming interface transmits still images, its transmission type is bulk endpoints.

Transmitting Images:
^^^^^^^^^^^^^^^^^^^^

UVC can transmit various video formats, which are defined by the video descriptors' Format and Frame.

.. list-table::
    :widths: 10 30 30
    :header-rows: 1

    * - Image Type
      - Format
      - Frame
    * - MJPEG
      - FORMAT_MJPEG: 0x06
      - FRAME_MJPEG: 0x07
    * - YUV2/NV12/M420/I420
      - FORMAT_UNCOMPRESSED: 0x04
      - FRAME_UNCOMPRESSED: 0x05
    * - H264
      - FORMAT_H264: 0x013
      - FRAME_H264: 0x014
    * - H265
      - FORMAT_FRAME_BASED: 0x10
      - FRAME_FRAME_BASED: 0x11

The Frame-based format is special in that it can store any image format as long as the image is stored frame by frame, such as MJPG, H264, H265, etc. The specific image format is indicated by the GUID field.

Dual Camera
^^^^^^^^^^^^^^

In UVC devices, a single physical camera has a VC (video control) descriptor, and a VC descriptor can have multiple VS (video streaming) descriptors, indicating that this camera can transmit multiple image formats. However, in some special hardware, there are two hardware cameras, in which case two VC descriptors are needed.

.. code:: C

    USB Descriptor
        |
        |-- Video Control
        |   |-- Video Streaming
        |
        |-- Video Control
            |-- Video Streaming

Refer to :doc:`/usb/usb_device/usb_device_uvc` for more information.
