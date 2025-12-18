## MIDI over BLE Profile Component

[![Component Registry](https://components.espressif.com/components/espressif/ble_midi/badge.svg)](https://components.espressif.com/components/espressif/ble_midi)

`ble_midi` 是一款用于 ESP32 系列芯片的 BLE‑MIDI（蓝牙低功耗 MIDI）Profile 组件，提供简洁的 C 语言 API，便于在 ESP 设备上快速实现基于 BLE 的 MIDI 输入/输出。

### 功能概览

- **BLE‑MIDI GATT 服务**
  - 服务 UUID：`03B80E5A‑EDE8‑4B33‑A751‑6CE34EC4C700`
  - 特征（MIDI I/O）UUID：`7772E5DB‑3868‑4112‑A1A9‑F2669D106BF3`
  - 特征属性：Write Without Response、Notify
- **MIDI 收发能力**
  - 发送/接收 BLE‑MIDI 事件包（BEP）
  - 13‑bit 时间戳（BEP 头高 6 bit + 每条消息低 7 bit）
  - 多条消息聚合发送（`esp_ble_midi_send_multi()`）
  - SysEx（0xF0…0xF7）自动分片与重组
  - 实时消息（0xF8…0xFF）穿插传输
- **高层 API**
  - 原始 BEP 接口：`esp_ble_midi_send()`、`esp_ble_midi_send_raw_midi()`、`esp_ble_midi_register_rx_cb()`
  - 解析后事件回调：`esp_ble_midi_register_event_cb()`（接收 Note On/Off、Control Change、Pitch Bend、SysEx 等）
- **Service 生命周期管理**
  - `esp_ble_midi_profile_init()` / `esp_ble_midi_profile_deinit()`：初始化/去初始化 MIDI Profile（初始化必须在其他 MIDI API 之前调用）
  - `esp_ble_midi_svc_init()` / `esp_ble_midi_svc_deinit()`：注册/注销 BLE‑MIDI GATT Service

### 将组件添加到工程

使用 Component Manager 将组件加入工程（CMake 配置阶段会自动拉取依赖）：

```bash
idf.py add-dependency "espressif/ble_midi=*"
```

### 示例

使用以下命令从示例模板创建工程：

```bash
idf.py create-project-from-example "espressif/ble_profiles=*:ble_midi"
```

也可直接参考本仓库示例：`examples/bluetooth/ble_profiles/ble_midi`。  
示例演示了：
- BLE‑MIDI GATT Service 的注册与发布（包含 128‑bit MIDI Service UUID 的扩展广播）
- 通过 Notify 收发 BLE‑MIDI 事件包
- 使用 `esp_ble_midi_send_multi()` 聚合多条 MIDI 消息
- 调用 `esp_ble_midi_svc_deinit()` 优雅卸载 Service

### 常见问题

- **如何让主机识别为 BLE‑MIDI 设备？**  
  示例工程的扩展广播已包含 BLE‑MIDI Service UUID（`0x03B80E5A‑EDE8‑4B33‑A751‑6CE34EC4C700`），iOS/macOS 等主机可在系统 MIDI 设备列表中发现并连接。

- **如何正确停止并释放资源？**  
  先调用 `esp_ble_midi_svc_deinit()` 注销 GATT Service，再调用 `esp_ble_conn_stop()` / `esp_ble_conn_deinit()` 结束连接与栈实例。
