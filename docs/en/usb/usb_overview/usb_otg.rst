
USB-OTG Peripheral Introduction
--------------------------------

:link_to_translation:`zh_CN:[中文]`

The ESP32-S2/S3/P4 and other chips come with built-in USB On-The-Go (USB-OTG) peripherals. They include a USB controller and USB PHY, supporting connection to a PC via a USB cable, enabling USB Host and USB Device functionalities.

USB-OTG Transfer Rate
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

For the ESP32-S2/S3/P4, the USB On-The-Go (USB-OTG) Full Speed bus transfer rate is 12 Mbps. However, due to the presence of checksum and synchronization mechanisms in USB transfers, the actual effective transfer rate will be lower than 12 Mbps. The specific values depend on the transfer type and are outlined in the table below:

.. list-table::
   :header-rows: 1

   * - **Transfer Types**
     - **Control**
     - **Interrupt**
     - **Bulk**
     - **Isochronous**
   * - Application Scenarios
     - Device Initialization and Management
     - Mouse and Keyboard
     - Printers and Bulk Storage
     - Streaming Audio and Video
   * - Supports Low Speed
     - Yes
     - Yes
     - No
     - No
   * - Checksum and Retransmission
     - Yes
     - Yes
     - Yes
     - No
   * - Ensuring Transfer Speed
     - No
     - No
     - No
     - Yes
   * - Utilizing Fixed Bandwidth
     - Yes (10%)
     - Yes (90%)
     - No
     - Yes (90%)
   * - Reducing Latency Time
     - No
     - Yes
     - No
     - Yes
   * - Maximum Transfer Size
     - 64 Bytes
     - 64 Bytes
     - 64 Bytes
     - ~512 Bytes
   * - Number of Packets Transferred Per Millisecond
     - *
     - 1
     - 19
     - 1
   * - Theoretical Effective Rate
     - *
     - 64000 Bytes/s
     - 1216000 Bytes/s
     - 512000 Bytes/s


..

For ESP32-P4 USB-OTG High Speed bus transfer rate is 480 Mbps, compared to Full Speed, there are some differences in the maximum transfer size, number of packets transferred per millisecond, and theoretical effective rate, as shown in the table below:

.. list-table::
   :header-rows: 1

   * - **Transfer Types**
     - **Control**
     - **Interrupt**
     - **Bulk**
     - **Isochronous**
   * - Maximum Transfer Size
     - 64 Bytes
     - 1024 Bytes
     - 512 Bytes
     - 1024 Bytes
   * - Number of Packets per Microframe
     - *
     - 6
     - 13
     - 7
   * - Theoretical Effective Rate
     - *
     - 49152000 Bytes/s
     - 53248000 Bytes/s
     - 57344000 Bytes/s
..


   * Calculation Formula for Transfer Rate: Transfer Rate (Bytes/s) = Maximum Packet Size * Number of Packets per Microframe * 8000
   * Control transfers are used for transmitting device control information and involve multiple stages. The effective transfer rate needs to be calculated based on the implementation of the protocol stack.


USB-OTG Peripheral Built-in Features
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Using USB OTG Console for Firmware Download and Logging
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

For chips like ESP32-S2/S3 with built-in USB On-The-Go (USB-OTG) peripherals, the ROM Code contains the functionality of USB Communication Device Class (CDC). This feature can be utilized as an alternative to UART interfaces, enabling functions such as logging, console access, and firmware downloads.


#. Since the USB OTG Console is initially disabled, follow these steps to perform the first firmware download:

   #. Enable the USB OTG Console feature in ``menuconfig`` and then compile the firmware.
   #. Manually pull down the ``Boot`` pin of the chip and connect the chip to the PC via USB to enter download mode. The PC will detect a new serial port device, listed as ``COM*`` on Windows, ``/dev/ttyACM*`` on Linux, and ``/dev/cu*`` on MacOS.
   #. Use the ``esptool`` utility (or directly use ``idf.py flash``) to configure the corresponding serial port and download the firmware.

#. After the initial download, the USB OTG Console functionality will be automatically enabled. You can then connect to the PC via USB, and the PC will detect a new serial port device, listed as ``COM*`` on Windows, ``/dev/ttyACM*`` on Linux, and ``/dev/cu*`` on MacOS. Log data will be printed from this virtual serial port.

#. Users no longer need to manually pull down the Boot control pin. To download firmware, use the esptool utility (or directly use idf.py flash) to configure the corresponding serial port. During the download, esptool will automatically reset the device and switch it to download mode using the USB control protocol.

..

   For more detailed information, please refer to `USB OTG Console <https://docs.espressif.com/projects/esp-idf/en/latest/esp32s2/api-guides/usb-otg-console.html>`_


Download firmware using USB OTG DFU
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

For chips like ESP32-S2/S3/P4 with built-in USB On-The-Go (USB-OTG) peripherals, the ROM Code contains the functionality of USB DFU (Device Firmware Upgrade). This feature enables the implementation of a standard DFU download mode.


#. To download firmware using DFU, users need to manually enter download mode each time by pulling down the Boot control pin of the chip and connecting it to the PC via USB.
#. Run the command ``idf.py dfu`` in the project directory to generate the DFU firmware, and then use ``idf.py dfu-flash`` to download the firmware.
#. If there are multiple DFU devices, users can use ``idf.py dfu-list`` to view the DFU device list and then use ``idf.py dfu-flash --path <path>`` to specify the download port.

..

   For more detailed information, please refer to `Device Firmware Upgrade via USB <https://docs.espressif.com/projects/esp-idf/en/latest/esp32s2/api-guides/dfu.html>`_\ .


USB Host development using USB-OTG peripherals
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

USB-OTG peripherals support USB Host functionality, allowing users to connect directly to external USB devices through the USB interface. Starting from ESP-IDF version 4.4, the USB Host Driver is supported. Users can refer to the `ESP-IDF USB Host documentation <https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/usb_host.html>`_ to develop USB Class Drivers.

Additionally, Espressif officially supports USB Host drivers for HID, MSC, CDC, UVC, and other device classes. Users can utilize these drivers directly for application development.

For more details on the USB Host solution, please refer to :doc:`USB Host Solution <./usb_host_solutions>`.

USB Device development using USB-OTG peripherals
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

USB-OTG peripherals support USB Device functionality, and Espressif has officially adapted the ``TinyUSB`` stack. Users can develop USB standard devices or custom devices based on the open-source ``TinyUSB``, such as HID, MSC, CDC, ECM, UAC, and more.

For more details on the USB Device solution, please refer to :doc:`USB Device Solution <./usb_device_solutions>`.

Dynamic Switching of USB-OTG Peripheral Modes
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The USB-OTG peripheral supports dynamic switching of its mode. Users can achieve dynamic switching by dynamically registering the USB Host Driver or USB Device Driver.

* Example Code: USB OTG Manual Switching :example:`usb/otg/usb_host_device_mode_manual_switch`

Using USB-OTG to Proactively Disconnect from the Host
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

You can proactively disconnect from the host by pulling the USB OTG ``VBUSVALID`` signal low, simulating the disconnection of the USB OTG peripheral. Note that if you need to reattach the device, you must wait at least **10ms** before doing so. Please refer to :doc:`USB Device Self Power <./usb_device_self_power>`.
