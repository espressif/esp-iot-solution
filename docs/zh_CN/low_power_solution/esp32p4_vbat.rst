ESP32-P4 备用电池供电方案 
=========================

:link_to_translation:`en:[English]`

ESP32-P4 集成 RTC (Real-Time Clock，实时时钟) 功能，包含备用电源输入端 （VBAT），32.768KHz 外部晶振输入，电源切换控制等，可在主电源断开后保持计时准确。

.. warning:: ESP-IDF 尚未完全支持 VBAT 方案，当前方案仅供测试。请按照如下过程在 master 分支上修改 ESP-IDF 驱动。

**支持以下特性**：

- 支持主电源掉电保持计时功能，掉电后由备用电池（VBAT）供电
- 支持睡眠/唤醒时 PMU （Power management unit，电源管理单元）自动切换电源
- 支持备用电池电压检测，阈值报警
- 支持备用电池充电，当 VDDA 存在时，可通过内部电阻为 VBAT 上的备用电池充电

.. figure:: ../../_static/low_power/p4_vbat_frame.png
    :align: center
    :scale: 70%

    ESP32-P4 备用电池连接示意图

电池连接与电路保护
--------------------

ESP32-P4 备用电池的连接非常简单。VBAT 引脚内置充电保护电路，因此无需外部电阻与二极管，有效降低了在备用电池通路上增加元件带来的有害风险。

.. figure:: ../../_static/low_power/p4_power_switch.png
    :align: center
    :scale: 80%

    ESP32-P4 电池切换控制框图

备用电池的选择
^^^^^^^^^^^^^^^^

- 低自放电率: 电池应具有出色的抗漏电性
- 充放电性能: 较高充放电次数的电池可以提升产品使用寿命
- 放电电流: VBAT 供电功耗较低（几uA），因此大部分电池放电能力均可满足
- 充电电流: 支持 VDDA 给 VBAT 上备用电池充电。软件可配置充电限流电阻 1KΩ ~ 9KΩ

.. warning:: 严禁接入 3.6V 的可充电电池，由于 3.6V 的可充电电池在满电情况下的电压能够达到 4V 左右，而 VBAT 引脚的输入电压范围为 2.3V ~ 3.6V，可能会导致 VBAT 引脚损坏。

电池上的去耦电容
^^^^^^^^^^^^^^^^^^

在电路设计中，用户通常会为每一个供电引脚添加一个去耦电容。但当 VBAT 作为备用电源输入时，电容上的泄露将导致电池寿命明显缩短，此外，电池具有良好的电容特性，因此无需外部去耦电容。

.. note:: D1 与 R1 为 ESD 保护器件，Rx 为充电保护电阻，使充电电流限制在 1 mA 以下，保护电路满足 UL 认证测试要求。

电源切换
------------

VBAT 与 VDDA 的电源选择由 PMU 在睡眠与唤醒时自动控制或手动选择。用户需要提前配置寄存器。当芯片处于 sleep 状态时可配置 VDDA 或 VBAT 作为电源输入。

- PMU_HP_SLEEP_VDDBAT_MODE: 选择 HP_SLEEP 状态时的供电电源。0：VDDA 供电、1：VBAT 供电、2：自动选择
- PMU_VDDBAT_SW_UPDATE: 置为1 使 ``PMU_VDDBAT_CFG_REG`` 寄存器配置生效
- PMU_LP_ANA_WAIT_TARGET: 设置唤醒过程中的 delay 时间。时间长度应该大于 VDDA 外部电源的上电稳定时间

.. note:: 当前 ESP-IDF 尚未完整支持 PMU 驱动。若您需要测试 VBAT 功能，请按照下述测试过程在 ESP-IDF 中添加必要的代码。

测试过程如下：

1. 在 ``components/esp_hw_support/port/esp32p4/pmu_sleep.c`` 中添加必要的头文件

.. code:: c

    #include "soc/pmu_reg.h"
    #include "soc/lp_analog_peri_reg.h"
    #include "soc/lp_gpio_struct.h"
    #include "esp_private/regi2c_ctrl.h"
    #include "soc/lp_analog_peri_reg.h"
    #include "soc/lp_analog_peri_struct.h"
    #include "esp_private/regi2c_ctrl.h"
    #include "soc/regi2c_bias.h"


2. 在 ``components/esp_hw_support/port/esp32p4/pmu_sleep.c`` 的 ``pmu_sleep_start`` 函数中的 ``pmu_ll_hp_set_sleep_enable(PMU_instance()->hal->dev);`` 上方添加如下代码：

方式1：VDDA 与 VBAT 自动切换方式：

