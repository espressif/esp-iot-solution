ADC Two-Point Calibration Scheme
====================================

:link_to_translation:`zh_CN:[中文]`

.. important:: The current two-point ADC calibration scheme is only applicable to ESP32 and ESP32-S2 chips

By default, the ESP32 is calibrated at the factory using a reference voltage. To further improve the consistency of ADC measurement results, users can modify the calibration scheme stored in eFuse to use the two-point calibration method.

Similarly, for the ESP32-S2 chip, the factory default is the two-point calibration method (Method 1). However, this method does not take into account calibration data across different ADC attenuation levels. If users wish to perform calibration using data from different attenuation settings, they can modify the calibration scheme in eFuse to use the two-point calibration method (Method 2).

It is important to note that both of the above methods require users to use the espefuse tool to modify the calibration scheme in eFuse and input the corresponding calibration data. This process must be carried out with extreme caution, as writing incorrect calibration data is irreversible and cannot be modified afterward. This is because data stored in eFuse is permanent and cannot be changed once written. Therefore, ensure the accuracy of the data before proceeding to avoid any unnecessary loss.

To avoid the risks associated with modifying eFuse, the ``adc_tp_calibration`` component provides a method that allows users to perform calibration at the application level. This approach offers the following features:

- Supports inputting calibration parameters at the application level, allowing users to store the calibration data in storage media such as NVS or SD cards.
- Supports the two-point calibration scheme for ESP32 and the Method 2 two-point calibration scheme for ESP32-S2, without interfering with the existing calibration scheme provided by ESP-IDF.

Principle of Two-Point Calibration for ESP32
-----------------------------------------------

The two-point calibration for ESP32 requires applying stable voltages of 150 mV and 850 mV to both ADC1 and ADC2 channels. At each voltage level, raw ADC data should be collected using the ADC raw data acquisition interface to obtain stable readings corresponding to the applied voltages.

+---------------+---------------+---------------+
| Input Voltage | ADC1 (ATTEN0) | ADC2 (ATTEN0) |
+===============+===============+===============+
| 150 mV        | A1            | A2            |
+---------------+---------------+---------------+
| 850 mV        | B1            | B2            |
+---------------+---------------+---------------+

.. note:: When collecting raw ADC data at 150 mV and 850 mV, the ADC driver attenuation must be configured to ``ADC_ATTEN_DB_0``.

Two-Point Calibration Formula for ESP32 ADC:

.. math:: y=\frac{coeff_{a} \ast x + coeff_{around}}{coeff_{scale}} + coeff_{b}

* :math:`coeff_{a}`：:math:`\frac{(850-150)*atten_{scale}[atten]+(B-A)/2}{B-A}`
* :math:`coeff_{b}`：:math:`850-\frac{(850-150)*B+(B-A)/2}{B-A} +atten_{offset}[atten]`
* :math:`coeff_{scale}`：65536
* :math:`coeff_{around}`： :math:`\frac{coeff_{scale}}{2}`

+------------------------+--------+-----------+---------+----------+
| ADC1                   | ATTEN0 | ATTEN_2_5 | ATTEN_6 | ATTEN_12 |
+========================+========+===========+=========+==========+
| :math:`atten_{scale}`  | 65504  | 86975     | 120389  | 224310   |
+------------------------+--------+-----------+---------+----------+
| :math:`atten_{offset}` | 0      | 1         | 27      | 54       |
+------------------------+--------+-----------+---------+----------+

+------------------------+--------+-----------+---------+----------+
| ADC2                   | ATTEN0 | ATTEN_2_5 | ATTEN_6 | ATTEN_12 |
+========================+========+===========+=========+==========+
| :math:`atten_{scale}`  | 65467  | 86861     | 120416  | 224708   |
+------------------------+--------+-----------+---------+----------+
| :math:`atten_{offset}` | 0      | 9         | 26      | 66       |
+------------------------+--------+-----------+---------+----------+

Principle of Two-Point Calibration (Method 2) for ESP32-S2
-------------------------------------------------------------

The two-point calibration (Method 2) for ESP32-S2 requires applying stable voltages of 600 mV, 800 mV, 1000 mV, and 2000 mV to both ADC1 and ADC2 channels. At each voltage level, raw ADC data should be collected using the ADC raw data acquisition interface to obtain stable readings. Taking ADC1 as an example:

+---------------+-----------+-----------------+
| Input Voltage | ATTEN     | ADC1 Raw Value  |
+===============+===========+=================+
| 600 mV        | ATTEN_0   | High[ATTEN_0]   |
+---------------+-----------+-----------------+
| 800 mV        | ATTEN_2_5 | High[ATTEN_2_5] |
+---------------+-----------+-----------------+
| 1000 mV       | ATTEN_6   | High[ATTEN_6]   |
+---------------+-----------+-----------------+
| 2000 mV       | ATTEN_12  | High[ATTEN_12]  |
+---------------+-----------+-----------------+

.. note:: In two-point calibration (Method 2) for ESP32-S2, calibration points must be selected by collecting stable raw ADC data under the corresponding attenuation settings and calibration voltages to ensure accuracy. The High value is used to store the raw ADC data obtained under each specific combination of attenuation and calibration voltage.

The calculation formula for two-point ADC calibration (Method 2) on ESP32-S2 is as follows:

.. math:: y=\frac{x \ast coeff_{a} / (coeff_{ascaling}/coeff_{bscaling}) + coeff_{b} }{coeff_{bscaling}} 

* :math:`coeff_{a}`：:math:`coeff_{ascaling} \ast v_{high}[atten] / High[atten]`
* :math:`coeff_{b}`：0
* :math:`coeff_{ascaling}`：65536
* :math:`coeff_{bscaling}`：1024

+------------------+--------+-----------+---------+----------+
|                  | ATTEN0 | ATTEN_2_5 | ATTEN_6 | ATTEN_12 |
+==================+========+===========+=========+==========+
| :math:`v_{high}` | 600    | 800       | 1000    | 2000     |
+------------------+--------+-----------+---------+----------+

API Reference
-----------------

.. include-build-file:: inc/adc_tp_calibration.inc
