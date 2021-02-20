:orphan:

Current Consumption Test for ESP32 in Deep sleep
================================================

:link_to_translation:`zh_CN:[中文]`

ESP32 has 18 RTC GPIOs and 10 touchpads, which can all be configured as
the wake-up source to wake up the chip from deep sleep.

Description
-----------

-  **Purpose**: measure the current consumptions of ESP32 in Deep\_sleep
   mode, under different wake-up setups;
-  **Tool**: ESP32\_ULP\_EB V1 evaluation board. 
-  **Scope**: The current consumption of all the RTC GPIOs and touchpads
   of ESP32 are measured in this test, except for RTC GPIO\_37 and RTC
   GPIO\_38.

.. note:: GPIO_37 and GPIO_38 are not tested here, becuase these two pads are not led out to the pin header of the board.

RTC GPIO
--------

External events can trigger level signals of the RTC GPIO, which has
been configured as the wake-up source of the chip, and wake up the chip
from deep sleep.

**Wake-up source**

-  EXT(0): wake up the chip when a specific GPIO pad, which has been
   configured as the wake-up source of the chip, meets certain
   requirement regarding electrical level. This GPIO can be configured
   as "high-level" triggered or "low-level" triggered;

-  EXT(1): wake up the chip when a set of specified GPIO pads, which
   have been configured as the wake-up sources of the chip, all meet
   certain requirements regarding electrical level. One GPIO can also be
   seen as a set of specified GPIOs (In this case, EXT(1) has no
   difference with EXT(0)).

**Configuration**

-  Set a RTC GPIO as the wake-up source of the chip, and configure it to
   *input floating* mode;
-  Resistors:

   -  when the GPIO is configured as high-level triggered, a 10 kΩ
      pull-down resistor should be used;
   -  when the GPIO is configured as low-level triggered, a 10 kΩ
      pull-up resistor should be used;

.. Note:: To achieve lower power consumption, external resistors, instead of internal ones, are recommanded in this case.

**Test Result**

-  **Wake-up source: EXT(0)**

+------------+------------------------+------------------------+
| GPIO NUM   | Low-level triggerred   | High-level triggered   |
+============+========================+========================+
| GPIO\_0    | 6.3 uA                 | 6.2 uA                 |
+------------+------------------------+------------------------+
| GPIO\_2    | 6.3 uA                 | 6.2 uA                 |
+------------+------------------------+------------------------+
| GPIO\_4    | 6.4 uA                 | 6.2 uA                 |
+------------+------------------------+------------------------+
| GPIO\_12   | 6.4 uA                 | 6.4 uA                 |
+------------+------------------------+------------------------+
| GPIO\_13   | 6.3 uA                 | 6.3 uA                 |
+------------+------------------------+------------------------+
| GPIO\_14   | 6.3 uA                 | 6.3 uA                 |
+------------+------------------------+------------------------+
| GPIO\_15   | 6.4 uA                 | 6.4 uA                 |
+------------+------------------------+------------------------+
| GPIO\_25   | 6.3 uA                 | 6.5 uA                 |
+------------+------------------------+------------------------+
| GPIO\_26   | 6.6 uA                 | 6.3 uA                 |
+------------+------------------------+------------------------+
| GPIO\_27   | 6.4 uA                 | 6.4 uA                 |
+------------+------------------------+------------------------+
| GPIO\_32   | 6.4 uA                 | 6.4 uA                 |
+------------+------------------------+------------------------+
| GPIO\_33   | 6.4 uA                 | 6.4 uA                 |
+------------+------------------------+------------------------+
| GPIO\_34   | 6.4 uA                 | 6.2 uA                 |
+------------+------------------------+------------------------+
| GPIO\_35   | 6.4 uA                 | 6.3 uA                 |
+------------+------------------------+------------------------+
| GPIO\_36   | 6.4 uA                 | 6.3 uA                 |
+------------+------------------------+------------------------+
| GPIO\_37   |                        |                        |
+------------+------------------------+------------------------+
| GPIO\_38   |                        |                        |
+------------+------------------------+------------------------+
| GPIO\_39   | 6.4 uA                 | 6.3 uA                 |
+------------+------------------------+------------------------+

-  **Wake-up source: EXT(1)**

