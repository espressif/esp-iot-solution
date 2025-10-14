# 功率测量示例

## 概述

本示例演示了如何使用功率测量芯片（**BL0937** 和 **INA236**）来检测电压、电流、有功功率和能耗等电气参数。它使用 FreeRTOS 在 **ESP32系列芯片** 上实现，展示了如何配置和连接不同的功率测量芯片。该示例初始化功率测量系统、获取各种参数并定期记录。

本示例支持两种功率测量芯片：

### BL0937 芯片

能够测量：

1. **电压**
2. **电流**
3. **有功功率**
4. **功率因数**
5. **能量**

### INA236 芯片

能够测量：

1. **电压**
2. **电流**
3. **有功功率**
4. **功率因数**（固定为1.0，DC电路）

项目的主要目的是演示如何配置硬件引脚、初始化功率测量系统以及从不同芯片类型中获取数据。

## 配置说明

### 芯片类型选择

在 `idf.py menuconfig` 中配置芯片类型：

```
Component config → power_measure → Power measurement chip type
```

可选择：

- **BL0937** - 单相功率测量IC，支持GPIO接口
- **INA236** - 精密数字功率监控器，支持I2C接口

### 硬件配置

选择芯片类型后，相应的硬件配置选项会自动显示：

#### BL0937 配置

- GPIO引脚配置（CF、SEL、CF1）
- 硬件参数配置（参考电压、采样电阻等）
- 校准因子配置（电压、电流、功率校准）

#### INA236 配置

- I2C引脚配置（SCL、SDA）
- I2C地址配置

## 硬件要求

### BL0937 配置

本示例使用 **BL0937** 功率测量芯片和 ESP32-C3(C2) 进行测试。要连接该芯片，须在 ESP32-C3(C2) 上配置以下引脚：

| 变量                | 默认GPIO引脚 | 芯片引脚 | 描述         |
| ------------------- | ------------ | -------- | ------------ |
| `BL0937_CF_GPIO`  | GPIO 3       | CF 引脚  | 电流频率输出 |
| `BL0937_SEL_GPIO` | GPIO 4       | SEL 引脚 | 选择引脚     |
| `BL0937_CF1_GPIO` | GPIO 7       | CF1 引脚 | 功率频率输出 |

**注意：** 可通过 `idf.py menuconfig` 在 Component config → power_measure → GPIO Pin Configuration 中配置GPIO引脚。

确保这些 GPIO 引脚正确连接到硬件设置中 **BL0937** 芯片上的相应引脚。

### INA236 配置

本示例还支持通过 I2C 接口连接 **INA236** 功率测量芯片。要连接该芯片，须配置以下引脚：

| 变量                  | 默认GPIO引脚 | 芯片引脚 | 描述      |
| --------------------- | ------------ | -------- | --------- |
| `I2C_MASTER_SDA_IO` | GPIO 20      | SDA 引脚 | I2C数据线 |
| `I2C_MASTER_SCL_IO` | GPIO 13      | SCL 引脚 | I2C时钟线 |

**INA236 I2C 配置：**

- I2C 地址：`0x41`（默认）
- I2C 时钟速度：`100kHz`
- 报警引脚：禁用（如需要可启用）

**注意：** 可通过 `idf.py menuconfig` 在 Component config → power_measure → GPIO Pin Configuration 中配置GPIO引脚。

确保这些 GPIO 引脚正确连接到硬件设置中 **INA236** 芯片上的相应引脚。

## BL0937 校准因子使用指南

### 校准因子概述

BL0937芯片的测量精度可以通过校准因子进行调整。校准因子包括：

- **KU (电压校准因子)**：用于调整电压测量精度，单位：us/V
- **KI (电流校准因子)**：用于调整电流测量精度，单位：us/A
- **KP (功率校准因子)**：用于调整功率测量精度，单位：us/W

### 校准因子工作原理

BL0937芯片通过测量脉冲频率来计算电气参数：

- 脉冲频率 → 校准因子 → 实际电气参数
- 校准因子本质上是频率到实际物理量的转换系数
- 校准因子需要根据实际硬件配置和测量精度需求进行调整

### 校准步骤

#### 1. 电压校准

1. **准备标准电压源**：使用万用表或标准电压源，确保电压值准确
2. **读取测量值**：
   ```c
   float measured_voltage;
   power_measure_get_voltage(power_handle, &measured_voltage);
   printf("测量电压: %.2f V\n", measured_voltage);
   ```
3. **计算校准因子**：
   ```c
   float standard_voltage = 220.0f;  // 标准电压值
   float calibration_factor = standard_voltage / measured_voltage;
   printf("电压校准因子: %.4f\n", calibration_factor);
   ```
4. **应用校准**：
   将上面计算获取的calibration_factor的值赋值给DEFAULT_KU

#### 2. 电流校准

1. **准备标准电流源**：使用电流表或标准电流源
2. **读取测量值**：

   ```c
   float measured_current;
   power_measure_get_current(power_handle, &measured_current);
   printf("测量电流: %.3f A\n", measured_current);
   ```
3. **计算校准因子**：

   ```c
   float standard_current = 15.0f;  // 标准电压值
   float calibration_factor = standard_current / measured_current;
   ```
