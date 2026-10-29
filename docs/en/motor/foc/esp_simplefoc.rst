ESP SimpleFOC
================

:link_to_translation:`zh_CN:[中文]`


``esp_simplefoc`` is an FOC component based on `Arduino-FOC <https://github.com/simplefoc/Arduino-FOC>`__, developed specifically for ESP chips that support LEDC and MCPWM functions. It supports the following features:

- Voltage control support
- Motor control parameters can be adjusted and configured through `SimpleFOCStudio <https://github.com/simplefoc/SimpleFOCStudio>`__
- Compatible with `Arduino-FOC <https://github.com/simplefoc/Arduino-FOC>`__ control routines
- Uses `IQMath <https://components.espressif.com/components/espressif/iqmath>`__ for FOC computation acceleration
- Allows manual selection of LEDC or MCPWM peripherals to control MOSFET drivers, supporting up to four brushless motor controls

FOC (Field-Oriented Control) is a field-oriented control algorithm for brushless DC motors that can produce smooth torque, speed, and position control. For a more user-friendly explanation, refer to the `Arduino-FOC official documentation <https://docs.simplefoc.com/>`__.

Using SimpleFOC in Your Project
-----------------------------------

1. Use the command ``idf.py add_dependency`` to add the component to your project:

    .. code:: sh

        idf.py add-dependency "espressif/esp_simplefoc"

2. Use ``idf.py menuconfig`` to configure FreeRTOS ``(Top) → Component config → FreeRTOS → Kernel``:

-  ``configTICK_RATE_HZ``: The tick rate frequency for FreeRTOS, defaulting to 100, needs to be set to 1000 in FOC scenarios.

3. Configure motor parameters, MOSFET driver control parameters and pins, angle sensors, and controllers.

**Motor Parameters**

-  ``pp``: Pole pair count, which you need to set according to the actual three-phase brushless motor parameters.
-  ``R``: Motor phase resistance, defaults to ``NOT_SET``.
-  ``KV``: Motor KV value, defaults to ``NOT_SET``.
-  ``L``: Motor phase inductance, defaults to ``NOT_SET``.

For a three-phase brushless motor with 14 pole pairs, the corresponding motor initialization would be:

    .. code:: cpp
        
        BLDCMotor motor = BLDCMotor(14);

**MOSFET Driver Control Parameters and Pins**

-  ``pwm pin``: PWM output pin, which you need to set according to the actual MOSFET driver circuit.
-  ``enable pin``: MOSFET driver enable pin. If your MOSFET driver needs manual enabling, fill in the corresponding GPIO manually. Defaults to `NOT_SET`.
-  ``voltage_power_supply``: Power supply voltage for the MOSFET driver.
-  ``voltage_limit``: Voltage limit for the MOSFET driver.

For a 3PWM mode 12V MOSFET driver, the corresponding driver initialization and parameters are:

    .. code:: cpp

        BLDCDriver3PWM driver = BLDCDriver3PWM(4, 5, 6);
        driver.voltage_power_supply = 12;
        driver.voltage_limit = 11;
        driver.init({1, 2, 3});
        motor.linkDriver(&driver);

**Angle Sensor**

You need to initialize the angle sensor according to the actual sensor you are using. Currently supported angle sensors are:

-  ``as5048a``: Supports SPI interface for angle data reading.
-  ``mt6701``: Supports I2C and SPI interfaces for angle data reading.
-  ``as5600``: Supports I2C interface for angle data reading.

For initializing the AS5600 angle sensor and binding it to the motor:

    .. code:: cpp

        AS5600 as5600 = AS5600(I2C_NUM_0, GPIO_NUM_1, GPIO_NUM_2);
        as5600.init();
        motor.linkSensor(&as5600);

**Controller**

``esp_simplefoc`` supports various control methods. You can choose based on the actual application requirements:

-  ``torque``: Torque control.
-  ``velocity``: Speed control.
-  ``angle``: Angle control.
-  ``velocity_openloop``: Open-loop speed control.
-  ``angle_openloop``: Open-loop angle control.

    .. Note::
        For hardware verification stages, you may prefer to select an open-loop control scheme for quick hardware validation.

4. FOC Initialization and Control Loop

After completing the above parameter settings, you need to run ``init`` and ``initFOC`` to perform motor initialization and calibration, then start ``loopFOC``:

    .. code:: cpp

        motor.init();                                  
        motor.initFOC(); 
        while (1) {
            motor.loopFOC();
            motor.move(target_value);
            command.run();
        }

    .. Note::
        If the pole pair count estimated by ``initFOC`` does not match the actual pole pair count entered, please check the motor's pole pairs and minimize the distance between the magnetic ring and the angle sensor as much as possible.

5. Run ``idf.py build flash`` for the initial download, then observe the motor's actual running performance.

API Reference
----------------

The API interface of ``esp_simplefoc`` is consistent with `Arduino-FOC <https://github.com/simplefoc/Arduino-FOC>`__, and you can refer to the `Arduino-FOC API documentation <http://source.simplefoc.com/>`__.
