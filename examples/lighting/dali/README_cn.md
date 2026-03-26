# DALI LED灯具控制示例应用

本示例演示了如何使用 `dali` 组件控制LED灯具进行亮度调节、查询状态等相关命令。

## 主要功能

- 初始化 DALI 驱动（RX/TX GPIO）
- 直接弧功率控制（DAPC）: OFF -> 25% -> 50% -> 100%
- 间接命令示范：`RECALL_MAX_LEVEL`
- 查询命令示范：`QUERY_STATUS`, `QUERY_ACTUAL_LEVEL`, `QUERY_MAX_LEVEL`, `QUERY_MIN_LEVEL`, `QUERY_DEVICE_TYPE`
- 配置命令示范（send-twice）：`STORE_ACTUAL_LEVEL`

## 硬件连接

1. ESP32-C3（或任意带 RMT 外设的 ESP32）

```
ESP32 RX ──┐
           ├── TTL转DALI ── DALI 总线 ── DALI驱动器 ── LED灯
ESP32 TX ──┘

```

2. 硬件注意事项：
   - DALI 电源模块：DALI总线高电平为16V，低电平为0V，因此必须使用专用DALI总线电源
   - DALI 驱动器：需要一个DALI驱动，实现灯具与DALI总线的通信
   - DALI 收发器：需要一个TTL转DALI模块，实现ESP32与DALI总线的通信，并将ESP32与DALI总线隔离


## 配置步骤

1. 进入例程目录：

```bash
cd ./esp-iot-solution/examples/lighting/dali
```

2. 运行 `idf.py menuconfig`，进入 "DALI Driver" 选项进行配置：
   - DALI_RX_GPIO
   - DALI_TX_GPIO
   - DALI_TX_INVERT
   - DALI_RX_INVERT

3. 运行 `idf.py set-target <your_target>`（例如 `esp32` / `esp32c6`）
4. 编译并烧写：

```bash
idf.py -p <PORT> build flash monitor
```
## 注意事项

- DALI 总线协议对定时要求严格，建议使用专用 DALI 转换芯片和驱动。
- 如果TTL转DALI模块内部是反相的，在 menuconfig 里开启 `DALI_TX_INVERT` 或 `DALI_RX_INVERT`。
- DALI 配置命令 `STORE_ACTUAL_LEVEL` 等需要发送两次的命令必须在 100ms 内发送两次，否则无效。

## 示例输出结果

程序启动后会循环执行：

- 输出 DAPC 级别日志
- 发送间接命令并打印日志
- 查询设备结果并打印（若未返回会显示 NO REPLY）
- 发送 `STORE_ACTUAL_LEVEL` 以演示 配置命令

以下为示例测试 log:

```log
I (304) dali: TX polarity: inverting (DALI_TX_INVERT=y)
I (305) dali: DALI driver initialised (RX GPIO 5, TX GPIO 6 inv)
I (307) dali_example: === Round 1 ===
I (310) dali_example: [DAPC] OFF
I (1399) dali_example: [DAPC] 25%  (0x3F)
I (2484) dali_example: [DAPC] 50%  (0x7F)
I (3569) dali_example: [DAPC] MAX  (0xFE)
I (4654) dali_example: [CMD]  RECALL_MAX_LEVEL
I (4970) dali_example: [QUERY] STATUS              = 0x24  (36)
I (5222) dali_example: [QUERY] ACTUAL_LEVEL        = 0xFE  (254)
I (5473) dali_example: [QUERY] MAX_LEVEL           = 0xFE  (254)
I (5724) dali_example: [QUERY] MIN_LEVEL           = 0x01  (1)
I (5975) dali_example: [QUERY] DEVICE_TYPE         = 0x06  (6)
I (6195) dali_example: [CMD]  STORE_ACTUAL_LEVEL (send-twice)
```

