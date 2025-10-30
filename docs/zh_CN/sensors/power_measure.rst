**功率测量**
==================

:link_to_translation:`en:[English]`

功率测量IC(集成电路)是用于监控和管理电源的集成电路。它可以实时监控电源的各种参数,如电压、电流和功率并为系统提供这些信息。功率测量IC在各种应用中至关重要,包括计算机、电源管理系统、消费电子产品、工业控制系统和通信设备。

本示例演示了如何使用 BL0937 和 INA236 功率测量芯片来检测电压、电流、有功功率和能耗(仅BL0937)等电气参数。

功能
----

* 测量**电压**、**电流**、**有功功率**和**能量**。
* 支持 **BL0937** 和 **INA236** 功率测量芯片。
* 通过 `DEMO_BL0937` 和 `DEMO_INA236` 宏可配置芯片选择。
* 支持过流、过压和欠压保护。
* 启用能量检测以获得准确读数（仅限 BL0937）。
* INA236 实时测量，BL0937 缓存测量。
* 每秒定期获取功率读数并记录。

工作原理
--------

`main/power_measure_example.c` 中的 `app_main()` 展示了基于"工厂创建 + 句柄 API"的用法：

1. **（可选）初始化 I2C 总线（仅 INA236）**：

   - 使用 `i2c_bus_create()` 按 `I2C_MASTER_SCL_IO` / `I2C_MASTER_SDA_IO` 创建 I2C 总线。
2. **创建功率测量设备**：

   - 准备通用配置 `power_measure_config_t` （过流/过压/欠压阈值、是否启用能量检测）。
   - 对 BL0937，准备 `power_measure_bl0937_config_t` （CF/CF1/SEL、分压/采样电阻、KI/KU/KP）。
   - 对 INA236，准备 `power_measure_ina236_config_t` （I2C 句柄、地址、告警配置）。
   - 使用 `power_measure_new_bl0937_device()` 或 `power_measure_new_ina236_device()` 创建设备并获得 `power_measure_handle_t` 句柄。
3. **周期读取测量值**：

   - 使用 `power_measure_get_voltage/current/active_power/power_factor()` 获取参数；
   - `power_measure_get_energy()` 仅在支持能量计量的芯片（如 BL0937）上返回有效结果；
   - 示例每 1 秒读取并输出日志。
4. **错误处理**：

   - 任一接口返回非 `ESP_OK` 时，通过 `ESP_LOGE` 打印错误。
5. **资源释放**：

   - 使用 `power_measure_delete(handle)` 释放设备；
   - INA236 示例中，使用 `i2c_bus_delete(&i2c_bus)` 释放 I2C 总线。

代码片段
--------

BL0937（GPIO）
^^^^^^^^^^^^^^^^^^^^

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

INA236（I2C）
^^^^^^^^^^^^^^^^^^^^

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

故障排除
--------

BL0937 问题：
^^^^^^^^^^^^^^^^^^^^

1. **初始化失败**：如果初始化失败，请确保所有 GPIO 引脚都已正确定义并连接到 **BL0937** 芯片。
2. **测量失败**：如果测量失败（如电压、电流），请检查 **BL0937** 芯片是否正确供电并与 ESP32系列芯片 通信。

INA236 问题：
^^^^^^^^^^^^^^^^^^^^

1. **I2C 总线初始化失败**：检查 I2C 引脚（SDA/SCL）是否正确连接且未与其他外设冲突。
2. **INA236 未检测到**：验证 I2C 地址（默认 0x41）并确保芯片正确供电。
3. **测量失败**：检查 I2C 通信并确保 INA236 芯片正常工作。
4. **无实时数据**：INA236 提供实时测量，如果看到缓存值，请检查 power_measure 组件实现。

适配列表
-----------------------

.. list-table:: 功率监测芯片
   :header-rows: 1

   * - 名称
     - 功能
     - 制造商
     - 规格书
     - 硬件抽象层
   * - BL0937
     - 检测电气参数，例如电压、电流、有功功率和能耗
     - BELLING
     - `BL0937 Datasheet`_
     - √
   * - INA236
     - 带I2C接口的精密功率监测器，用于电压、电流和功率测量
     - TI
     - `INA236 Datasheet`_
     - √


.. _BL0937 Datasheet: https://www.belling.com.cn/media/file_object/bel_product/BL0937/datasheet/BL0937_V1.02_en.pdf
.. _INA236 Datasheet: https://www.ti.com/lit/ds/symlink/ina236.pdf?ts=1716462373021


API 参考
--------------------

以下API实现了功率测量的硬件抽象。用户可以直接调用此层代码来编写传感器应用程序。

.. include-build-file:: inc/power_measure.inc