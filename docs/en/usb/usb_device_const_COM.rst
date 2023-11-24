
Prevent Windows from incrementing COM numbers based on USB device serial number
--------------------------------------------------------------------------------

Due to the fact that any device connected to a Windows PC is identified by its VID (Vendor ID), PID (Product ID), and Serial number. If any of these three parameters undergo a change, the PC will detect new hardware and assigns it to a different COM port. For more details, please refer to the `Windows USB device registry entries <https://learn.microsoft.com/en-us/windows-hardware/drivers/usbcon/usb-device-specific-registry-settings>`_.

In the ESP ROM Code, the configuration of USB descriptors is as follows:

.. list-table::
   :header-rows: 1

   * -
     - ESP32S2
     - ESP32S3
     - ESP32C3
   * - **VID**
     - 0x303a
     - 0x303a
     - 0x303a
   * - **PID**
     - 0x0002
     - 0x1001
     - 0x1001
   * - **Serial**
     - 0
     - MAC address
     - MAC address

* For ESP32S2 with USB On-The-Go (usb-otg), the Serial is a constant 0, and it remains the same for every device, resulting in consistent COM port numbers.
* For ESP32S3 (usb-serial-jtag) and ESP32C3 (usb-serial-jtag), the Serial is set to the device MAC address. Each device has a unique MAC address, leading to different COM port numbers by default with incremental assignment.

Incrementing COM numbers pose additional challenges for mass production programming. For customers requiring firmware downloads via USB, it is recommended to modify the Windows rules for incrementing the COM number to prevent incrementing the number based on the Serial number.

Solution
^^^^^^^^^^^^^

**Open Command Prompt in Administrator Mode** ``on Windows``, and execute the following command. This will add a registry entry to prevent incremental numbering based on the Serial number. **After completing the setup, please restart your computer to apply the changes**.

.. code-block:: shell

   REG ADD HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Control\usbflags\303A10010101 /V IgnoreHWSerNum /t REG_BINARY /d 01

Users have the option to download the script :download:`ignore_hwserial_esp32s3c3.bat <../../_static/ignore_hwserial_esp32s3c3.bat>` , then run it with administrative privileges by right-clicking and selecting **Run as Administrator**.