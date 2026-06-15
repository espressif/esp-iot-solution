# DALI 总线控制器驱动（`esp_dali`）

[![Component Registry](https://components.espressif.com/components/espressif/esp_dali/badge.svg)](https://components.espressif.com/components/espressif/esp_dali)

基于 ESP-IDF 的 DALI（数字可寻址照明接口，IEC 62386）主站控制器驱动。该驱动使用片上 **RMT（远程控制收发器）** 外设生成和解码曼彻斯特编码的 DALI 物理层，无需外部定时逻辑。

---

## 功能特性
    
- **完整 IEC 62386 物理层** — 曼彻斯特编码，Te = 416.67 µs，±10% 容差窗口。
- **前向帧 TX** — 发送 16 位 DALI 前向帧（地址字节 + 命令/数据字节）。
- **后向帧 RX** — 接收并解码来自从设备的 8 位 DALI 后向帧。
- **全部寻址模式** — 短地址（0–63）、组地址（0–15）、广播及特殊命令寻址。
- **IEC 62386-102 控制装置** — 灯具的亮度控制、开关、场景、配置及查询命令。
- **IEC 62386-103 控制设备** — 输入设备（传感器）支持，含事件上报和 commission 功能。
- **配置命令** — RESET、STORE_DTR_AS_*、ADD/REMOVE 组、场景存储。
- **查询命令** — QUERY_STATUS、QUERY_ACTUAL_LEVEL、QUERY_VERSION 及所有 IEC 62386-102/103 查询命令。
- **特殊（调试）命令** — INITIALIZE、RANDOMIZE、COMPARE、PROGRAM_SHORT_ADDRESS 等。
- **Commissioning** — 自动短地址分配，支持控制装置（102）和控制设备（103）两种类型。
- **send-twice 自动重发** — 对需要连续两次前向帧的命令自动在 100 ms 内重发。
- **自动帧间隔** — 每次事务结束后驱动自动插入 IEC 62386 要求的最小帧间隔（> 22 Te），无需用户手动延时。
- **IEC 62386-209 彩色控制** — 通过 `dali_master_set_color()` 支持三种彩色模式：色温（CCT/Mirek）、RGB 三通道和完整 RGBWAF 六通道，自动处理 DTR 预载、`ENABLE_DEVICE_TYPE 8`、`ACTIVATE` 等必要时序。
- **handle 化 API** — `dali_new_master_rmt()` 返回独立实例句柄，支持在不同 GPIO 对上同时运行多个实例。
- **兼容 ESP-IDF v5.0+** — 使用新版 `esp_driver_rmt` 组件 API。

---

## 支持目标

| ESP32 | ESP32-S2 | ESP32-S3 | ESP32-C3 | ESP32-C6 | ESP32-P4 | ESP32-H2 |
|:-----:|:--------:|:--------:|:--------:|:--------:|:--------:|:--------:|
| 是    | 是       | 是       | 是       | 是       | 是       | 是       |

---

## 依赖要求

- **ESP-IDF** v5.5.0 或更高版本
- **两个 GPIO 引脚** — 一个用于 TX（DALI 总线驱动），一个用于 RX（DALI 总线采样）
- **外部 DALI 接口电路** — GPIO 信号为 3.3 V 逻辑电平；需要符合 DALI 规范的总线收发器（例如基于限流开漏驱动和光耦的电路）来与 9.5 V–22.5 V 的 DALI 总线对接。

---

## 配置步骤

通过 menuconfig 启用所需的 DALI 协议部分：

```bash
idf.py menuconfig
# → Component config → DALI Component Configuration
```

| 配置项 | 说明 | 默认 |
|--------|------|------|
| `DALI_PART102_ENABLED` | 控制装置（灯具、驱动） | y |
| `DALI_PART209_ENABLED` | DT8 彩色控制（依赖 Part 102） | y |
| `DALI_PART103_ENABLED` | 输入设备（传感器） | y |
| `DALI_PART303_304_ENABLED` | 人体感应/光照传感器辅助功能（依赖 Part 103） | y |

> **注意：** Part 101（物理层）始终启用。禁用不需要的功能可减少代码体积。

---

## API 概览

### 包含头文件

```c
#include "dali.h"          /* 驱动 API 和常量  */
#include "dali_command.h"  /* DALI 命令码宏定义 */
```

### 初始化

```c
dali_master_handle_t dali;
dali_master_config_t cfg = {
    .rx_gpio = GPIO_NUM_4,
    .tx_gpio = GPIO_NUM_5,
    .invert_tx = false,   /* TX 硬件链路为奇数次反相时启用 */
    .invert_rx = false,   /* RX 硬件链路为奇数次反相时启用 */
};
dali_master_rmt_config_t rmt_cfg = {
    .mem_block_symbols = 64,
};
ESP_ERROR_CHECK(dali_new_master_rmt(&cfg, &rmt_cfg, &dali));
```

### 发送命令（无回复）

```c
dali_master_transaction_config_t tx = {
    .addr_type = DALI_ADDR_BROADCAST, .addr = 0,
    .is_cmd = true, .command = DALI_CMD_OFF,
    .send_twice = false, .tx_timeout_ms = DALI_TX_TIMEOUT_MS,
};
dali_master_do_transaction(dali, &tx, NULL);
/* 帧间隔由驱动自动插入，无需手动延时 */
```

### 直接弧功率控制（DAPC）

```c
dali_master_transaction_config_t tx = {
    .addr_type = DALI_ADDR_SHORT, .addr = 0,
    .is_cmd = false, .command = 200,
    .send_twice = false, .tx_timeout_ms = DALI_TX_TIMEOUT_MS,
};
dali_master_do_transaction(dali, &tx, NULL);
```

### 查询设备（有回复）

```c
dali_master_transaction_config_t tx = {
    .addr_type = DALI_ADDR_SHORT, .addr = 0,
    .is_cmd = true, .command = DALI_CMD_QUERY_ACTUAL_LEVEL,
    .send_twice = false, .tx_timeout_ms = DALI_TX_TIMEOUT_MS,
};
int reply;
esp_err_t err = dali_master_do_transaction(dali, &tx, &reply);
if (err == ESP_OK && DALI_RESULT_IS_VALID(reply)) {
    ESP_LOGI(TAG, "当前亮度: 0x%02X", (uint8_t)reply);
}
```

### 地址分配（Commissioning）

**控制装置（IEC 62386-102 灯具）：**

```c
uint8_t count = 0;
esp_err_t err = dali_commission(dali, DALI_COMMISSION_ALL, 0, 64, &count,
                                DALI_TX_TIMEOUT_MS);
if (err == ESP_OK) {
    ESP_LOGI(TAG, "Commissioning 完成，共分配 %d 台设备", count);
}
```

**控制设备（IEC 62386-103 传感器）：**

```c
uint8_t count = 0;
esp_err_t err = dali_103_commission(dali, DALI_COMMISSION_ALL, 0, 64, &count,
                                    DALI_TX_TIMEOUT_MS);
if (err == ESP_OK) {
    ESP_LOGI(TAG, "输入设备 commissioning 完成：%d 个", count);
}
```

> **静默模式说明：** 两个 commissioning 函数完成后都会自动将 103 输入设备置于**静默模式（quiescent mode）**。这可以防止103传感器立即向总线发送事件帧，避免某些 102 灯具将其误解析为 DAPC 亮度命令。当需要接收传感器事件时，调用 `dali_103_do_device_command(dali, DALI_ADDR_BROADCAST, 0, DALI_103_STOP_QUIESCENT_MODE, true, timeout, NULL)` 退出静默模式。

### Part 103 输入设备 API

Part 103 输入设备使用与 Part 102 灯具不同的 24-bit 帧格式。包含头文件：

```c
#include "control_device.h"
```

**主要函数：**

| 函数 | 用途 |
|------|------|
| `dali_103_commission()` | 为输入设备分配短地址 |
| `dali_103_do_device_command()` | 发送设备级命令（如 启停静默模式） |
| `dali_103_do_instance_command()` | 发送实例级命令（如 Part 303/304 传感器命令） |
| `dali_103_query_device_status()` | 查询设备状态字节 |
| `dali_103_query_number_of_instances()` | 查询设备拥有的传感器实例数量 |
| `dali_103_query_instance_type()` | 查询指定实例的类型（如 occupancy=3, light=4） |
| `dali_103_reset_device()` | 恢复设备出厂设置 |

### Part 303/304 传感器 API

针对实现 Part 303（人体感应）或 Part 304（光照）传感器实例的输入设备：

```c
#include "device_sensors.h"
```

**Part 303 — 人体感应传感器：**

| 函数 | 用途 |
|------|------|
| `dali_303_query_occupancy()` | 查询当前 occupancy 状态（有人/无人） |
| `dali_303_query_hold_timer()` | 查询 hold timer 值 |
| `dali_303_set_hold_timer()` | 设置 hold timer（单位：10秒） |
| `dali_303_query_deadtime_timer()` | 查询 deadtime timer 值 |

**Part 304 — 光照传感器：**

| 函数 | 用途 |
|------|------|
| `dali_304_query_hysteresis()` | 查询 hysteresis 设置 |
| `dali_304_query_report_timer()` | 查询 report timer 值 |
| `dali_304_query_deadtime_timer()` | 查询 deadtime timer 值 |
| `DALI_304_RAW_TO_LUX_FP(raw)` | 将原始值转换为 Lux（浮点） |

```c
/* 从短地址5的实例0查询 occupancy 状态 */
bool occupied;
esp_err_t err = dali_303_query_occupancy(dali, DALI_ADDR_SHORT, 5, 0, &occupied, timeout);

/* 将原始光照值转换为 Lux */
uint8_t raw_value = 201;  /* 示例原始读数 */
float lux = DALI_304_RAW_TO_LUX_FP(raw_value);  /* ≈ 100000 Lux */
```

### send-twice 命令

```c
/* RESET 必须在 100 ms 内发送两次，设置 send_twice = true */
dali_master_transaction_config_t tx = {
    .addr_type = DALI_ADDR_SHORT, .addr = 0,
    .is_cmd = true, .command = DALI_CMD_RESET,
    .send_twice = true, .tx_timeout_ms = DALI_TX_TIMEOUT_MS,
};
dali_master_do_transaction(dali, &tx, NULL);
```

### IEC 62386-209 彩色+色溫控制

`dali_master_set_color()` 封装了 Part 209 规定的完整 DTR 预载 + 颜色设置 + ACTIVATE 时序，支持三种颜色模式：

| 模式 | 枚举值 | 说明 |
|------|--------|------|
| 色温 | `DALI_COLOR_CCT` | Mirek（153 ≈ 6500 K，370 ≈ 2700 K） |
| RGB | `DALI_COLOR_RGB` | R/G/B 三通道，各 0–254 |
| RGBWAF | `DALI_COLOR_RGBWAF` | R/G/B/W/A/F 六通道，0xFF = 通道不变 |

```c
/* 色温 */
dali_color_val_t val = { .cct = { .mirek = 370 } };
dali_master_set_color(dali, DALI_ADDR_SHORT, 4, DALI_COLOR_CCT, val, DALI_TX_TIMEOUT_MS);

/* RGB */
dali_color_val_t val = { .rgb = { .r = 254, .g = 0, .b = 0 } };
dali_master_set_color(dali, DALI_ADDR_SHORT, 4, DALI_COLOR_RGB, val, DALI_TX_TIMEOUT_MS);
```

**查询颜色能力**：使用 `dali_enable_device_type()` 解锁Part 209扩展命令后再发查询命令：

```c
dali_enable_device_type(dali, 8, DALI_TX_TIMEOUT_MS);

int caps = DALI_RESULT_NO_REPLY;
dali_master_transaction_config_t caps_cfg = {
    .addr_type = DALI_ADDR_SHORT, .addr = 4,
    .is_cmd = true, .command = DALI_209_QUERY_COLOR_CAPABILITIES,
    .send_twice = false, .tx_timeout_ms = DALI_TX_TIMEOUT_MS,
};
dali_master_do_transaction(dali, &caps_cfg, &caps);
/* caps: bit4=XY  bit5=Tc  bit6=Primary-N  bit7=RGBWAF */
```

> `dali_master_set_color()` 内部已自动调用 `dali_enable_device_type()`，无需手动发送。手动发查询命令时需自行调用。

### 释放资源

```c
dali_del_master(dali);
```

---

## DALI 帧时序参考

```
Te = 416.67 µs（半周期）

前向帧（FF）：起始位(1Te×2) + 16位数据(1Te×2 each) + 停止位(2Te×2) = 38 Te ≈ 15.8 ms
后向帧（BF）：起始位(1Te×2) + 8位数据(1Te×2 each) + 停止位(2Te×2) = 22 Te ≈  9.2 ms

无回复：   FF → 自动等待 > 22 Te → 下一帧
有回复：   FF → [7 Te … 22 Te] → BF → 自动等待 > 22 Te → 下一帧
send-twice：FF1 → 等待 < 100 ms → FF2
```

---

## 项目结构

```
components/dali/
├── CMakeLists.txt
├── idf_component.yml
├── CHANGELOG.md
├── LICENSE
├── README.md
├── README_cn.md                
├── src/
│   ├── dali_control_gear.c          # IEC 62386-102 控制装置（灯具）
│   ├── dali_control_device.c        # IEC 62386-103 控制设备（输入传感器）
│   ├── dali_color_control_dt8.c    # IEC 62386-209 颜色控制（DT8）
│   ├── dali_device_sensors.c        # 传感器相关功能
│   └── dali_system_components.c     # 系统组件支持
└── include/
    ├── dali.h                  # 核心公共 API
    ├── dali_command.h          # 命令码定义
    ├── dali_control_gear.h          # 控制装置 API
    ├── dali_control_device.h        # 控制设备 API
    ├── dali_color_control_dt8.h    # 颜色控制 API
    ├── dali_device_sensors.h        # 传感器 API
    └── dali_system_components.h     # 系统组件 API
```

---

## 许可证

Apache License 2.0，详见 [LICENSE](LICENSE)。
