# ESP-BLE-UART Vibe Indicator 示例

[English](README.md)

| Supported Targets | ESP32 | ESP32-C2 | ESP32-C3 | ESP32-C5 | ESP32-C6 | ESP32-C61 | ESP32-H2 | ESP32-H21 | ESP32-H4 | ESP32-S3 | ESP32-S31 |
| ----------------- | ----- | -------- | -------- | -------- | -------- | --------- | -------- | --------- | -------- | -------- | --------- |

`ble_uart_vibe_indicator` 是面向 **vibe coding** 场景的 ESP-IDF 设备端示例：开发板通过 **ESP-BLE-UART** 暴露一组或多组红 / 黄 / 绿信号灯，上位机用 **JSONL**（Signal Light Control Protocol v1）发送 `query` / `control` 等命令，固件解析后驱动 GPIO，实现远程点灯、灭灯和快慢闪烁。实际联调不能只烧录固件，还需要在 PC 上运行 [**ESP-BLE-UART Bridge**]($IDF_PATH/tools/ble/ble_uart_bridge) 的 **`daemon`**（先 `connection-check`，再 `daemon <DEVICE_ID>`）：由 daemon 维持 BLE 连接、订阅通知，把 JSON 命令经 GATT 转发给设备并把响应回传给脚本或自动化工具。在 vibe coding 工作流中，这可以把写代码或跑任务的进度与状态映射成可见的灯语反馈——设备端负责执行灯控，bridge daemon 负责 BLE 通道与主机侧集成。

## 功能特性

- 复用 ESP-IDF 共享组件 [ble_uart]($IDF_PATH/examples/bluetooth/common/ble_uart) （默认 NimBLE）
- `op: "command"`，子命令为 `query` / `control`
- 通过 `data.payload[]` 批量下发控制
- Kconfig 可配置：组数（默认 3）、各组 GPIO 引脚、慢闪/快闪周期、设备名前缀

## 硬件接线

在 menuconfig 中配置 GPIO 引脚：

```
idf.py menuconfig → ESP-BLE-UART Vibe Indicator Example
```

为各组设置 `Channel N red/yellow/green GPIO`。默认 `-1` 表示未连接（协议仍可用，灯珠不会动作）。

**若需要看到灯珠输出，必须先为所用开发板配置 GPIO。** 本示例不提供板级默认引脚。

## 编译与烧录

主要 bring-up 目标为 **ESP32-H4**；**ESP32-H21** 同样支持。

```bash
cd esp-iot-solution/examples/bluetooth/ble_uart_vibe_indicator
idf.py --preview set-target esp32h4    # 或 esp32h21
idf.py --preview build flash monitor
```

当前 IDF 版本中 ESP32-H4 与 ESP32-H21 为 preview 目标，`idf.py` 子命令需保留 `--preview` 参数。

设备广播名为 `Vibe-Indicator-XXXX`（`XXXX` 为蓝牙 MAC 地址末两字节）。

BLE 以 `encrypted = false` 运行，便于调试。

## 协议

线格式、错误码与更多示例见 [json_format.md](json_format.md)。

典型主机流程：

1. 建立 BLE 连接（NUS / ESP-BLE-UART GATT）
2. 查询 `indicator_count`
3. 发送 `control`，`payload` 中可含一条或多条灯控命令

每条 JSONL **必须以 `\n` 结尾**。

## 使用 ESP-BLE-UART Bridge 进行主机测试

首次使用需安装 Bridge 依赖：

```bash
cd $IDF_PATH
. ./export.sh
python -m pip install -r tools/ble/ble_uart_bridge/requirements.txt
```

将 `<DEVICE_ID>` 替换为 `list-devices` 或串口 monitor 日志中的 BLE 地址。

```bash
cd $IDF_PATH/tools/ble/ble_uart_bridge

python main.py connection-check <DEVICE_ID>
python main.py daemon <DEVICE_ID>

# 查询组数
python main.py daemon-send --op command \
  --json '{"cmd":"query","type":"indicator_count"}' --timeout 5

# 控制单路灯
python main.py daemon-send --op command \
  --json '{"cmd":"control","payload":[{"indicator_id":0,"light_id":0,"light_action":2}]}' \
  --timeout 5

# 批量控制
python main.py daemon-send --op command \
  --json '{"cmd":"control","payload":[{"indicator_id":0,"light_id":0,"light_action":1},{"indicator_id":0,"light_id":1,"light_action":0}]}' \
  --timeout 5
```

### 预期结果

| 步骤 | 预期 |
|------|------|
| 查询 `indicator_count` | `ok: true`，`data.count` 与 Kconfig 一致 |
| `control` 单条 | `ok: true`，`data.payload` 回显请求 |
| `control` 批量 | 全部生效；回显 `payload` 与请求一致 |
| 非法 `light_id` | `ok: false`，`error: invalid_parameter`，`data.message` 有说明 |
| `id` 为空 | `ok: false`，`error: id_not_specified` |

## 工程结构

| 文件 | 说明 |
|------|------|
| `main/app_main.c` | NVS、indicator 与 JSONL 初始化、BLE UART 安装与打开 |
| `main/ble_jsonl.c` | JSONL 信封、`query` / `control` 分发 |
| `main/indicator.c` | 单灯 GPIO 驱动、慢闪/快闪定时器 |
| `main/Kconfig.projbuild` | 组数、GPIO 映射、闪烁周期、设备名前缀 |
| `json_format.md` | 协议参考 |

## 相关示例

- [ble_uart_service]($IDF_PATH/examples/bluetooth/ble_uart_service) — 最小回显服务与 Console 冒烟测试
- [ble_uart]($IDF_PATH/examples/bluetooth/common/ble_uart) — ESP-IDF 共享传输组件
- [ESP-BLE-UART Bridge]($IDF_PATH/tools/ble/ble_uart_bridge) — 提供上位机命令行工具，通过 BLE-UART 通道与设备通信
