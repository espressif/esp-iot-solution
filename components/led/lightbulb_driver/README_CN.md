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

单总线方案使用 SPI 驱动输出 WS2812 所需要的数据，数据封装顺序为 GRB。

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

```c
input           output(color_max_power = 100)       output(color_max_power = 200)       output(color_max_power = 300)
255,255,0       127,127,0                           255,255,0                           255,255,0
127,127,0       63,63,0                             127,127,0                           127,127,0
63,63,0         31,31,0                             63,63,0                             63,63,0
255,255,255     85,85,85                            170,170,170                         255,255,255
127,127,127     42,42,42                            84,84,84                            127,127,127
63,63,63        21,21,21                            42,42,42                            63,63,63

```

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
