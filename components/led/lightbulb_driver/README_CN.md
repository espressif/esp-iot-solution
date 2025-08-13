# 球泡灯组件概述

👉 [English Version](./README.md)

球泡灯组件将球泡灯中常用的多款调光方案做了封装，使用一个抽象层管理这些方案，便于开发者集成到自己的应用程序中，目前所有 ESP32 系列芯片都已经完成支持。

## 已支持的调光方案如下

- PWM 方案

  - RGB + C/W
  - RGB + CCT/Brightness
- IIC 调光芯片方案

  - ~~SM2135E~~
  - SM2135EH
  - SM2182E
  - SM2X35EGH (SM2235EGH/SM2335EGH)
  - BP57x8D (BP5758/BP5758D/BP5768)
  - BP1658CJ
  - KP18058
- 单总线方案

  - WS2812
  - SM16825E

## 已支持的常用功能如下

- 动效：支持使用渐变切换各种颜色，支持配置周期性的呼吸及闪烁效果
- 校准：支持使用系数微调输出数据实现白平衡功能，支持使用 gamma 校准曲线
- 状态存储：使用 `NVS` 组件存储球泡灯当前状态，方便实现断电记忆等功能
- 灯珠配置：支持最多 5 种灯珠，可配置的组合如下：
  - 单路，冷或暖色温灯珠，可以完成单色温下的亮度控制
  - 双路，冷暖灯珠，可以完成色温与亮度的控制
  - 三路，红色、绿色、蓝色灯珠，可以完成任意颜色控制
  - 四路，红色、绿色、蓝色、冷色或暖色灯珠，可以完成颜色与单一色温下的亮度控制，如果配置了混色表格，则支持使用这些灯珠混出不同色温，实现色温控制
  - 五路，红色、绿色、蓝色、冷色、暖色灯珠，可以完成颜色与色温下的亮度控制
- 功率限制：平衡不同色温，颜色下的输出功率
- 低功耗：在不影响动效的情况下减少整体功耗
- 软件色温：适用于不进行硬件调整色温的 PWM 驱动

## PWM 方案使用示例

PWM 方案使用 LEDC 驱动实现，支持配置软件/硬件渐变功能，支持根据频率自动配置分辨率，使用实例如下：

```c
lightbulb_config_t config = {
    //1. 选择 PWM 输出并进行参数配置
    .type = DRIVER_ESP_PWM,
    .driver_conf.pwm.freq_hz = 4000,

    //2. 功能选择，根据你的需要启用/禁用
    .capability.enable_fade = true,
    .capability.fade_time_ms = 800,
    .capability.enable_lowpower = false,
    /* 如果你的驱动白光输出为硬件单独控制而不是软件混色，需要启用此功能 */
    .capability.enable_hardware_cct = true,
    .capability.enable_status_storage = true,
    /* 用于配置 LED 灯珠组合 */
    .capability.led_beads = LED_BEADS_3CH_RGB,
    .capability.storage_cb = NULL,
    .capability.sync_change_brightness_value = true,

    //3. 配置 PWM 输出的硬件管脚
    .io_conf.pwm_io.red = 25,
    .io_conf.pwm_io.green = 26,
    .io_conf.pwm_io.blue = 27,

    //4. 限制参数，使用细则请参考后面小节
    .external_limit = NULL,

    //5. 颜色校准参数
    .gamma_conf = NULL,

    //6. 初始化照明参数，如果 on 置位将在初始化驱动时点亮球泡灯
    .init_status.mode = WORK_COLOR,
    .init_status.on = true,
    .init_status.hue = 0,
    .init_status.saturation = 100,
    .init_status.value = 100,
};
lightbulb_init(&config);
```

## IIC 调光芯片方案使用示例

IIC 调光芯片方案已支持配置 IIC 调光芯片的所有参数，调光芯片的具体功能及参数请参阅手册填写.使用实例如下：

