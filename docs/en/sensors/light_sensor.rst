Ambient Light Sensor
========================
:link_to_translation:`zh_CN:[中文]`

The ambient light sensor can be used as a light intensity sensor, a color sensor, a UV sensor or a sensor with multiple functions.

Adapted Products
---------------------

+------------+---------------------------------+-------+----------+-------------------------------------------------------------------------------------------------------+----------+
| Name       | Function                        | Bus   | Vendor   | Datasheet                                                                                             |HAL       |
+============+=================================+=======+==========+=======================================================================================================+==========+
| BH1750     | Light                           | I2C   | rohm     | `Datasheet <https://www.mouser.com/datasheet/2/348/bh1750fvi-e-186247.pdf>`__                         |  √       |
+------------+---------------------------------+-------+----------+-------------------------------------------------------------------------------------------------------+----------+
| VEML6040   | Light RGBW                      | I2C   | Vishay   | `Datasheet <https://www.vishay.com/docs/84276/veml6040.pdf>`__                                        |  √       |
+------------+---------------------------------+-------+----------+-------------------------------------------------------------------------------------------------------+----------+
| VEML6075   | Light UVA UVB                   | I2C   | Vishay   | `Datasheet <https://cdn.sparkfun.com/assets/3/c/3/2/f/veml6075.pdf>`__                                |  √       |
+------------+---------------------------------+-------+----------+-------------------------------------------------------------------------------------------------------+----------+

API Reference
---------------------

The following APIs have implemented hardware abstraction on the ambient light sensor. Users can call the code from this layer directly to write a sensor application, or use the sensor interface in :doc:`sensor_hub <sensor_hub>` for easier development.

.. include:: /_build/inc/light_sensor_hal.inc
