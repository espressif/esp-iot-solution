Humidity and Temperature Sensor
=========================================
:link_to_translation:`zh_CN:[中文]`

The humidity and temperature sensor can be used as a temperature sensor, a humidity sensor or a sensor with both functions. It is mainly used for environmental temperature and humidity detections in smart home, smart farm and smart factory applications.

Adapted Products
---------------------

+----------+-----------------------+-----+-----------+---------------------------------------------------------------------------------------------------------------------------+-----+
|   Name   |       Function        | Bus |  Vendor   |                                                         Datasheet                                                         | HAL |
+==========+=======================+=====+===========+===========================================================================================================================+=====+
| HDC2010  | Temperature, Humidity | I2C | TI        | `HDC2010 Datasheet <https://www.ti.com/lit/gpn/hdc2010>`_                                                                 |     |
+----------+-----------------------+-----+-----------+---------------------------------------------------------------------------------------------------------------------------+-----+
| HTS221   | Temperature, Humidity | I2C | ST        | `HTS221 Datasheet <https://www.st.com/resource/en/datasheet/hts221.pdf>`_                                                 | √   |
+----------+-----------------------+-----+-----------+---------------------------------------------------------------------------------------------------------------------------+-----+
| SHT3X    | Temperature, Humidity | I2C | Sensirion | `SHT3X Datasheet <https://www.mouser.com/datasheet/2/682/Sensirion_Humidity_Sensors_SHT3x_Datasheet_digital-971521.pdf>`_ | √   |
+----------+-----------------------+-----+-----------+---------------------------------------------------------------------------------------------------------------------------+-----+
| MVH3004D | Temperature, Humidity | I2C | --        |                                                                                                                           |     |
+----------+-----------------------+-----+-----------+---------------------------------------------------------------------------------------------------------------------------+-----+

API Reference
---------------------

The following APIs have implemented hardware abstraction on the humidity and temperature sensor. Users can call the code from this layer directly to write a sensor application, or use the sensor interface in :doc:`sensor_hub <sensor_hub>` for easier development.

.. include-build-file:: inc/humiture_hal.inc