```c
lightbulb_config_t config = {
    //1. 选择需要的芯片并进行参数配置，每款芯片配置的参数存在不同，请仔细参阅芯片手册
    .type = DRIVER_BP57x8D,
    .driver_conf.bp57x8d.freq_khz = 300,
    .driver_conf.bp57x8d.enable_iic_queue = true,
    .driver_conf.bp57x8d.iic_clk = 4,
    .driver_conf.bp57x8d.iic_sda = 5,
    .driver_conf.bp57x8d.current = {50, 50, 60, 30, 50},

    //2. 驱动功能选择，根据你的需要启用/禁用
    .capability.enable_fade = true,
    .capability.fade_time_ms = 800,
    .capability.enable_lowpower = false,

    .capability.enable_status_storage = true,
    .capability.led_beads = LED_BEADS_5CH_RGBCW,
    .capability.storage_cb = NULL,
    .capability.sync_change_brightness_value = true,

    //3. 配置 IIC 芯片的硬件管脚
    .io_conf.iic_io.red = OUT3,
    .io_conf.iic_io.green = OUT2,
    .io_conf.iic_io.blue = OUT1,
    .io_conf.iic_io.cold_white = OUT5,
    .io_conf.iic_io.warm_yellow = OUT4,

    //4. 限制参数，使用细则请参考后面小节
    .external_limit = NULL,

    //5. 颜色校准参数
    .gamma_conf = NULL,

    //6. 初始化照明参数，如果 on 置位将在初始化驱动时点亮球泡灯
    .init_status.mode = WORK_COLOR,
    .init_status.on = true,
    .init_status.hue = 0,
    .init_status.saturation = 100,
    .init_status.value = 100,
};
lightbulb_init(&config);
```

## 单总线方案使用示例

### WS2812 使用示例

单总线方案之一使用 SPI 驱动输出 WS2812 所需要的数据，数据封装顺序为 GRB。

```c
lightbulb_config_t config = {
    //1. 选择 WS2812 输出并进行参数配置
    .type = DRIVER_WS2812,
    .driver_conf.ws2812.led_num = 22,
    .driver_conf.ws2812.ctrl_io = 4,

    //2. 驱动功能选择，根据你的需要启用/禁用
    .capability.enable_fade = true,
    .capability.fade_time_ms = 800,
    .capability.enable_status_storage = true,

    /* 对于 WS2812 只能选择 LED_BEADS_3CH_RGB */
    .capability.led_beads = LED_BEADS_3CH_RGB,
    .capability.storage_cb = NULL,

    //3. 限制参数，使用细则请参考后面小节
    .external_limit = NULL,

    //4. 颜色校准参数
    .gamma_conf = NULL,

    //5. 初始化照明参数，如果 on 置位将在初始化驱动时点亮球泡灯
    .init_status.mode = WORK_COLOR,
    .init_status.on = true,
    .init_status.hue = 0,
    .init_status.saturation = 100,
    .init_status.value = 100,
};
lightbulb_init(&config);
```

### SM16825E 使用示例

SM16825E 是一款支持 RGBWY 五通道的 LED 驱动器，使用 SPI 接口和 RZ 编码协议，支持 16 位灰度控制和电流调节。

