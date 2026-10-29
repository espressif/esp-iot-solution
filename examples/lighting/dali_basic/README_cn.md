# DALI LED灯具控制示例应用

本示例演示了如何使用 `dali` 组件控制LED灯具进行亮度调节、色彩控制、状态查询及103和303传感器的设备的控制与使用。

## 主要功能

- 初始化 DALI 驱动（RX/TX GPIO；RX/TX INVERT）
- Commission：为 Part 102 控制设备和 Part 103 输入设备分配短地址
- 自动识别：扫描总线并按设备类型区分 DT6（调光）和 DT8（调色）灯具
- 直接弧功率控制（DAPC）: 对所有灯具执行 OFF -> 25% -> 50% -> 100%
- 间接命令示范：`RECALL_MAX_LEVEL`
- 查询命令示范：`QUERY_STATUS`、`QUERY_ACTUAL_LEVEL`、`QUERY_DEVICE_TYPE`、`QUERY_COLOR_CAPABILITIES`
- Part 103 传感器演示：检测到有人时触发灯具同时闪烁
  - DT6 灯具：同时亮灭闪烁
  - DT8 灯具：红蓝交替闪烁

## 硬件连接

1. ESP32-C6（或任意带 RMT 外设的 ESP32）

```                         Part 103 传感器
ESP32 RX ──┐                      │
           ├── TTL转DALI ──┬── DALI 总线 ── DALI驱动器 ── LED灯
ESP32 TX ──┘               │
                  DALI 总线电源 (16 V) 
```

2. 硬件注意事项：
   - DALI 电源模块：DALI总线高电平为16V，低电平为0V，因此必须使用专用DALI总线电源
   - DALI 驱动器：需要一个DALI驱动，实现灯具与DALI总线的通信
   - DALI 收发器：需要一个TTL转DALI模块，实现ESP32与DALI总线的通信，并将ESP32与DALI总线隔离
   - Part 103 传感器（可选）：本示例使用 DLS-203-P 传感器用于示例6演示。只需要将DLS-203-P挂载到DALI总线上，并且直接通过DALI总线取电，不需要单独电源供电。

## 配置步骤

1. 进入例程目录：

```bash
cd ./esp-iot-solution/examples/lighting/dali_basic
```

2. 修改 GPIO 引脚（根据实际硬件连接）：

```c
#define DALI_RX_GPIO    5
#define DALI_TX_GPIO    6
```

3. 配置信号极性（如果 TTL 转 DALI 模块内部有反相逻辑，需启用 `invert_tx/rx`）：

```c
dali_master_config_t cfg = {
    .rx_gpio   = DALI_RX_GPIO,
    .tx_gpio   = DALI_TX_GPIO,
    .invert_tx = true,
    .invert_rx = true,
};
```

4. 通过 menuconfig 启用所需的 DALI 功能（请根据dali设备选择所需要的协议，本示例使用 Part 102/103/209/303）：

```bash
idf.py menuconfig
# → Component config → DALI Component Configuration
#   启用：Part 102、Part 103、Part 209、Part 303/304
```

5. 运行 `idf.py set-target <your_target>`（例如 `esp32` / `esp32c6`）

6. 编译并烧写：

```bash
idf.py -p <PORT> build flash monitor
```

## 注意事项

- DALI 总线协议对定时要求严格，建议使用专用 DALI 转换芯片和驱动。
- 如果TTL转DALI模块内部是反相的，需要配置引脚极性。
- DALI 配置命令 `STORE_ACTUAL_LEVEL` 等需要发送两次的命令必须在 100ms 内发送两次，否则无效。

## 示例输出结果

程序启动后按顺序执行：

1. Commission Part 102 和 Part 103 设备
2. 扫描地址并按设备类型识别 DT6 / DT8
3. 对所有灯具执行 DAPC 调光序列
4. 在第一个发现的灯具上执行间接命令
5. 查询所有发现的灯具和传感器
6. 等待占用信号并触发同时闪烁

示例日志：

```log
I (30628) dali_example: --- Part 2: Scan & identify addresses ---
I (30730) dali_example:     addr  0 -> DT8 RGB lamp  (color_caps=0xE0 XY:N Tc:Y PrimaryN:Y RGBWAF:Y)
I (30862) dali_example:     addr  1 -> lamp (type=6, status=0x00)
I (30994) dali_example:     addr  2 -> lamp (type=6, status=0x02)
I (37483) dali_example:   Identified: 4 DT6 lamp(s), DT8_ADDR=0
...
I (76346) dali_example:   Occupied! Triggering simultaneous DT6 blink and DT8 Red/Blue flash.
I (76580) dali_example:   [1/6] DT6 ON + DT8 Red
I (77255) dali_example:   [1/6] DT6 OFF + DT8 Blue
...
```