4. **应用校准**：

   将上面计算获取的calibration_factor的值赋值给DEFAULT_KI

#### 3. 功率校准

1. **准备标准功率源**：使用功率计或已知功率的负载
2. **读取测量值**：
   ```c
   float measured_power;
   power_measure_get_active_power(power_handle, &measured_power);
   printf("测量功率: %.2f W\n", measured_power);
   ```
3. **计算校准因子：**
   ```c
   float standard_power = 20.0f;  // 标准功率值
   float calibration_factor = standard_power / measured_power;
   ```
4. **应用校准**：
   将上面计算获取的calibration_factor的值赋值给DEFAULT_KP

### 校准注意事项

1. **校准环境**：

   - 确保环境温度稳定
   - 避免电磁干扰
   - 使用稳定的电源
2. **校准顺序**：

   - 建议先校准电压，再校准电流，最后校准功率
   - 每次校准后等待测量稳定再进行下一步
3. **校准因子保存**：

   - 校准因子会保存在设备中，重启后仍然有效
   - 如需重置校准因子，可重新初始化设备

## 芯片选择

要在不同的功率测量芯片之间切换，请修改源代码中的演示宏：

```c
// 对于 BL0937
#define DEMO_BL0937    1
#define DEMO_INA236    0

// 对于 INA236  
#define DEMO_BL0937    0
#define DEMO_INA236    1
```

## 软件要求

* ESP-IDF（乐鑫物联网开发框架）
* 用于任务管理的 FreeRTOS
* 功率测量驱动（`power_measure.h`）

## GPIO配置

GPIO引脚可通过menuconfig配置，无需修改源代码：

```bash
idf.py menuconfig
# 导航到：Component config → power_measure → GPIO Pin Configuration
# 根据需要配置BL0937和INA236的GPIO引脚
```

这样您可以轻松地将示例适配到不同的硬件配置，而无需修改代码。

## 配置

可根据硬件与应用需要调整：

- **芯片类型选择**：通过 `DEMO_BL0937` 和 `DEMO_INA236` 宏在不同芯片间切换。
- **GPIO 与 I2C 引脚**：
  - BL0937：`BL0937_CF_GPIO`、`BL0937_SEL_GPIO`、`BL0937_CF1_GPIO`；
  - INA236：`I2C_MASTER_SDA_IO`、`I2C_MASTER_SCL_IO`、`I2C_MASTER_NUM`、`I2C_MASTER_FREQ_HZ`；
- **I2C 参数（INA236）**：地址通常为 `0x41`，频率 `100kHz`（可调整）；
- **校准参数（BL0937）**：`ki/ku/kp` 校准因子，可通过API函数动态调整或使用硬件标定结果；
- **阈值**：合理设置过流/过压/欠压以满足安全要求；
- **能量检测**：仅在支持的芯片上启用（示例中 INA236 关闭）。

## 日志

应用程序使用 `ESP_LOGI` 记录信息日志，使用 `ESP_LOGE` 记录错误日志。日志信息将显示在串行监视器或日志控制台中。

日志输出示例：

### BL0937 输出：

```
I (12345) PowerMeasureExample: Power Measure Example Starting...
I (12346) PowerMeasureExample: Selected chip: BL0937 (GPIO interface)
I (12347) PowerMeasureExample: GPIO pins - CF:3, SEL:4, CF1:7
I (12348) PowerMeasureExample: Power measurement initialized successfully
I (12349) PowerMeasureExample: Starting measurement loop...
I (12350) PowerMeasureExample: Voltage: 220.15 V
I (12351) PowerMeasureExample: Current: 0.05 A
I (12352) PowerMeasureExample: Power: 11.85 W
I (12353) PowerMeasureExample: Energy: 0.12 Kw/h
I (12354) PowerMeasureExample: ---
```

### BL0937 校准输出：

```
I (12345) PowerMeasureExample: 开始BL0937校准过程...
I (12346) PowerMeasureExample: 校准前电压: 225.30 V
I (12347) PowerMeasureExample: 校准后电压: 220.00 V
I (12348) PowerMeasureExample: 校准前电流: 1.45 A
I (12349) PowerMeasureExample: 校准后电流: 1.50 A
I (12350) PowerMeasureExample: 校准前功率: 340.20 W
I (12351) PowerMeasureExample: 校准后功率: 330.00 W
I (12352) PowerMeasureExample: BL0937校准完成
```

### INA236 输出：

```
I (12345) PowerMeasureExample: Power Measure Example Starting...
I (12346) PowerMeasureExample: Using chip type: INA236
I (12347) PowerMeasureExample: I2C bus initialized successfully
I (12348) PowerMeasureExample: INA236 Configuration: I2C_ADDR=0x41, SCL=13, SDA=20
I (12349) PowerMeasureExample: Power measurement initialized successfully
I (12350) PowerMeasureExample: Starting measurement loop...
I (12351) PowerMeasureExample: Voltage: 3.30 V
I (12352) PowerMeasureExample: Current: 125.50 mA
I (12353) PowerMeasureExample: Power: 0.41 W (calculated)
I (12354) PowerMeasureExample: Energy measurement not available for INA236
I (12355) PowerMeasureExample: ---
```