```cpp
lightbulb_config_t config = {
    //1. 选择 SM16825E 输出并进行参数配置
    .type = DRIVER_SM16825E,
    .driver_conf.sm16825e.led_num = 1,        // LED 芯片数量
    .driver_conf.sm16825e.ctrl_io = 9,        // 控制 GPIO 引脚
    .driver_conf.sm16825e.freq_hz = 3333000,  // SPI 频率 (默认 3.33MHz，基于 RZ 协议时序要求自动计算)

    //2. 驱动功能选择，根据你的需要启用/禁用
    .capability.enable_fade = true,
    .capability.fade_time_ms = 800,
    .capability.enable_lowpower = false,
    .capability.enable_status_storage = true,
    .capability.led_beads = LED_BEADS_5CH_RGBCW,  // 支持 5 通道 RGBWY
    .capability.storage_cb = NULL,
    .capability.sync_change_brightness_value = true,

    //3. 限制参数，使用细则请参考后面小节
    .external_limit = NULL,

    //4. 颜色校准参数
    .gamma_conf = NULL,

    //5. 初始化照明参数，如果 on 置位将在初始化驱动时点亮球泡灯
    .init_status.mode = WORK_COLOR,
    .init_status.on = true,
    .init_status.hue = 0,
    .init_status.saturation = 100,
    .init_status.value = 100,
};
lightbulb_init(&config);

// 可选：配置通道映射（如果需要自定义引脚映射）
sm16825e_regist_channel(SM16825E_CHANNEL_R, SM16825E_PIN_OUTR);
sm16825e_regist_channel(SM16825E_CHANNEL_G, SM16825E_PIN_OUTG);
sm16825e_regist_channel(SM16825E_CHANNEL_B, SM16825E_PIN_OUTB);
sm16825e_regist_channel(SM16825E_CHANNEL_W, SM16825E_PIN_OUTW);
sm16825e_regist_channel(SM16825E_CHANNEL_Y, SM16825E_PIN_OUTY);

// 可选：设置通道电流 (10-300mA)
sm16825e_set_channel_current(SM16825E_CHANNEL_R, 100);  // 100mA
sm16825e_set_channel_current(SM16825E_CHANNEL_G, 100);  // 100mA
sm16825e_set_channel_current(SM16825E_CHANNEL_B, 100);  // 100mA
sm16825e_set_channel_current(SM16825E_CHANNEL_W, 100);  // 100mA
sm16825e_set_channel_current(SM16825E_CHANNEL_Y, 100);  // 100mA

// 可选：启用/禁用待机模式
sm16825e_set_standby(false);  // 禁用待机模式，启用正常工作

// 可选：关闭所有通道
// sm16825e_set_shutdown();
```

**SM16825E 特性：**

- 支持 RGBWY 五通道独立控制
- 16 位灰度分辨率 (65,536 级)
- 可调节电流控制 (每通道 10-300mA)
- 支持待机模式 (功耗 <2mW)
- RZ 编码协议，800Kbps 数据传输率，1200ns 码周期
- 支持多芯片级联
- 自动时序计算：基于数据手册参数动态计算 SPI 频率和位模式
- 灵活的通道映射：支持逻辑通道到物理引脚的任意映射
- 优化的 SPI 传输：使用 3.33MHz SPI 频率，每个 SM16825E 位用 4 个 SPI 位编码

#### SM16825E 高级功能

##### 通道映射配置

SM16825E 支持灵活的通道映射，可以将逻辑通道（R、G、B、W、Y）映射到任意物理引脚：

```c
// 自定义通道映射示例
sm16825e_regist_channel(SM16825E_CHANNEL_R, SM16825E_PIN_OUT1);  // 红色映射到 OUT1
sm16825e_regist_channel(SM16825E_CHANNEL_G, SM16825E_PIN_OUT2);  // 绿色映射到 OUT2
sm16825e_regist_channel(SM16825E_CHANNEL_B, SM16825E_PIN_OUT3);  // 蓝色映射到 OUT3
sm16825e_regist_channel(SM16825E_CHANNEL_W, SM16825E_PIN_OUT4);  // 白色映射到 OUT4
sm16825e_regist_channel(SM16825E_CHANNEL_Y, SM16825E_PIN_OUT5);  // 黄色映射到 OUT5
```

##### 电流控制

每个通道可以独立设置电流值，范围 10-300mA：

```c
// 为不同通道设置电流
sm16825e_set_channel_current(SM16825E_CHANNEL_R, config->current[SM16825E_CHANNEL_R]);  // 红色通道
sm16825e_set_channel_current(SM16825E_CHANNEL_G, config->current[SM16825E_CHANNEL_G]);  // 绿色通道
sm16825e_set_channel_current(SM16825E_CHANNEL_B, config->current[SM16825E_CHANNEL_B]);  // 蓝色通道
sm16825e_set_channel_current(SM16825E_CHANNEL_W, config->current[SM16825E_CHANNEL_W]);  // 白色通道
sm16825e_set_channel_current(SM16825E_CHANNEL_Y, config->current[SM16825E_CHANNEL_Y]);  // 黄色通道
```

