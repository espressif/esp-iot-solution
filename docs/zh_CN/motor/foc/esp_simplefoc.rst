ESP SimpleFOC
================

:link_to_translation:`en:[English]`


``esp_simplefoc`` 是一个基于 `Arduino-FOC <https://github.com/simplefoc/Arduino-FOC>`__ 的 FOC 组件，针对支持 LEDC 与 MCPWM 功能的 ESP 芯片开发。支持以下特性：

- 支持电压控制
- 支持通过 `SimpleFOCStudio <https://github.com/simplefoc/SimpleFOCStudio>`__ 来调整和配置电机控制参数
- 兼容 `Arduino-FOC <https://github.com/simplefoc/Arduino-FOC>`__ 控制例程
- 采用 `IQMath <https://components.espressif.com/components/espressif/iqmath>`__ 实现 FOC 运算加速
- 支持手动选择 LEDC 或 MCPWM 外设来控制 MOS 驱动器，最高可实现四路无刷电机控制

FOC (Field-Oriented Control) 是一种用于无刷直流电机磁场定向控制算法，能够产生平滑的扭矩、速度与位置控制。有关更友好的解释，请查看 `Arduino-FOC 官方文档 <https://docs.simplefoc.com/>`__ 。

在您的项目中使用 SimpleFOC
------------------------------

1. 使用 ``idf.py add_dependency`` 命令将组件添加到项目中

    .. code:: sh

        idf.py add-dependency "espressif/esp_simplefoc"

2. 使用 ``idf.py menuconfig`` 配置 FreeRTOS ``(Top) → Component config → FreeRTOS → Kernel``

-  ``configTICK_RATE_HZ``: FreeRTOS的时钟Tick的频率，默认情况下为 100，在 FOC 场景下需设置为 1000

3. 配置电机参数、MOS 驱动器控制参数与引脚、角度传感器、控制器

**电机参数**

-  ``pp``：极对数，您需要根据实际的三相无刷电机参数设置对应的电机极对数
-  ``R``：电机相电阻，默认为 ``NOT_SET``
-  ``KV``: 电机 KV 值，默认为 ``NOT_SET``
-  ``L``：电机相电感，默认为 ``NOT_SET``

对于极对数为 14 的三相无刷电机而言，对应的电机实例化为：

    .. code:: cpp
        
        BLDCMotor motor = BLDCMotor(14);

**MOS 驱动器控制参数与引脚**

-  ``pwm pin``：PWM 输出引脚，您需要根据实际的 MOS 驱动器电路来设置对应的 PWM 输出引脚
-  ``enable pin``：MOS 驱动器使能脚，若您的 MOS 驱动器需要手动使能，请手动填入对应的 GPIO，默认为 ``NOT_SET``
-  ``voltage_power_supply``：MOS 驱动器的供电电压
-  ``voltage_limit``：MOS 驱动器的电压限制

对于 3PWM 模式的 12V MOS 驱动器而言，对应的驱动实例化与参数为：

    .. code:: cpp

        BLDCDriver3PWM driver = BLDCDriver3PWM(4, 5, 6);
        driver.voltage_power_supply = 12;
        driver.voltage_limit = 11;
        driver.init({1, 2, 3});
        motor.linkDriver(&driver);

**角度传感器**

您需要根据实际使用的角度传感器进行对应的实例化，当前支持的角度传感器：

-  ``as5048a``：支持 SPI 方式读取角度数据
-  ``mt6701``：支持 I2C、SPI 方式读取角度数据
-  ``as5600``：支持 I2C 方式读取角度数据

对于 AS5600 角度传感器实例化与对应的绑定操作为：

    .. code:: cpp

        AS5600 as5600 = AS5600(I2C_NUM_0, GPIO_NUM_1, GPIO_NUM_2);
        as5600.init();
        motor.linkSensor(&as5600);

**控制器**

``esp_simplefoc`` 支持多种控制方式，您需要根据实际的应用需求进行选择：

-  ``torque``：力矩控制
-  ``velocity``：速度控制
-  ``angle``：角度控制
-  ``velocity_openloop``：速度开环控制
-  ``angle_openloop``：角度开环控制

    .. Note::
        若在硬件验证阶段，可优先选择开环控制方案，以实现对硬件的快速验证。

4. FOC 初始化与循环控制

在完成上述参数设置后，您需要运行 电机 ``init`` 与 ``initFOC``，以实现电机初始化校准，并启动 ``loopFOC`` :

    .. code:: cpp

        motor.init();                                  
        motor.initFOC(); 
        while (1) {
            motor.loopFOC();
            motor.move(target_value);
            command.run();
        }

    .. Note::
        若 ``initFOC`` 所估计的极对数与实际填入的极对数不符合，请排查电机极对数，并尽可能缩减磁环与角度传感器之间的间距。

5. 运行 ``idf.py build flash`` 进行初始下载, 之后可观察电机的实际运行效果

API 参考
----------------

``esp_simplefoc`` 的 API 接口与 `Arduino-FOC <https://github.com/simplefoc/Arduino-FOC>`__，可参考 `Arduino-FOC API 文档 <http://source.simplefoc.com/>`__
