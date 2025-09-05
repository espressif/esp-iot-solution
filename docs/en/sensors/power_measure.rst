**Power Measure**
==================

:link_to_translation:`zh_CN:[中文]`

Power Measure IC (Integrated Circuit) is an integrated circuit used for monitoring and managing power supply. It can monitor various parameters of the power supply in real time, such as voltage, current, and power, and provide this information for system use. Power Measure ICs are crucial in various applications, including computers, power management systems, consumer electronics, industrial control systems, and communication devices.

This example demonstrates how to use the BL0937 and INA236 power measurement chips to detect electrical parameters such as voltage, current, active power, and energy(BL0937 only) consumption.

Features
--------

* Measures **voltage**, **current**, **active power**, and **energy**.
* Supports both **BL0937** and **INA236** power measurement chips.
* Configurable chip selection via `DEMO_BL0937` and `DEMO_INA236` macros.
* Supports overcurrent, overvoltage, and undervoltage protection.
* Energy detection is enabled for accurate readings (BL0937 only).
* Real-time measurements for INA236, cached measurements for BL0937.
* Regularly fetches power readings every second and logs them.

How It Works
------------

The `app_main()` function in `main/power_measure_example.c` demonstrates the handle-based factory API:

1. **(Optional) Initialize I2C bus (INA236 only)**:

   - Use `i2c_bus_create()` with `I2C_MASTER_SCL_IO`/`I2C_MASTER_SDA_IO` to create the I2C bus.
2. **Create a power measurement device**:

   - Prepare common config `power_measure_config_t` (overcurrent/overvoltage/undervoltage thresholds, enable energy detection).
   - For BL0937, prepare `power_measure_bl0937_config_t` (CF/CF1/SEL pins, divider/sampling resistors, KI/KU/KP factors).
   - For INA236, prepare `power_measure_ina236_config_t` (I2C bus, address, alert config).
   - Call `power_measure_new_bl0937_device()` or `power_measure_new_ina236_device()` to get a `power_measure_handle_t`.
3. **Fetch measurements periodically**:

   - Use `power_measure_get_voltage/current/active_power/power_factor()` as needed;
   - `power_measure_get_energy()` returns valid data only on chips that support energy (e.g., BL0937);
   - The example logs values every 1 second.
4. **Error handling**:

   - If any API returns a non-`ESP_OK` value, log the error with `ESP_LOGE`.
5. **Resource cleanup**:

   - Release the device with `power_measure_delete(handle)`;
   - For INA236, also release the I2C bus with `i2c_bus_delete(&i2c_bus)`.

Code Snippets
-------------

BL0937 (GPIO)
~~~~~~~~~~~~

.. code-block:: c

   #include "power_measure.h"
   #include "power_measure_bl0937.h"

   power_measure_config_t common = {
       .overcurrent = 2000,
       .overvoltage = 250,
       .undervoltage = 180,
       .enable_energy_detection = true,
   };

   power_measure_bl0937_config_t bl = {
       .sel_gpio = GPIO_NUM_4,
       .cf1_gpio = GPIO_NUM_7,
       .cf_gpio = GPIO_NUM_3,
       .pin_mode = 0,
       .sampling_resistor = 0.001f,
       .divider_resistor = 1981.0f,
       .ki = 2.75f, .ku = 0.65f, .kp = 15.0f,
   };

   power_measure_handle_t h;
   ESP_ERROR_CHECK(power_measure_new_bl0937_device(&common, &bl, &h));

   float u,i,p,e;
   power_measure_get_voltage(h, &u);
   power_measure_get_current(h, &i);
   power_measure_get_active_power(h, &p);
   power_measure_get_energy(h, &e);

   power_measure_delete(h);

INA236 (I2C)
~~~~~~~~~~~~

.. code-block:: c

   #include "power_measure.h"
   #include "power_measure_ina236.h"
   #include "i2c_bus.h"

   i2c_config_t conf = {
       .mode = I2C_MODE_MASTER,
       .sda_io_num = I2C_MASTER_SDA_IO, // GPIO_NUM_20
       .sda_pullup_en = GPIO_PULLUP_ENABLE,
       .scl_io_num = I2C_MASTER_SCL_IO, // GPIO_NUM_13
       .scl_pullup_en = GPIO_PULLUP_ENABLE,
       .master.clk_speed = 100000,
   };
   i2c_bus_handle_t bus = i2c_bus_create(I2C_NUM_0, &conf);

   power_measure_config_t common = {
       .overcurrent = 15,
       .overvoltage = 260,
       .undervoltage = 180,
       .enable_energy_detection = false,
   };

   power_measure_ina236_config_t ina = {
       .i2c_bus = bus,
       .i2c_addr = 0x41,
       .alert_en = false,
       .alert_pin = -1,
       .alert_cb = NULL,
   };

   power_measure_handle_t h;
   ESP_ERROR_CHECK(power_measure_new_ina236_device(&common, &ina, &h));

   float u,i,p;
   power_measure_get_voltage(h, &u);
   power_measure_get_current(h, &i);
   power_measure_get_active_power(h, &p);

   power_measure_delete(h);
   i2c_bus_delete(&bus);

Troubleshooting
---------------

BL0937 Issues:
~~~~~~~~~~~~~~

1. **Failed Initialization** : If the initialization fails, ensure that all GPIO pins are correctly defined and connected to the **BL0937** chip.
2. **Measurement Failures** : If the measurements fail (e.g., voltage, current), verify that the **BL0937** chip is properly powered and communicating with your ESP32 series chips.

INA236 Issues:
~~~~~~~~~~~~~~

1. **I2C Bus Initialization Failed** : Check that the I2C pins (SDA/SCL) are correctly connected and not conflicting with other peripherals.
2. **INA236 Not Detected** : Verify the I2C address (default 0x41) and ensure the chip is properly powered.
3. **Measurement Failures** : Check I2C communication and ensure the INA236 chip is functioning correctly.
4. **No Real-time Data** : INA236 provides real-time measurements, so if you see cached values, check the power_measure component implementation.

Adapted Products
-----------------------

.. list-table:: Power Measurement Chips
   :header-rows: 1

   * - Name
     - Function
     - Vendor
     - Datasheet
     - HAL
   * - BL0937
     - detect electrical parameters such as voltage, current, active power, and energy consumption
     - BELLING
     - `BL0937 Datasheet`_
     - √
   * - INA236
     - precision power monitor with I2C interface for voltage, current, and power measurement
     - TI
     - `INA236 Datasheet`_
     - √


.. _BL0937 Datasheet: https://www.belling.com.cn/media/file_object/bel_product/BL0937/datasheet/BL0937_V1.02_en.pdf
.. _INA236 Datasheet: https://www.ti.com/lit/ds/symlink/ina236.pdf?ts=1716462373021

API Reference
--------------------

The following API implements hardware abstraction for power measure. Users can directly call this layer of code to write sensor applications.

.. include-build-file:: inc/power_measure.inc