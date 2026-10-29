**功率测量**
==================

:link_to_translation:`en:[English]`

功率测量IC(集成电路)是用于监控和管理电源的集成电路。它可以实时监控电源的各种参数,如电压、电流和功率并为系统提供这些信息。功率测量IC在各种应用中至关重要,包括计算机、电源管理系统、消费电子产品、工业控制系统和通信设备。

本示例演示了如何使用 BL0937、INA236 和 BL0942 功率测量芯片来检测电压、电流、有功功率和能耗(仅BL0937和BL0942)等电气参数。

功能
----

* 测量**电压**、**电流**、**有功功率**、**功率因数**、**能量**和**频率**。
* 支持 **BL0937**、**INA236** 和 **BL0942** 功率测量芯片。
* 通过 `BL0937`、`INA236` 和 `BL0942` 宏可配置芯片选择。
* 支持过流、过压和欠压保护。
* 启用能量检测以获得准确读数（仅限 BL0937 和 BL0942）。
* INA236 实时测量，BL0937 和 BL0942 缓存测量。
* BL0937芯片测量使用具有校准因子的硬件脉冲计数方式，通过校准因子将脉冲计数转换为实际电气参数。

BL0937 校准流程与方法
----------------------

以下流程可用于校准 BL0937 的电压（KU）、电流（KI）与功率（KP）三个因子，提升测量准确度：

- 准备：可溯源万用表/功率计、稳定市电/可调交流源、已知阻性负载（如电阻负载或12W白炽灯）。
- 术语：
  - KU：电压校准因子（使测得电压接近真实电压）。
  - KI：电流校准因子（使测得电流接近真实电流）。
  - KP：功率校准因子（使测得有功功率接近真实功率）。

步骤：
1. 固定硬件参数（分流电阻、分压电阻）并烧录示例工程，打开串口日志。
2. 电压校准（KU）：在空载或轻载下，读取 `voltage_meas`，用万用表得到 `voltage_true`，按公式更新：

   .. code-block:: text

      KU_new = KU_old × (voltage_true / voltage_meas)

3. 电流校准（KI）：接入已知阻性负载，读取 `current_meas`，用万用表/功率计计算/读取 `current_true`，更新：

   .. code-block:: text

      KI_new = KI_old × (current_true / current_meas)

4. 功率校准（KP）：在相同负载下，读取 `power_meas`，以万用表/功率计的 `power_true` 为准，更新：

   .. code-block:: text

      KP_new = KP_old × (power_true / power_meas)

5. 重复2-4步数次，直至电压/电流/功率的相对误差满足要求（例如 ≤1%~2%）。

在代码中，将校准后的 KU/KI/KP 配置到 `power_measure_bl0937_config_t`（示例中的 `ki/ku/kp` 字段）。

更多背景与项目总体信息参见：`README_CN.md <../../../../examples/sensors/power_measure/README_CN.md>`_。

工作原理
--------

`main/power_measure_example.c` 中的 `app_main()` 展示了基于"工厂创建 + 句柄 API"的用法：

1. **（可选）初始化 I2C 总线（仅 INA236）**：

   - 使用 `i2c_bus_create()` 按 `I2C_MASTER_SCL_IO` / `I2C_MASTER_SDA_IO` 创建 I2C 总线。
2. **创建功率测量设备**：

   - 准备通用配置 `power_measure_config_t` （过流/过压/欠压阈值、是否启用能量检测）。
   - 对 BL0937，准备 `power_measure_bl0937_config_t` （CF/CF1/SEL、分压/采样电阻、KI/KU/KP）。
   - 对 INA236，准备 `power_measure_ina236_config_t` （I2C 句柄、地址、告警配置）。
   - 对 BL0942，准备 `power_measure_bl0942_config_t` （UART/SPI配置、分流电阻、分压比、设备地址）。
   - 使用 `power_measure_new_bl0937_device()` 、 `power_measure_new_ina236_device()` 或 `power_measure_new_bl0942_device()` 创建设备并获得 `power_measure_handle_t` 句柄。
