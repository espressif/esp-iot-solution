# ESP Display Present

[![组件服务](https://components.espressif.com/components/espressif/esp_display_present/badge.svg)](https://components.espressif.com/components/espressif/esp_display_present)

[English](README.md) | 中文

`esp_display_present` 用于连接渲染器生成的像素表面与 `esp_lcd` 面板，不依赖
场景、控件或输入语义。它把送屏路径（缓冲、TE、旋转、提交）从 GUI 里抽出来，
同一套 Presenter 可以给 LVGL、GSP 或其他自绘 GUI 用。

它不是 [`esp_lvgl_adapter`](../esp_lvgl_adapter/README_CN.md)。adapter 是完整的
LVGL 适配层（显示注册、任务、锁、输入等）；本组件不认识 LVGL。若 GUI 是
LVGL 9，用 [`esp_lv_present`](../esp_lv_present/README_CN.md) 把 flush 接到
Presenter 上。

```text
LVGL 9 / GSP / 其他 GUI
        │  像素 + 脏区
esp_display_present     ← 本组件
        │
esp_lcd 面板
```

## 添加到工程

```
idf.py add-dependency "espressif/esp_display_present"
```

或在 `idf_component.yml` 中：

```yml
dependencies:
  espressif/esp_display_present: "*"
```

## 职责

- 校验显示目标并选择合适的呈现策略。
- 租用渲染表面或有界绘制缓冲区。
- 提交整帧或脏区更新。
- 处理帧缓冲区所有权、TE 同步、旋转、字节序、缓存同步和可选硬件拷贝。
- 按顺序跟踪完成状态，并在关闭时避免提前释放传输中的缓冲区。

渲染器负责逻辑脏区和像素生成；Presenter 负责物理缓冲区一致性和面板提交。

## 公共 API

包含 `esp_display_present.h` 即可使用公共 API。配置和共享类型分别定义在
`esp_display_present_config.h` 和 `esp_display_present_types.h` 中。

主要对象为 `esp_display_presenter_t`。渲染器按以下顺序工作：

1. 使用 `esp_display_present_target_config_t` 创建 Presenter；
2. 获取帧契约；
3. 渲染请求的完整表面或分区；
4. 提交逻辑覆盖范围，或取消当前帧；
5. 等待生产端工作排空后停止并删除 Presenter。

绘制契约分为以下类型：

| 契约 | 渲染器职责 |
|---|---|
| `PARTITION` | 将请求区域渲染到 Presenter 管理的绘制缓冲区 |
| `DIRECT` | 将脏区直接渲染到一致的完整逻辑表面 |
| `FULL` | 重绘完整逻辑表面 |

呈现模式通常应保持为 `ESP_DISPLAY_PRESENT_MODE_AUTO`。在目标配置中准确提供
面板类型、帧缓冲区、TE、旋转和字节序信息，组件即可选择正确路径。

## 面板初始化

创建面板前，明确配置面板类型、模式、旋转及 TE，再查询所需的最少面板帧缓冲数量：

```c
uint8_t panel_fb_count;
ESP_ERROR_CHECK(esp_display_present_get_required_frame_buffer_count(
    &target, &panel_fb_count));
```

将结果用于 RGB/MIPI 驱动的 `num_fbs`，随后填入 `target.hw.panel` 和
`target.hw.io`，使用同一配置创建 Presenter。使用驱动自有缓冲时，保持
`target.fb.frame_buffer_count = 0`；该字段表示传入的有效缓冲地址数量，
不表示面板分配数量。IO/GRAM 返回 0，绘制缓冲与 TE compose 缓冲另行管理。
查询不访问句柄或分配内存，会拒绝不支持的静态策略组合；实际驱动和缓冲检查
仍在创建阶段完成。运行多个模式时，应按所有支持的测试组合取最大需求。

## 呈现模式

| 模式 | 目标与缓冲区 | 生产者契约 | 旋转与验证情况 |
|---|---|---|---|
| `NONE` | GRAM 面板不使用帧缓冲区；RGB/MIPI 使用一个帧缓冲区 | GRAM 为 `PARTITION`，RGB/MIPI 为 `DIRECT` | GRAM 仅支持 0°。已有 host 测试，当前 test app 不可选。 |
| `DOUBLE_FULL` | RGB/MIPI，两个帧缓冲区 | `FULL`，每帧完整重绘 | 通过逻辑 transform 表面实现旋转。已有 host 测试，当前 test app 不可选。 |
| `TRIPLE_FULL` | RGB/MIPI，三个帧缓冲区 | `FULL`，每帧完整重绘 | 所有旋转角度均已加入运行时 benchmark 矩阵。 |
| `DOUBLE_DIRECT` | RGB/MIPI，两个帧缓冲区 | `DIRECT`，0° 时支持脏区渲染 | 非 0° 旋转会被拒绝。 |
| `DOUBLE_PARTIAL` | RGB/MIPI，两个帧缓冲区及分区绘制缓冲区 | `PARTITION`，修复帧缓冲区中的过期区域 | 旋转融合在分区拷贝中，并已加入运行时 benchmark 矩阵。 |
| `TRIPLE_PARTIAL` | RGB/MIPI，三个帧缓冲区及分区绘制缓冲区 | `PARTITION`，修复帧缓冲区中的过期区域 | 所有旋转角度均已加入运行时 benchmark 矩阵；默认 P4 handoff 路径也会解析为该模式。 |
| `TE_SYNC` | SPI/I80/QSPI GRAM 面板；一个或两个整帧 Compose 缓冲区 | `PARTITION`，在 TE 时刻推送整帧 | 旋转融合在 Compose 缓冲区写入中，并会在兼容的 GRAM 目标上进入运行时矩阵。 |
| `AUTO` | 根据面板类型和 TE 配置解析 | 使用最终模式的契约 | GRAM 开启 TE 时解析为 `TE_SYNC`，否则为 `NONE`；RGB/MIPI 解析为 `TRIPLE_PARTIAL`。 |

以上验证状态仅描述本仓库已有覆盖，不代表所有面板驱动和开发板均已验证。

`DOUBLE_DIRECT` 仅定义为 0°。整帧旋转应使用 FULL 模式，旋转脏区渲染应使用
PARTIAL 模式。

## 分区绘制缓冲区

PARTIAL 和 `TE_SYNC` 使用有界绘制缓冲区（默认约 32 KiB）。有两个缓冲区时，
可选 worker 会拷贝上一条带（包括 RGB565 字节交换），同时生产者渲染下一条带。
`drawbuf.buffers = 0` 时，无 TE 的 GRAM 默认使用两个；FB repair 和 `TE_SYNC`
仅在内存允许时使用两个，单核芯片保持一个。设置 `drawbuf.buffers = 1` 可保持
单缓冲路径；worker 分配失败会回退到同步拷贝。

RGB 面板上，只有前一次帧缓冲区切换生效后才请求下一次切换，因为部分 RGB 驱动只在
DMA 切换缓冲区时报告完成。等待期间第三个帧缓冲区仍可继续渲染，因此快于刷新率
的生产者仍能达到刷新率。

## TE 模式下的 SPI/QSPI 配置

`ESP_DISPLAY_PRESENT_MODE_TE_SYNC` 可以让渲染与正在进行的整帧传输重叠，
但不能把同步阻塞的面板驱动自动变成异步驱动。SPI/QSPI 面板 IO 必须允许
`esp_lcd_panel_draw_bitmap()` 将完整一帧加入队列，并在传输完成前返回：

- 目标和驱动支持时，将 SPI 总线的 `max_transfer_sz` 设置为至少一帧物理数据
  的大小：`width * height * bytes_per_pixel`。
- 面板 IO 的 `trans_queue_depth` 至少要覆盖驱动传输一帧所需的事务数量。如果
  驱动还会加入命令事务，再额外预留一个队列项。驱动单个数据事务上限为
  `chunk_bytes` 时，数据队列项数量为 `ceil(frame_bytes / chunk_bytes)`。例如，
  480 × 480 RGB565 一帧为 460800 字节；每个数据块上限为 32 KiB 时需要 15 个
  数据事务，因此队列深度设为 16 可以再为命令预留一个队列项。
- SoC 和当前 ESP-IDF 面板 IO 驱动都支持时，启用 PSRAM 直接 DMA。部分目标通过
  `psram_dma_direct` 等驱动专用标志提供此能力；不要在未定义该字段的驱动上
  使用它。Compose 缓冲区会按 DMA 能力分配；如果不能从 PSRAM 直接 DMA，驱动
  可能通过内部 RAM 中转，或将传输拆成更多数据块。
- 确保面板初始化已开启 TE 输出（通常使用 `TEON` 命令），并在 `te_sync` 中
  配置真实的 TE GPIO、总线频率和数据线数量。

传输完成回调必须在队列中的整帧真正完成后触发。如果 `draw_bitmap()` 的耗时
接近总线传完整帧所需的时间，说明事务队列过浅或驱动本身是同步实现，此时
渲染与传输会退化为串行执行。增加 Compose 缓冲区数量无法解决该问题，应先
修正总线或驱动配置。

执行 Display Off 或销毁面板前，应先停止帧生产并调用
`esp_display_presenter_quiesce()`。在 quiesce 成功之前必须保持面板 IO 和完成
中断有效，之后才能关闭面板并删除 Presenter。

## 并发

Presenter 只允许一个帧生产者。其他任务可以请求停止，但帧获取、提交和取消
调用必须保持串行。传输中的硬件任务可能使删除操作暂时返回
`ESP_ERR_INVALID_STATE`；此时应保留对象，等待生产者停止后重试。停止前必须提交
或取消已提交的绘制工作；stop 不会接管生产者持有的帧。

`private_include/` 下的头文件和 `src/` 下的文件均为实现细节，不提供公共兼容性
承诺。内部帧/生命周期转移表及缓冲区所有权不变量见
[`STATE_MACHINE.md`](STATE_MACHINE.md)。

## 和 GUI 的关系

本组件只处理像素送屏，不绑定某一种 GUI：

| | `esp_display_present` | `esp_lvgl_adapter` |
|---|---|---|
| 职责 | 把渲染器像素送到 `esp_lcd` | 完整 LVGL 适配（显示、任务、锁、输入等） |
| GUI | 任意：LVGL、GSP、自绘 | 仅 LVGL 8/9 |
| LVGL | 不依赖。LVGL 9 请用 `esp_lv_present` | 自己完成 LVGL 刷新路径 |

LVGL 9 接入步骤见 [`esp_lv_present`](../esp_lv_present/README_CN.md)。

## 示例和 test app

- 示例：`examples/display/gui/lvgl_present_benchmark` — 在应用持有的 Presenter
  上跑 LVGL 9 benchmark。
- 示例：`examples/display/gui/gsp_lvgl_present_handoff` — GSP ↔ LVGL Presenter
  所有权切换（需要 LCD 和 `espressif/esp-gsp`）。
- Test app：`test_apps/` — Unity 检查 create/delete 以及 PARTITION 帧提交。
  使用软件 stub 面板，不需要 LCD，形式与 `esp_lv_decoder` / `esp_lv_fs` 相同。

```sh
cd components/display/tools/esp_display_present/test_apps
idf.py set-target esp32c3 build flash monitor
```
