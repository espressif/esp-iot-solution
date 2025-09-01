USB VID and PID
-----------------

:link_to_translation:`zh_CN:[中文]`

VID and PID are unique identifiers for USB devices, used to distinguish between different USB devices. Generally, VID and PID are assigned by USB-IF, which is the USB Implementers Forum. 

In the following scenarios, you can exempt from applying for VID and PID
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

* If your product operates in USB Host mode, you do not need to apply for VID and PID.
* If your product operates in USB Device mode, and you plan to use Espressif's VID (0x303A) and develop a USB standard device based on the TinyUSB protocol stack, you do not need to apply for a PID. You can use the default PID provided by TinyUSB.

To apply for a VID (Vendor ID) and PID (Product ID)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

If your product requires the use of USB Device mode, you can apply for VID (Vendor ID) or PID (Product ID) following the process outlined below.


* If your product requires the use of a VID assigned by USB-IF, `you need to first register as a USB-IF member <https://www.usb.org/members>`_ and then follow the USB-IF process to apply for VID and PID.
* If your product is considering the use of Espressif's VID, you can directly apply for a PID (free of charge). For the application process, please refer to `Applying for Espressif PID <https://github.com/espressif/usb-pids/blob/main/README.md>`_\ .

Note: Utilizing Espressif's VID and TinyUSB's default PID does not imply compliance with USB specifications. You still need to undergo USB certification for your product.

USB Certification
^^^^^^^^^^^^^^^^^^^^

USB Certification is managed by the USB Implementers Forum (USB-IF) and is designed to ensure that products comply with USB specifications, ensuring interoperability and compatibility between devices.

USB USB certification is an optional process, but it is mandatory in the following scenarios:


* If a product claims to comply with USB specifications and uses the official `USB logo <https://www.usb.org/logo-license>`_\ .
* If a product intends to use the USB logo, trademark, or references USB certification in promotional materials.

For specific certification processes and requirements, please refer to https://www.usb.org or contact the `USB-IF Authorized Testing Laboratory <https://www.usb.org/labs>`_\ .

In addition, for detailed steps and firmware related to USB signal quality testing, please refer to :doc:`USB Signal Quality Test <./usb_signal_quality>`