.. code:: c

    #define VBAT_MODE_VDDA 0
    #define VBAT_MODE_VBAT 1
    #define VBAT_MODE_AUTO 2

    REG_SET_FIELD(PMU_HP_SLEEP_LP_DIG_POWER_REG, PMU_HP_SLEEP_VDDBAT_MODE, VBAT_MODE_AUTO);
    REG_SET_BIT(PMU_VDDBAT_CFG_REG, PMU_VDDBAT_SW_UPDATE);
    while(VBAT_MODE_AUTO != REG_GET_FIELD(PMU_VDDBAT_CFG_REG, PMU_ANA_VDDBAT_MODE));


方式2：VDDA 与 VBAT 手动设置：

.. code:: c

    #define VBAT_MODE_VDDA 0
    #define VBAT_MODE_VBAT 1
    #define VBAT_MODE_AUTO 2

    REG_SET_FIELD(PMU_HP_SLEEP_LP_DIG_POWER_REG, PMU_HP_SLEEP_VDDBAT_MODE, VBAT_MODE_VDDA);
    REG_SET_BIT(PMU_VDDBAT_CFG_REG, PMU_VDDBAT_SW_UPDATE);
    while(VBAT_MODE_VDDA != REG_GET_FIELD(PMU_VDDBAT_CFG_REG, PMU_ANA_VDDBAT_MODE));
    REG_SET_FIELD(PMU_LP_SLEEP_LP_DIG_POWER_REG, PMU_LP_SLEEP_VDDBAT_MODE, VBAT_MODE_VBAT);
    REG_SET_FIELD(PMU_SLP_WAKEUP_CNTL5_REG, PMU_LP_ANA_WAIT_TARGET, 0xFF);

.. note:: 经过测试，deep sleep 下的 VBAT 平均电流为 7 uA，light sleep 下的 VBAT 平均电流为 21 uA

备用电池运行时间
^^^^^^^^^^^^^^^^^^^^

以 CR2032 电池 （225 mAh）为例，在 deep sleep 下的理论待机时间为 3.669 年。

欠压检测器 & BOD filter
---------------------------

ESP32-P4 的欠压检测器支持检测 VDDA 与 VBAT 的电压，在电压快速下落至预设阈值以下触发信号，并进行相应的处理。

阈值配置与电压关系：

+----------+-------+
| 阈值配置 | 电压  |
+==========+=======+
| 0        | 2.52v |
+----------+-------+
| 1        | 2.57v |
+----------+-------+
| 2        | 2.63v |
+----------+-------+
| 3        | 2.68v |
+----------+-------+
| 4        | 2.74v |
+----------+-------+
| 5        | 2.78v |
+----------+-------+
| 6        | 2.83v |
+----------+-------+
| 7        | 2.89v |
+----------+-------+

新建测试工程，并添加如下代码检测欠压标志位：

.. code:: c

    REGI2C_WRITE_MASK(I2C_BIAS, I2C_BIAS_OR_DREFL_VDDA, 6);                                                                        // Vdda 比较器电压阈值设置 2.83
    REGI2C_WRITE_MASK(I2C_BIAS, I2C_BIAS_OR_DREFH_VDDA, 7);                                                                        // Vdda 比较器电压阈值设置 2.89
    REGI2C_WRITE_MASK(I2C_BIAS, I2C_BIAS_OR_DREFL_VBAT, 6);                                                                        // Vbat 比较器电压阈值设置 2.83
    REGI2C_WRITE_MASK(I2C_BIAS, I2C_BIAS_OR_DREFH_VBAT, 7);                                                                        // Vbat 比较器电压阈值设置 2.89

    REG_SET_FIELD(LP_ANALOG_PERI_VDDBAT_CHARGE_CNTL_REG, LP_ANALOG_PERI_VDDBAT_CHARGE_UNDERVOLTAGE_TARGET, 20);                    // 设置bod filter的时间阈值，单位20MHz cycle
    REG_SET_FIELD(LP_ANALOG_PERI_VDDBAT_CHARGE_CNTL_REG, LP_ANALOG_PERI_VDDBAT_CHARGE_UPVOLTAGE_TARGET, 10);                       // 设置bod filter的时间阈值，单位20MHz cycle
    REG_SET_BIT(LP_ANALOG_PERI_VDDBAT_CHARGE_CNTL_REG, LP_ANALOG_PERI_VDDBAT_CHARGE_CNT_CLR);
    esp_rom_delay_us(30);
    REG_CLR_BIT(LP_ANALOG_PERI_VDDBAT_CHARGE_CNTL_REG, LP_ANALOG_PERI_VDDBAT_CHARGE_CNT_CLR);

    while (1)
    {
        printf("value:%lx\n", REG_READ(LP_ANALOG_PERI_VDD_SOURCE_CNTL_REG));
        REG_WRITE(LP_ANALOG_PERI_VDD_SOURCE_CNTL_REG, 0X30020000);                                                                 // 清空VBAT欠压标志位
        REG_WRITE(LP_ANALOG_PERI_VDD_SOURCE_CNTL_REG, 0X30040000);                                                                 // 清空VDDA欠压标志位
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }


电池充电电路
----------------