+------------+-----------------------+------------------------+
| GPIO NUM   | Low-level triggered   | High-level triggered   |
+============+=======================+========================+
| GPIO\_0    | 5.2 uA                | 5.3 uA                 |
+------------+-----------------------+------------------------+
| GPIO\_2    | 5.2 uA                | 5.2 uA                 |
+------------+-----------------------+------------------------+
| GPIO\_4    | 5.2 uA                | 5.2 uA                 |
+------------+-----------------------+------------------------+
| GPIO\_12   | 5.2 uA                | 5.3 uA                 |
+------------+-----------------------+------------------------+
| GPIO\_13   | 5.3 uA                | 5.2 uA                 |
+------------+-----------------------+------------------------+
| GPIO\_14   | 5.3 uA                | 5.3 uA                 |
+------------+-----------------------+------------------------+
| GPIO\_15   | 5.3 uA                | 5.2 uA                 |
+------------+-----------------------+------------------------+
| GPIO\_25   | 5.2 uA                | 5.3 uA                 |
+------------+-----------------------+------------------------+
| GPIO\_26   | 5.3 uA                | 5.2 uA                 |
+------------+-----------------------+------------------------+
| GPIO\_27   | 5.3 uA                | 5.3 uA                 |
+------------+-----------------------+------------------------+
| GPIO\_32   | 5.3 uA                | 5.3 uA                 |
+------------+-----------------------+------------------------+
| GPIO\_33   | 5.3 uA                | 5.3 uA                 |
+------------+-----------------------+------------------------+
| GPIO\_34   | 5.3 uA                | 5.7 uA                 |
+------------+-----------------------+------------------------+
| GPIO\_35   | 5.3 uA                | 5.7 uA                 |
+------------+-----------------------+------------------------+
| GPIO\_36   | 5.3 uA                | 5.3 uA                 |
+------------+-----------------------+------------------------+
| GPIO\_37   |                       |                        |
+------------+-----------------------+------------------------+
| GPIO\_38   |                       |                        |
+------------+-----------------------+------------------------+
| GPIO\_39   | 5.4 uA                | 5.5 uA                 |
+------------+-----------------------+------------------------+

**Conclusion**

-  The current consumptions of the chip are basically the same and
   extremely low during the deep sleep, when different RTC GPIOs are
   configured as the wake-up sources；
-  The current consumptions of the chip in EXT1 mode are about 1 uA
   lower than that in EXT0 mode.

+-------------------+-----------------------+
| Pad Num           | Current               |
+===================+=======================+
| Pad0 (GPIO\_4)    | 37.3 uA               |
+-------------------+-----------------------+
| Pad1 (GPIO\_0)    | 35.7 uA               |
+-------------------+-----------------------+
| Pad2 (GPIO\_2)    | 36.6 uA               |
+-------------------+-----------------------+
| Pad3 (GPIO\_15)   | 35.6 uA               |
+-------------------+-----------------------+
| Pad4 (GPIO\_13)   | 36.5 uA               |
+-------------------+-----------------------+
| Pad5 (GPIO\_12)   | 36.1 uA               |
+-------------------+-----------------------+
| Pad6 (GPIO\_14)   | 36.7 uA               |
+-------------------+-----------------------+
| Pad7 (GPIO\_27)   | 35.7 uA               |
+-------------------+-----------------------+
| Pad8 (GPIO\_33)   | 36.7 uA               |
+-------------------+-----------------------+
| Pad9 (GPIO\_32)   | 36.3 uA               |
+-------------------+-----------------------+

Touchpad
--------

Touchpad can be enabled as the wake-up source to wake up the chip from
deep sleep.

**Configuration**

-  Set a touchpad as the wake-up source of the chip, and initialize this
   pad;
-  Set up the touchpad's trigger threshold.

**Test Result**

-  **Wake-up source: touchpad**

+-------------------+-------------------------+
| Pad Num           | Current                 |
+===================+=========================+
| Pad0 (GPIO\_4)    | 37.3 uA                 |
+-------------------+-------------------------+
| Pad1 (GPIO\_0)    | 35.7 uA                 |
+-------------------+-------------------------+
| Pad2 (GPIO\_2)    | 36.6 uA                 |
+-------------------+-------------------------+
| Pad3 (GPIO\_15)   | 35.6 uA                 |
+-------------------+-------------------------+
| Pad4 (GPIO\_13)   | 36.5 uA                 |
+-------------------+-------------------------+
| Pad5 (GPIO\_12)   | 36.1 uA                 |
+-------------------+-------------------------+
| Pad6 (GPIO\_14)   | 36.7 uA                 |
+-------------------+-------------------------+
| Pad7 (GPIO\_27)   | 35.7 uA                 |
+-------------------+-------------------------+
| Pad8 (GPIO\_33)   | 36.7 uA                 |
+-------------------+-------------------------+
| Pad9 (GPIO\_32)   | 36.3 uA                 |
+-------------------+-------------------------+

.. Note:: touch_pad_set_meas_time can be used to adjust the charging/discharging cycle and the detection period of the touch sensor accordingly, so as to optimize the response time and achieve even lower power consumption.