3. **周期读取测量值**：

   - 使用 `power_measure_get_voltage/current/active_power/power_factor()` 获取参数；
   - `power_measure_get_energy()` 仅在支持能量计量的芯片（如 BL0937 和 BL0942）上返回有效结果；
   - `power_measure_get_line_freq()` 仅在 BL0942 芯片上返回有效结果；
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

BL0942（UART/SPI）
^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: c

   #include "power_measure.h"
   #include "power_measure_bl0942.h"

   power_measure_config_t common = {
       .overcurrent = 15,
       .overvoltage = 260,
       .undervoltage = 180,
       .enable_energy_detection = true,
   };

   power_measure_bl0942_config_t bl0942 = {
       .addr = 0,
       .shunt_resistor = 0.001f,
       .divider_ratio = 3760.0f,
       .use_spi = false,
       .uart = {
           .uart_num = UART_NUM_1,
           .tx_io = GPIO_NUM_6,
           .rx_io = GPIO_NUM_7,
           .sel_io = -1,
           .baud = 4800,
       },
       .spi = {
           .spi_host = SPI2_HOST,
           .mosi_io = -1,
           .miso_io = -1,
           .sclk_io = -1,
           .cs_io = -1,
       },
   };

   // power_measure_bl0942_config_t bl0942 = {
   //     .addr = 0,
   //     .shunt_resistor = 0.001f,
   //     .divider_ratio = 3760.0f,
   //     .use_spi = true,
   //     .uart = {
   //         .uart_num = UART_NUM_1,
   //         .tx_io = -1,
   //         .rx_io = -1,
   //         .sel_io = -1,
   //         .baud = 4800,
   //     },
   //     .spi = {
   //         .spi_host = SPI2_HOST,
   //         .mosi_io = GPIO_NUM_11,
   //         .miso_io = GPIO_NUM_12,
   //         .sclk_io = GPIO_NUM_10,
   //         .cs_io = GPIO_NUM_13,
   //     },
   // };

   power_measure_handle_t h;
   ESP_ERROR_CHECK(power_measure_new_bl0942_device(&common, &bl0942, &h));

   float u, i, p, pf, e, f;
   power_measure_get_voltage(h, &u);
   power_measure_get_current(h, &i);
   power_measure_get_active_power(h, &p);
   power_measure_get_power_factor(h, &pf);
   power_measure_get_energy(h, &e);
   power_measure_get_line_freq(h, &f);

   power_measure_delete(h);

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

BL0942 问题：
^^^^^^^^^^^^^^^^^^^^

1. **UART 初始化失败**：检查 UART 引脚（TX/RX）是否正确连接，确认波特率设置（默认 4800）。
2. **SPI 初始化失败**：检查 SPI 引脚（MOSI/MISO/SCLK/CS）是否正确连接且未与其他外设冲突。
3. **BL0942 未检测到**：验证设备地址（默认 0）并确保芯片正确供电。
4. **通信失败**：检查 UART/SPI 通信参数，确认接口模式配置正确。
5. **测量值异常**：检查分流电阻和分压比配置是否与实际硬件匹配。
6. **看门狗复位**：如果出现看门狗复位，检查硬件连接和通信配置。

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
   * - BL0942
     - 单相功率测量IC，支持UART/SPI接口，可测量电压、电流、功率、功率因数、能量和频率
     - BELLING
     - `BL0942 Datasheet`_
     - √


.. _BL0937 Datasheet: https://www.belling.com.cn/media/file_object/bel_product/BL0937/datasheet/BL0937_V1.02_en.pdf
.. _INA236 Datasheet: https://www.ti.com/lit/ds/symlink/ina236.pdf?ts=1716462373021
.. _BL0942 Datasheet: https://www.belling.com.cn/media/file_object/bel_product/BL0942/datasheet/BL0942_V1.05_cn.pdf


API 参考
--------------------

以下API实现了功率测量的硬件抽象。用户可以直接调用此层代码来编写传感器应用程序。

.. include-build-file:: inc/power_measure.inc