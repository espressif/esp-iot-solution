| 支持的芯片 | ESP32-H4 | ESP32-C6 | ESP32-S31 |
| ---------- | -------- | -------- | --------- |

# BLE ESL Tag（Profile 示例）

**ESP32-C6** 与 **ESP32-H4** 的默认编译为 **模式 2**：SSD1680 电子纸、LED 后端关闭、关联信息写入 NVS。ESP32-S31 保持 mock 显示后端（没有电子纸默认引脚）。请与 [`ble_esl_ap`](../ble_esl_ap/README_CN.md) 以及现有 Android App 配对。

协议栈：iot-solution `esp_ble_eslp_*` + `ble_conn_mgr`。

## 环境要求

- ESP-IDF **v6.1 及以上**
- **ESP32-H4** 或 **ESP32-C6**（SSD1680 默认引脚）；S31 可编译但无电子纸默认脚
- 外接 SSD1680（128×296，4736 字节 1bpp 帧）

## 使用方法

```bash
cd examples/bluetooth/ble_profiles/ble_esl/ble_esl_tag
idf.py set-target esp32c6    # 或: idf.py --preview set-target esp32h4
idf.py erase-flash
idf.py build flash monitor
```

`sdkconfig.defaults` 选择广播名 `ESP_ESL_TAG`、4 个图像槽、`MAX_IMAGE_SIZE=4736`、4MB Flash、NimBLE 绑定持久化，以及 `partitions.csv`（`images`，data 子类型 `0x40`，`0x210000` 处 64 KB）。`sdkconfig.defaults.esp32c6` 与 `sdkconfig.defaults.esp32h4` 选择 SSD1680 后端并关闭 LED。ESP32-S31 仍使用 mock 显示，但图像槽、对象大小和 `images` 分区相同，因此图像存储初始化不会中止。分区表变更后需要先执行 `idf.py erase-flash` 再烧录。GAP 连接重试保持芯片默认（ESP32-C6/H4 上为开启）。断开连接后的重新广播在 `ble_conn_mgr` 中。

## 电子纸默认引脚

| 项目 | ESP32-H4 | ESP32-C6 |
|------|----------|----------|
| 电源 GPIO | 37（高有效） | 8（低有效） |
| DC / RST / CS / BUSY | 1 / 16 / 0 / 17 | 20 / 19 / 21 / 22 |
| MOSI / SCLK | 38 / 39 | 18 / 15 |
| 帧大小 | 128 × 296 / 8 = **4736** 字节，线性 1bpp | 同左 |
