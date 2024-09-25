ADC Range Extension Solution
================================
:link_to_translation:`zh_CN:[中文]`

ESP32-S3 ADC Range Extension
------------------------------------

The maximum effective range of the ESP32-S3 ADC is 0 ~ 3100 mV. Through the external voltage divider circuit, it can meet most of the functions such as the ADC button or battery voltage detection. However, for applications such as NTC (Negative Temperature Coefficient) based temperature measurement, it may need to support full-scale (0 ~ 3300 mV) measurement.
ESP32-S3 can adjust the ADC offset through registers, and combined with the nonlinear compensation method of the high voltage area, the expansion of the ADC range can be implemented.

The process is as follows:

1. Measure the first voltage value using the default offset
2. If the measured voltage is less than 2900 mV, the first voltage is directly output as the measurement result
3. Else if the measured voltage is greater than 2900 mV, increase the offset value to take the secondary measurement. Then carried out the nonlinear correction calculation on the secondary value, will be output as the final measurement result.
4. Restore the offset value once measurement is completed

Overall, during each ADC measurement, there will be 1-2 times ADC reading. For most application scenarios, the measurement delay introduced by this scheme is negligible.

Patch Use Guide
-------------------

How to Apply a Patch Based on ESP-IDF ``v4.4.8``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

1. Please make sure ESP-IDF has been ``checked out`` to the ``v4.4.8``
2. Please download file :download:`esp32s3_adc_range_to_3100.patch <../../_static/esp32s3_adc_range_to_3100.patch>` to anywhere you want
3. Using command ``git am --signoff < esp32s3_adc_range_to_3100.patch`` to apply the patch to ESP-IDF

How to Apply a Patch Based on ESP-IDF ``v5.3.1``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

1. Please make sure ESP-IDF has been ``checked out`` to the ``v5.3.1``
2. Please download file :download:`esp32s3_adc_range_to_3100_v531.patch <../../_static/esp32s3_adc_range_to_3100_v531.patch>` to anywhere you want
3. Using command ``git am --signoff < esp32s3_adc_range_to_3100_v531.patch`` to apply the patch to ESP-IDF

API Guide
-------------

The method to obtain the voltage value after ADC range extension varies for different versions of ESP-IDF:

- ESP-IDF ``v4.4.8``

  1. To get the range expansion result, users must directly use ``esp_adc_cal_get_voltage`` to get the voltage of ``ADC1`` or ``ADC2``.
  2. Other APIs of ESP-IDF ``v4.4.8`` ADC are not affected, and the read results are consistent with the default results


- ESP-IDF ``v5.3.1``

  1. To get the range expansion result, users must directly use ``adc_oneshot_get_calibrated_result`` to get the voltage of ``ADC1`` or ``ADC2``.
  2. Other APIs of ESP-IDF ``v5.3.1`` ADC are not affected, and the read results are consistent with the default results
