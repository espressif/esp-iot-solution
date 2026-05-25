# DALI 总线控制器驱动（`esp_dali`）

[![Component Registry](https://components.espressif.com/components/espressif/esp_dali/badge.svg)](https://components.espressif.com/components/espressif/esp_dali)

基于 ESP-IDF 的 DALI（数字可寻址照明接口，IEC 62386）主站控制器驱动。该驱动使用片上 **RMT（远程控制收发器）** 外设生成和解码曼彻斯特编码的 DALI 物理层，无需外部定时逻辑。

---

## 功能特性

- **完整 IEC 62386 物理层** — 曼彻斯特编码，Te = 416.67 µs，±10% 容差窗口。
- **前向帧 TX** — 发送 16 位 DALI 前向帧（地址字节 + 命令/数据字节）。
- **后向帧 RX** — 接收并解码来自从设备的 8 位 DALI 后向帧。
- **全部寻址模式** — 短地址（0–63）、组地址（0–15）、广播及特殊命令寻址。
- **直接弧功率控制（DAPC）** — 直接设置灯具输出亮度（0x01–0xFE）。
- **间接控制命令** — OFF、UP、DOWN、STEP_UP、RECALL_MAX、GO_TO_SCENE 等。
- **配置命令** — RESET、STORE_DTR_AS_*、ADD/REMOVE 组、场景存储。
- **查询命令** — QUERY_STATUS、QUERY_ACTUAL_LEVEL、QUERY_VERSION 及所有 IEC 62386-102 查询命令。
- **特殊（调试）命令** — INITIALIZE、RANDOMIZE、COMPARE、PROGRAM_SHORT_ADDRESS 等。
- **send-twice 自动重发** — 对需要连续两次前向帧的命令自动在 100 ms 内重发。
- **自动帧间隔** — 每次事务结束后驱动自动插入 IEC 62386 要求的最小帧间隔（> 22 Te），无需用户手动延时。
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
dali_master_do_transaction(dali, DALI_ADDR_BROADCAST, 0, true, DALI_CMD_OFF,
                    false, DALI_TX_TIMEOUT_MS, NULL);
/* 帧间隔由驱动自动插入，无需手动延时 */
```

### 直接弧功率控制（DAPC）

```c
dali_master_do_transaction(dali, DALI_ADDR_SHORT, 0, false, 200,
                    false, DALI_TX_TIMEOUT_MS, NULL);
```

### 查询设备（有回复）

```c
int reply;
esp_err_t err = dali_master_do_transaction(dali, DALI_ADDR_SHORT, 0, true,
                                    DALI_CMD_QUERY_ACTUAL_LEVEL,
                                    false, DALI_TX_TIMEOUT_MS, &reply);
if (err == ESP_OK && DALI_RESULT_IS_VALID(reply)) {
    ESP_LOGI(TAG, "当前亮度: 0x%02X", (uint8_t)reply);
}
```

### send-twice 命令

```c
/* RESET 必须在 100 ms 内发送两次，设置 send_twice = true */
dali_master_do_transaction(dali, DALI_ADDR_SHORT, 0, true, DALI_CMD_RESET,
                    true, DALI_TX_TIMEOUT_MS, NULL);
```

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
├── README_cn.md            # 本文件
├── src/
│   └── dali.c              # 驱动实现
└── include/
    ├── dali.h              # 公共 API
    └── dali_command.h      # 命令码定义
```

---

## 许可证

Apache License 2.0，详见 [LICENSE](LICENSE)。
