**Power Monitor**
==================

:link_to_translation:`zh_CN:[中文]`

Power Monitor IC (Integrated Circuit) is an integrated circuit used for monitoring and managing power supply. It can monitor various parameters of the power supply in real time, such as voltage, current, and power, and provide this information for system use. Power Monitor ICs are crucial in various applications, including computers, power management systems, consumer electronics, industrial control systems, and communication devices.

Adapted Products
-----------------------

+----------+-------------------------------------------------+-----+------------+------------------------------------------------------------------------------------------------------+-----+
|   Name   |                    Function                     | Bus |   Vendor   |                                              Datasheet                                               | HAL |
+==========+=================================================+=====+============+======================================================================================================+=====+
| INA236 |  16-Bit Current, Voltage, and Power Monitor IC    | I2C |     TI     | `INA236 Datasheet <https://www.ti.com/lit/ds/symlink/ina236.pdf?ts=1716462373021>`_                  |  x  |
+----------+-------------------------------------------------+-----+------------+------------------------------------------------------------------------------------------------------+-----+

API Reference
--------------------

The following API implements hardware abstraction for power monitor sensors. Users can directly call this layer of code to write sensor applications.

.. include-build-file:: inc/ina236.inc