##### 待机模式控制

支持待机模式以降低功耗：

```c
// 启用待机模式（功耗 <2mW）
sm16825e_set_standby(true);

// 禁用待机模式，恢复正常工作
sm16825e_set_standby(false);
```

##### 直接通道控制

可以直接设置 RGBWY 各通道的值（0-255）：

```c
// 设置各通道值
sm16825e_set_rgbwy_channel(255, 128, 64, 192, 96);
// 参数顺序：红色、绿色、蓝色、白色、黄色

// 关闭所有通道
sm16825e_set_shutdown();
```

##### 时序优化说明

驱动基于数据手册参数自动计算最优时序：

- **RZ 编码**：800Kbps 有效传输率，1200ns 码周期
- **SPI 频率**：3.33MHz，每个 SM16825E 位用 4 个 SPI 位编码
- **时序参数**：
  - T0 位：300ns 高电平 + 900ns 低电平
  - T1 位：900ns 高电平 + 300ns 低电平
  - 复位信号：最小 200μs 低电平

## 限制参数使用说明

限制参数主要用途为限制输出的最大功率，以及将亮度参数限制在一个区间。该组件彩光与白光可独立控制，所以存在 2 组最大/最小亮度参数及功率参数。彩光使用 `HSV` 模型，`value` 代表彩光亮度，白光使用 `brightness` 参数。`value` 与 `brightness` 数据输入范围为 0 <= x <= 100。

如果设置了亮度限制参数，那么将对输入值进行等比例缩放，例如，我们设置了下面这些参数

```c
lightbulb_power_limit_t limit = {
    .white_max_brightness = 100,
    .white_min_brightness = 10,
    .color_max_value = 100,
    .color_min_value = 10,
    .white_max_power = 100,
    .color_max_power = 100
}
```

`value` 与 `brightness` 输入与输出的关系如下：

```c
input   output
100      100
80       82
50       55
11       19
1        10
0        0
```

功率限制在亮度参数限制后进一步进行，对于 RGB 通道调整区间为 100 <= x <= 300，输入与输出的关系如下：

|    input    | output(color_max_power = 100) | output(color_max_power = 200) | output(color_max_power = 300) |
| :---------: | :---------------------------: | :---------------------------: | :---------------------------: |
|  255,255,0  |           127,127,0           |           255,255,0           |           255,255,0           |
|  127,127,0  |            63,63,0            |           127,127,0           |           127,127,0           |
|   63,63,0   |            31,31,0            |            63,63,0            |            63,63,0            |
| 255,255,255 |           85,85,85           |          170,170,170          |          255,255,255          |
| 127,127,127 |           42,42,42           |           84,84,84           |          127,127,127          |
|  63,63,63  |           21,21,21           |           42,42,42           |           63,63,63           |

## 颜色校准参数

颜色校准参数由 2 个部分组成，用于生成 gamma 灰度表的曲线系数 `curve_coefficient` 及用于做最终调整的白平衡系数 `balance_coefficient`。

- 曲线系数的取值在 0.8 <= x <= 2.2，各参数输出如下

```c

   |  x = 1.0                           | x < 1.0                          | x > 1.0
max|                                    |                                  |
   |                                *   |                     *            |                           *
   |                             *      |                  *               |                          *
   |                          *         |               *                  |                         *
   |                       *            |             *                    |                       *
   |                    *               |           *                      |                     *
   |                 *                  |         *                        |                   *
   |              *                     |       *                          |                 *
   |           *                        |     *                            |              *
   |        *                           |    *                             |           * 
   |     *                              |   *                              |        *
   |  *                                 |  *                               |  *
0  |------------------------------------|----------------------------------|------------------------------   
  0               ...                255 
```

- 白平衡系数的取值在 0.5-1.0，对数据进行最后微调，计算规则为 `输出值 = 输入值 * 系数`，可以为每个通道设置不同的系数。

## 示例代码

[点击此处](https://github.com/espressif/esp-iot-solution/tree/master/examples/lighting/lightbulb) 获取示例代码及使用说明。