当选择可充电电池作为 VBAT 电源输入时，芯片支持检测电池电压，并在低于设定阈值通过 VDDA 反向给 VBAT 端电池充电。

充电检测阈值与电压关系：

+----------+-------+
| 阈值配置 | 电压  |
+==========+=======+
| 0        | 2.52v |
+----------+-------+
| 1        | 2.57v |
+----------+-------+
| 2        | 2.63v |
+----------+-------+
| 3        | 2.68v |
+----------+-------+
| 4        | 2.74v |
+----------+-------+
| 5        | 2.78v |
+----------+-------+
| 6        | 2.83v |
+----------+-------+
| 7        | 2.89v |
+----------+-------+

新建测试工程，并添加如下代码检测欠压标志位：

.. code:: c

    REGI2C_WRITE_MASK(I2C_BIAS, I2C_BIAS_OR_DREFL_VDDA, 6);                                                                        // Vdda 比较器电压阈值设置 2.83
    REGI2C_WRITE_MASK(I2C_BIAS, I2C_BIAS_OR_DREFH_VDDA, 7);                                                                        // Vdda 比较器电压阈值设置 2.89
    REGI2C_WRITE_MASK(I2C_BIAS, I2C_BIAS_OR_DREFL_VBAT, 6);                                                                        // Vbat 比较器电压阈值设置 2.83
    REGI2C_WRITE_MASK(I2C_BIAS, I2C_BIAS_OR_DREFH_VBAT, 7);                                                                        // Vbat 比较器电压阈值设置 2.89

    REGI2C_WRITE_MASK(I2C_BIAS, I2C_BIAS_OR_DREFL_VBAT_CHARGER, 6);                                                                // 充电检测比较器的阈值设置，应高于 brownout 报警电压 2.83
    REGI2C_WRITE_MASK(I2C_BIAS, I2C_BIAS_OR_DREFH_VBAT_CHARGER, 7);                                                                // 充电检测比较器的阈值设置，应高于 brownout 报警电压
    REGI2C_WRITE_MASK(I2C_BIAS, I2C_BIAS_OR_FORCE_PU_VBAT_CHARGER, 1);                                                             // vbat_charger 比较器强制上电

    REG_SET_FIELD(LP_ANALOG_PERI_VDDBAT_CHARGE_CNTL_REG, LP_ANALOG_PERI_VDDBAT_CHARGE_UNDERVOLTAGE_TARGET, 20);                    // 设置bod filter的时间阈值，单位20MHz cycle
    REG_SET_FIELD(LP_ANALOG_PERI_VDDBAT_CHARGE_CNTL_REG, LP_ANALOG_PERI_VDDBAT_CHARGE_UPVOLTAGE_TARGET,    10);                    // 设置bod filter的时间阈值，单位20MHz cycle
    REG_SET_BIT(LP_ANALOG_PERI_VDDBAT_CHARGE_CNTL_REG, LP_ANALOG_PERI_VDDBAT_CHARGE_CNT_CLR);
    esp_rom_delay_us(30);
    REG_CLR_BIT(LP_ANALOG_PERI_VDDBAT_CHARGE_CNTL_REG, LP_ANALOG_PERI_VDDBAT_CHARGE_CNT_CLR);

    while(1)
    {
        printf("value:%lx\n", REG_GET_BIT(LP_ANALOG_PERI_VDDBAT_CHARGE_CNTL_REG, LP_ANALOG_PERI_VDDBAT_CHARGE_UNDERVOLTAGE_FLAG)); // 获取 VBAT 管脚充电标志信号
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }



VBAT 供电场景下的外设支持功能
------------------------------

RTC TIMER
^^^^^^^^^^^^

在进入休眠状态切换 VBAT 供电后，VDDA 下电后 RTC TIMER 正常计数，再次重启后时间显示正常。测试代码如下：

.. code:: c

    time_t now;
    char strftime_buf[64];
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    printf( "The current date/time is: %s\n", strftime_buf);

    const gpio_config_t config = {
        .pin_bit_mask = BIT(GPIO_NUM_0),
        .mode = GPIO_MODE_INPUT,
    };

    ESP_ERROR_CHECK(gpio_config(&config));
    esp_deep_sleep_enable_gpio_wakeup(BIT(GPIO_NUM_0), 0);

    esp_deep_sleep_start();

.. note:: 您需要按照上述 电源切换章节 在 ESP-IDF 中添加电源切换代码，同时，测试过程在 VDDA 下电后等待了一段时间后重新上电，并使用 GPIO0 唤醒 ESP32-P4，对比休眠前后时间变化。


LP GPIO/ADC/UART 外设
^^^^^^^^^^^^^^^^^^^^^^^^^^

请参考 ESP-IDF 中 ``examples/system/ulp/lp_core`` 的测试驱动进行测试，当前 GPIO/ADC/UART 的测试已支持 ESP32-P4。
