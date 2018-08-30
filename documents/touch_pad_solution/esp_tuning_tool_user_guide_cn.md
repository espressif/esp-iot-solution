[[EN]](esp_tuning_tool_user_guide_en.md)

# ESP-Tuning Tool 使用手册

## 目录

- [ESP-Tuning Tool 简介](#简介)  

- [ESP-Tuning Tool 概述](#概述)  

- [软件界面介绍](#软件界面介绍)  

- [使用说明](#使用说明)  

    * [系统要求](#系统要求)  

    * [使用调试步骤](#使用调试步骤)   

- [相关程序介绍](#相关程序介绍)  

- [FAQs](#faqs)  

## 简介

在 Touchpad 传感器设计过程中，您需要监控 Touchpad 传感器数据（如原始计数值、基线值和计数差值），**评估触摸产品的性能（灵敏度，信噪比，通道干扰）**，以进行调校和调试。

此文档帮助您选择正确的工具以查看 Touchpad 传感器数据。ESP-Tuning Tool 是用于调试 Touchpad 传感器性能的专用工具。阅读此文档之前，您应已熟悉 Touchpad 传感器相关技术。如果您需要了解有关 Touchpad 传感器常规理论和操作的更多信息，请参见 [Touchpad 传感器入门](touch_sensor_design_cn.md)。

## 概述

ESP32 支持 UART 接口，用于监控 Touchpad 传感器数据。ESP32 中的数据通过 UART 接口被读取，并再通过 USB 接口发送到 PC，如图 1 所示，ESP-Tuning Tool 通过 ESP-Prog 从 ESP32 中收集 Touchpad 传感器数据并绘制成图表。

本文的例子使用下面的工具测试:

 - [ESP32-Sense Kit 开发板](../evaluation_boards/esp32_sense_kit_guide_cn.md)
 - [ESP-Prog 下载器](../evaluation_boards/ESP-Prog_guide_cn.md)
 - [ESP-Tuning Tool 软件](https://www.espressif.com/zh-hans/support/download/other-tools)

<div align="center"><img src="../_static/esp_tuning_tool/ESP_Tuning_tool.jpg" width = "1000" alt="touch_tune_tool" align=center /></div>  
<div align="center">图 1. 从 ESP32 中收集 Touchpad 传感器数据</div>  

## 软件界面介绍

### 首页

图 2 为 ESP-Tuning Tool 首页，分为四个部分：

1. 标题栏  
标题栏中可以选择显示 ESP-Tuning Tool 首页或者灵敏度测试页面。  

2. 通讯栏  
通讯栏包含：
    - 通讯相关参数配置：波特率、串口号
    - 设备信息：设备类型、Mac 地址
    - **Get Port** 按钮：重新获取端口列表
    - **Refresh** 按钮：重新与设备建立通讯连接

3. ESP32 管脚映射（ESP32 Pin Mapping）  
ESP32 管脚映射界面显示当前设备所使用的 pad。  

4. Touchpad 组合结构  
显示当前设备的 Touchpad 组合结构，点击某个组合之后会跳转到该组合的详细数据查看界面（如图 5 所示）。  

<div align="center"><img src="../_static/esp_tuning_tool/ESP_Tuning_tool_1.jpg" width = "800" alt="首页" align=center /></div>  
<div align="center">图 2. ESP-Tuning Tool 首页</div>  

### 灵敏度测试页面

图 4 为 ESP-Tuning Tool 灵敏度测试页面。通过此界面用户可以测试触摸产品的灵敏度，信噪比，通道干扰情况。触摸性能达到产品要求后，设置通道变化率到[触摸程序](../../examples/touch_pad_evb/main/evb.h)中。界面分为五个部分:

1. 标题栏  
标题栏中可以选择显示 ESP-Tuning Tool 首页或者灵敏度测试页面。  

2. 分析栏  
分析栏中包含通道号、对应 ESP-Tuning Tool 分析的数据，例如：信噪比 (SNR)，灵敏度 (Sensitivity)。  

3. 原始值条形图  
显示当前设备所用 Touchpad 通道的原始值条形图。条形图显示触摸通道的实时脉冲计数值。记录下各通道无触摸和触摸时的读数值，计算出**触摸变化率（（非触摸值-触摸值）/ 非触摸值 * 100%）**。触摸算法需要通道变化率来计算触摸阈值。触摸产品若具有较稳定的触摸体验，触摸变化率应大于 3%。

4. 原始值折线图  
显示当前设备所用 Touchpad 通道的原始值折线图。折线图可以显示触摸通道的历史数据。信噪比和通道干扰是评估触摸性能的重要参数。记录无触摸状态时读数的噪声幅值和触摸时读数变化量，计算得出通道的**信噪比（SNR = 变化量/噪声幅值）**，如图 3 所示。要求信噪比应大于 5:1。因为通道走线存在耦合现象，触摸某通道时会导致相邻通道读数值发生变化，这种现象是通道干扰。折线图也能评估通道干扰的问题。

    <div align="center"><img src="../_static/esp_tuning_tool/ESP_Tuning_Tool_SNR_Formula.jpg" width = "800" alt="SNR 计算" align=center /></div>
<div align="center">图 3. SNR 计算</div>

5. Touchpad 通道  
显示当前设备所用的 Touchpad 通道，并可以在复选框中勾选条形图和折线图中想要显示的 Touchpad 通道。  

<div align="center"><img src="../_static/esp_tuning_tool/ESP_Tuning_tool_2.jpg" width = "800" alt="灵敏度测试页面" align=center /></div>  
<div align="center">图 4. ESP-Tuning Tool 灵敏度测试页面</div>

### 详细数据页面

图 5 为 ESP-Tuning Tool 详细数据页面。检测触摸算法中的基线数据，触摸变化量，触摸阈值，触摸状态值等，评估触摸产品的各触摸参数是否正常运行。界面分为四个部分：

1. 操作按钮栏  
包含放大、恢复、开始、暂停、数据预览五个按钮  
    - 放大：点击放大按钮后，可在折线图中选择一块区域进行放大，便于查看
    - 恢复：点击恢复按钮之后，恢复之前的状态
    - 开始：点击开始按钮之后，开始显示实时数据
    - 暂停：点击暂停按钮之后，暂停显示实时数据
    - 数据预览：点击数据预览按钮之后，当鼠标移动到折线图中的某个点时，会显示该时间点所有被选择通道的数据

2. 原始值折线图  
    - 显示当前 Touchpad 组合所用通道的原始值折线图
    - 原始值折线图中包含所用通道的基线值
    - 在原始数据折线图下方和右方有显示区域滑条

3. Touchpad 组合状态/位置折线图  
矩阵按键或由多个独立按键构成的按键组合时，此图表显示按键状态。0 为无按压，1 为按键被按下。滑条按键时，此图表显示滑条滑动轨迹，并记录上次滑条最后停在的位置。

4. Touchpad 组合所用通道  
显示当前 Touchpad 组合所用的通道，并可以在复选框中勾选折线图中想要显示的 Touchpad 通道。

<div align="center"><img src="../_static/esp_tuning_tool/ESP_Tuning_tool_3.jpg" width = "800" alt="详细数据页面" align=center /></div>  
<div align="center">图 5. ESP-Tuning Tool 详细数据页面</div>  

## 使用说明

### 系统要求

 - 当前版本支持在 **Linux** 和 **Windows** 下运行。请依据电脑系统下载对应版本的软件。

### 使用调试步骤

步骤 1：Touchpad 设备准备

 - 烧录示例 [examples/touch_pad_evb](https://github.com/espressif/esp-iot-solution/tree/master/examples/touch_pad_evb) 到 ESP32-Sense Kit 开发板，在烧录前需要确定在 **menuconfig** 已经打开使用 ESP-Tuning Tool 调试功能，配置路径：`IoT Solution settings > IoT Components Management > General functions > Touch Sensor`

步骤 2：下载 ESP-Tuning Tool

 - 根据电脑系统下载对应版本的软件

步骤 3：连接 Touchpad 设备

 - ESP-Prog 一端通过 USB 连接到电脑
 - ESP-Prog 另一端连接到 ESP32-Sense Kit

步骤 4：打开 ESP-Tuning Tool 读取 Touchpad 传感器数据

 - 软件打开时工具会自动获取现有串口信息，并且自动尝试连接，获取 Touchpad 传感器数据
 - 如果尝试连接的端口没有响应，可以尝试[手动连接](#faqs)
 - 成功获取串口信息后，主界面左侧通讯栏会显示 **MAC地址**，**设备类型**，右侧工具栏会显示 **Touchpad 组合结构**

|<img src="../_static/esp_tuning_tool/ESP_Tuning_Tool_Com.jpg" width = "200" alt="MAC地址" align=center />|<img src="../_static/esp_tuning_tool/ESP_Tuning_Tool_Comb.jpg" width = "200" alt="Touchpad 组合结构" align=center />|
|:---:|:---:|
|通讯栏|Touchpad 组合结构|

步骤 5：根据 Touchpad 传感器数据修改 Touchpad 传感器相关参数

 - 在查看 Touchpad 传感器 SNR 图表之后，若没有达到理想的触摸效果，可以修改 Touchpad 的参数以达到最佳触摸效果
 - 观察折线图得到 Touchpad 触摸时和未触摸时的原始值，根据触摸时和未触摸时这两个原始值修改 Touchpad 的参数
 - 如下图所示，在 SNR 图标中可以通过数据预览功能获得触摸时和未触摸时的原始值，将对应 Touchpad 通道的触摸变化率修改为:**（非触摸值-触摸值）/ 非触摸值**

步骤 6：评估触摸性能

 - 根据 ESP-Tuning Tool 绘制的折线图，观察 Touchpad 原始值的变化量和 Touchpad 状态判断 Touchpad 的灵敏度

<div align="center"><img src="../_static/esp_tuning_tool/ESP_Tuning_Tool_SNR.jpg" width = "800" alt="灵敏度测试" align=center /></div>  
<div align="center">图 6. ESP-Tuning Tool 灵敏度测试</div>  

Touchpad 传感器相关参数如下：
```
/*
 * Spring button threshold settings.
 * Store the max change rate of the reading value when a touch event occurs.
 * Decreasing this threshold appropriately gives higher sensitivity.
 * If the value is less than 0.1 (10%), leave at least 4 decimal places.
 * Calculation formula: (non-trigger value - trigger value) / non-trigger value.
 * */
#if COVER_THICK_SPRING_BUTTON <= 1          /*!< Plastic cover thickness is 0 ~ 1 mm */
    #define SPRING_BUTTON_MAX_CHANGE_RATE_0    0.1129   // (1196-1061) / 1196 = 0.1129
    #define SPRING_BUTTON_MAX_CHANGE_RATE_1    0.1029   // (1215-1090) / 1215 = 0.1029
    #define SPRING_BUTTON_MAX_CHANGE_RATE_2    0.0950   // (1053-953 ) / 1053 = 0.0950
    #define SPRING_BUTTON_MAX_CHANGE_RATE_3    0.0856   // (1110-1015) / 1110 = 0.0856
    #define SPRING_BUTTON_MAX_CHANGE_RATE_4    0.0883   // (1132-1032) / 1132 = 0.0883
    #define SPRING_BUTTON_MAX_CHANGE_RATE_5    0.0862   // (986 -901 ) / 986  = 0.0862
```

## 相关程序介绍

1. UART 初始化 - 波特率、数据位、停止位等  
2. 设置设备信息 - 设备 ID、版本号、MAC 地址  
3. 设置 Touchpad 参数 - 滤波器阈值、消抖动延时、基线值等  
4. 添加 Touchpad 组合结构 - 所用通道、组合信息等  
5. 设置 Touchpad 数据 - 通道、原始值、基线值等  
6. ESP-Tuning Tool 任务创建 - 接收任务、发送任务  

### UART 初始化

 - 在 **menuconfig** 中可以修改 UART 默认配置 - UART NUM、波特率等

```
void uart_init()
{
    uart_config_t uart_config = {
        .baud_rate = UART_BAUD_RATE，
        .data_bits = UART_DATA_8_BITS，
        .parity    = UART_PARITY_DISABLE，
        .stop_bits = UART_STOP_BITS_1，
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };
    uart_param_config(UART_PORT_NUM, &uart_config);
    uart_set_pin(UART_PORT_NUM, UART_TXD_PIN, UART_RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(UART_PORT_NUM, 2 * 100, 2 * 100, 0, NULL, 0);
}
```

### 设置设备信息

 - `tune_tool_set_device_info()` 函数的作用是设置 ESP-Tuning Tool 中的设备信息 - 设备 ID、版本号、MAC 地址
 - `tune_dev_info_t` 结构体中包含需要设置的设备信息

```
typedef struct {
    tune_dev_cid_t dev_cid;
    tune_dev_ver_t dev_ver;
    uint8_t dev_mac[6];
} tune_dev_info_t;

esp_err_t tune_tool_set_device_info(tune_dev_info_t *dev_info);
```

### 设置 Touchpad 参数

 - `tune_tool_set_device_parameter()` 函数的作用是设置 Touchpad 参数 - 滤波器阈值、消抖动延时、基线值等
 - `tune_dev_parameter_t` 结构体中包含需要设置的 Touchpad 参数

```
typedef struct {
    uint16_t filter_period;     //TOUCHPAD_FILTER_IDLE_PERIOD
    uint8_t debounce_ms;        //TOUCHPAD_STATE_SWITCH_DEBOUNCE
    uint8_t base_reset_cnt;     //TOUCHPAD_BASELINE_RESET_COUNT_THRESHOLD
    uint16_t base_update_cnt;   //TOUCHPAD_BASELINE_UPDATE_COUNT_THRESHOLD
    uint8_t touch_th;           //TOUCHPAD_TOUCH_THRESHOLD_PERCENT
    uint8_t noise_th;           //TOUCHPAD_NOISE_THRESHOLD_PERCENT
    uint8_t hys_th;             //TOUCHPAD_HYSTERESIS_THRESHOLD_PERCENT
    uint8_t base_reset_th;      //TOUCHPAD_BASELINE_RESET_THRESHOLD_PERCENT
    uint8_t base_slider_th;     //TOUCHPAD_SLIDER_TRIGGER_THRESHOLD_PERCENT
} tune_dev_parameter_t;

esp_err_t tune_tool_set_device_parameter(tune_dev_parameter_t *dev_para);
```

### 添加 Touchpad 组合结构

 - 在 Touchpad 初始化完成之后，应调用 `tune_tool_add_device_setting()` 函数添加想要监视的 Touchpad 组合
 - `tune_dev_comb_t` 结构体中包含 Touchpad 组合信息 - 组合类型、使用的 Touchpad 通道等
 - `tune_dev_setting_t` 结构体中包含需要监视的所有 Touchpad 组合信息 - Touchpad 组合、这些组合使用的 Touchpad 通道

```
typedef struct {
    tune_dev_char_t dev_comb;
    uint8_t ch_num_h;
    uint8_t ch_num_l;
    uint8_t ch[25];
} tune_dev_comb_t;

typedef struct {
    uint32_t ch_bits;
    tune_dev_comb_t dev_comb[10];
} tune_dev_setting_t;

esp_err_t tune_tool_add_device_setting(tune_dev_comb_t *ch_comb);
```

### 设置 Touchpad 数据

 - `tune_tool_set_device_data()` 函数的作用是更新将要发送到 ESP-Tuning Tool 的数据
 - `tune_dev_data_t` 结构体中包含发送到 ESP-Tuning Tool 的 Touchpad 数据

```
typedef struct {
    uint8_t ch;
    uint16_t raw;
    uint16_t baseline;
    uint16_t diff;
    uint16_t status; //if the button is slide, the value of status if the position of slide.
                     //else the value of status is the state of button.
} tune_dev_data_t;

esp_err_t tune_tool_set_device_data(tune_dev_data_t *dev_data);
```

### ESP-Tuning Tool 任务创建

 - `touch_tune_tool_task_create()` 函数的作用是创建 ESP-Tuning Tool 数据接收任务和数据发送任务

```
void touch_tune_tool_task_create()
{
    xTaskCreate(&touch_tune_tool_read_task, "touch_tune_tool_read_task", 2048, NULL, 5, NULL);
    xTaskCreate(&touch_tune_tool_write_task, "touch_tune_tool_write_task", 2048, NULL, 5, NULL);
}

```

## FAQs

 - 打开串口错误，请确认当前用户有串口使用权限（Linux 系统下需要增加串口权限）
 - 插入新设备后，在端口列表中如果没有该串口号，需要点击 **Get Port** 按钮刷新端口列表
 - 若获取串口设备失败，在选择正确的串口后，点击 **Refresh** 按钮即可尝试重新发送串口连接请求
 - 若端口列表中没有期待监控的串口，在确认设备已经插入以后并选择正确的串口号和波特率，点击 **Get Port** 按钮刷新端口列表，新的串口设备将会显示在列表中
