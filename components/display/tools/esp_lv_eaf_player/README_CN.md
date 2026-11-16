[![Component Registry](https://components.espressif.com/components/espressif/esp_lv_eaf_player/badge.svg)](https://components.espressif.com/components/espressif/esp_lv_eaf_player)

# EAF/EAF4 动画播放器核心与 LVGL 适配层

**[English](README.md)**

`esp_lv_eaf_player` 是面向 ESP-IDF 的动画解码与播放组件，通过与 GUI 解耦的播放器接口（`esp_eaf_player.h`）统一支持传统 `EAF/AAF` 与新一代 `EAF4` 容器；同时提供可选的 LVGL v8 / v9 控件适配层（`lv_eaf.h`）。

## 架构说明

组件内部划分为四个独立层次：

1. `player.c` 负责管理流状态、帧缓冲、随机跳转（seek）和容器选择，对外提供无 LVGL 依赖的 `esp_eaf_player.h`。
2. `container/eaf` 与 `container/eaf4` 负责校验文件头与帧索引，并将帧/分块请求归一化，不包含像素重建代码。
3. `codec_registry.c` 统一调度 `0x00..0x06` 编码模式与 EAF4 帧子模式（含 `HSUB` 与 `TILE_SUB_PACKED`）。
4. `lv_eaf.c` 将解码帧封装为 LVGL 图像描述符，负责定时器推进、事件分发和脏矩形（dirty area）局部刷新。

非 LVGL 渲染框架可以直接消费 `esp_eaf_frame_t` 中的像素指针、可选 A8 透明平面、行跨度（stride）与脏矩形，无需链接 LVGL。

## 依赖要求

- ESP-IDF `>= 5.5`；
- 仅在使用 `lv_eaf.h` 时需要 `lvgl/lvgl`（8.x 或 9.x）；
- 大尺寸动画建议启用 PSRAM，小尺寸资源可自动回退至片内 SRAM；
- 默认启用软件 JPEG 解码；在 `ESP32-P4` 与 `ESP32-S31` 上会自动优先使用硬件 JPEG 解码器。

<!-- BEGIN GENERATED CODEC GUIDE -->
## 支持的编码模式与编码能力

“仅解码”表示组件可播放这类资源，但内置转换工具不生成这类文件。

| ID | 编码模式 | CLI `--codec` | 文件格式 | 像素格式 | 适用场景 |
|---|---|---|---|---|---|
| `0x00` | RLE | 仅解码 | EAF / AAF | RGB565, BGRA8888 | 既有资源、图标动画 |
| `0x01` | HUFFMAN_RLE | 仅解码 | EAF / AAF | RGB565, BGRA8888 | 既有资源、图标动画 |
| `0x02` | JPEG | 仅解码 | EAF / AAF | RGB565 | 兼容既有 JPEG 资源 |
| `0x03` | HUFFMAN | 仅解码 | EAF / AAF | RGB565, BGRA8888 | 既有资源、图标动画 |
| `0x04` | LOSSLESS | `lossless` | EAF4 | BGRA8888, RGB565, RGB565A8 | 精细 UI、阴影、半透明动画 |
| `0x05` | BOUNDED | `bounded` | EAF4 | BGRA8888, RGB565 | 通道误差有上限的 UI／卡通动画 |
| `0x06` | JPEG_DELTA | `jpeg-delta` | EAF | RGB565 | 实景、渐变、不透明动效 |

转换工具会在图片右侧和底部补像素，使 LOSSLESS／BOUNDED 的宽高为 4 的倍数，JPEG_DELTA 为 16 的倍数；不会缩放原图。

支持 PNG 序列目录、GIF、APNG、动画 WebP 和视频输入；完整参数与输入要求见[编码指南](docs/ENCODING.md)。

RGB565 的“无损”以量化后的 16-bit 像素为准；RGB565A8 额外精确保留 A8。`jpeg-delta` 不保存 Alpha。BOUNDED 的 BGRA8888 对包括 A8 在内的通道施加误差上限，不保证 Alpha 逐字节无损。

JPEG 解码优先使用硬件，不支持或失败时默认回退至 `esp_new_jpeg`；关闭软件 JPEG 后无此回退。

**EAF4 输出限制：**宽高均为 4 的倍数且不超过 1024；1..10000 帧；帧周期 10..10000 ms；文件不超过 32 MiB。

压缩对比、内存预算与复现方法见[评测说明](docs/MEASUREMENTS.md)。
<!-- END GENERATED CODEC GUIDE -->

## 添加到工程

```bash
idf.py add-dependency "espressif/esp_lv_eaf_player:*"
```

或在 `idf_component.yml` 中声明：

```yaml
dependencies:
  espressif/esp_lv_eaf_player: "*"
```

## 转换与打包工具

先运行 `python3 -m pip install -r requirements.txt`；视频转换需额外安装 FFmpeg。
需保留原始 BGRA 字节时，添加 `--no-clean-alpha` 并保持时域阈值为 0。
设置 `CONFIG_ESP_LV_EAF_ENABLE_LVGL_ADAPTER=n` 可在存在 LVGL 时只构建核心。
关闭软件 JPEG 只移除回退，不关闭支持芯片上的硬件 JPEG。


- **在线转换工具**：[https://esp32-gif.espressif.com/](https://esp32-gif.espressif.com/)（用于生成传统 `0x00..0x03` `.eaf` 文件）。
- **组件内置 CLI 工具（`tools/eaf_cli.py`）**：支持将 PNG 序列目录、GIF、APNG 或 MP4 编码为 `EAF4` 或 `EAF`，支持批量 64 字节对齐打包、生成 C 数组头文件及离线检查：

常用转换与打包命令：

```bash
# 无损 EAF4，默认清零全透明像素的 RGB，不影响 straight-alpha 显示
python3 tools/eaf_cli.py encode frames/ -o anim.eaf4 --codec lossless

# BTC4 / BTC8：同一 codec，最大通道误差分别为 4 / 8，块大小均为 4×4
python3 tools/eaf_cli.py encode frames/ -o btc4.eaf4 --codec bounded --max-error 4
python3 tools/eaf_cli.py encode frames/ -o btc8.eaf4 --codec bounded --max-error 8

# JPEG Delta（不透明 RGB565）
python3 tools/eaf_cli.py encode frames/ -o anim.eaf --format rgb565 --codec jpeg-delta

# 批量输出资源、64 字节对齐分区镜像、清单和索引头文件
python3 tools/eaf_cli.py batch animations/ -o build/eaf_assets --codec lossless
python3 tools/eaf_cli.py inspect anim.eaf4
```

| 参数 | 默认值 | 用途 |
|---|---|---|
| `--format` | `bgra8888` | 可选 `rgb565`、`rgb565a8`；支持范围见下表 |
| `--period` | `67` ms | EAF4 帧周期 |
| `--key-interval` | `30` 帧 | 关键帧／完整刷新间隔 |
| `--max-error` | `8` | Bounded 最大通道误差，范围 0..255 |
| `--no-clean-alpha` | 不启用 | 关闭默认透明清零，保留 A=0 时的原始 RGB 字节 |
| `--temporal-deadzone` | `0` | 时域近似阈值；仅 BGRA8888 Lossless，0 表示关闭 |
| `--resize WIDTHxHEIGHT` | 不缩放 | 编码前缩放 |

Lossless 自动选择无损子模式，无需指定策略版本。透明清零仅用于 EAF4 BGRA8888 Lossless／Bounded。
完整参数、C 数组与传统容器选项见[编码指南](docs/ENCODING.md#cli-options)，或运行 `encode --help`／`batch --help`。


## 测试

组件提供纯核心、LVGL 8／9 设备测试及 Host 测试。构建、烧录和验收步骤见
[测试应用说明](test_apps/README.md)，覆盖范围见[测试指南](docs/TESTING.md)。

## 许可证

Apache-2.0，详见 [LICENSE](LICENSE)。
