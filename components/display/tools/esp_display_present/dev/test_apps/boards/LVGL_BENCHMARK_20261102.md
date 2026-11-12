# LVGL 官方 Benchmark：V1 / V2 同板对比

本页为历史固件与配置的结果；当前提交的同条件三板对照见 [当前验证](../../VALIDATION.md)。

日期：2026-11-02。每块板同一测试固件依次运行 V1、V2 的 LVGL 9.5.0 `lv_demo_benchmark`，各 16 个有效场景。每个结果为一次运行，未进行统计重复，FPS 为 LVGL 官方场景平均的整数值。

| 板子 | 屏幕 / 模式 | V1 FPS | V2 FPS | V1 / V2 render ms | V1 / V2 flush ms | LVGL CPU V1 / V2 |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| P4 ACM0 | 1024×600 MIPI，双 FB PARTIAL | 42 | 22 | 25 / 42 | 8 / 21 | 100% / 100% |
| S3 USB0 | 480×480 RGB，双 FB PARTIAL | 15 | 14 | 100 / 94 | 14 / 23 | 100% / 100% |
| S31 ACM1 | 480×480 QSPI，GRAM PARTIAL，TE 关闭 | 37 | 33 | 39 / 42 | 7 / 11 | 100% / 100% |

共用设置：RGB565、0°、tile 16 行、2 块后端绘制 tile、LVGL 单软件绘制单元、刷新周期 15 ms、DPI 60、性能优化编译、FreeRTOS tick 1000 Hz。S3/P4 FB 的两版均启用异步 copy；S31 是普通 GRAM 路径，未覆盖 TE。CPU/PSRAM：P4 400/200 MHz，S3 240/80 MHz，S31 320/200 MHz。板与板之间的绝对值受屏幕分辨率、处理器和面板差异影响。

接入路径并不完全相同：V1 `esp_lv_present` 借用 presenter 绘制缓冲；V2 `pv2_lvgl_create` 使用独立 LVGL 缓冲再复制。这些数字反映当前各自实际 LVGL 接入，而非仅比较后端传输效率。LVGL CPU 指标所有场景均为 100%，不据此判断各核利用率或定位瓶颈。

S31 测试固件使用临时 2 MB factory 分区以容纳 benchmark；三块板运行完成后停留在测试固件。原始日志、固件、sdkconfig 和 SHA256/板子 MAC 位于忽略目录 `artifacts/lvgl-standard-20261102/summary.json`。三板同仓库源码版本 `902ef970cad0134ff2b2c74fb3a8275869321c8a`；S31 使用编译期 S31 分支，S3/P4 最终固件的 FB V2 明确启用异步 copy。

以下为每个场景的 FPS，左为 V1，右为 V2：

| 场景 | P4 | S3 | S31 |
| --- | --- | --- | --- |
| Empty screen | 57 / 17 | 16 / 12 | 23 / 17 |
| Moving wallpaper | 40 / 12 | 6 / 6 | 15 / 12 |
| Single rectangle | 60 / 60 | 55 / 57 | 66 / 66 |
| Multiple rectangles | 60 / 27 | 20 / 17 | 46 / 31 |
| Multiple RGB images | 60 / 29 | 19 / 15 | 64 / 58 |
| Multiple ARGB images | 59 / 23 | 16 / 14 | 62 / 52 |
| Rotated ARGB images | 36 / 27 | 25 / 27 | 65 / 64 |
| Multiple labels | 57 / 26 | 17 / 16 | 37 / 28 |
| Screen sized text | 7 / 4 | 2 / 2 | 6 / 5 |
| Multiple arcs | 30 / 12 | 27 / 27 | 65 / 63 |
| Containers | 60 / 40 | 10 / 10 | 50 / 45 |
| Containers with overlay | 20 / 11 | 7 / 7 | 14 / 13 |
| Containers with opa | 59 / 34 | 8 / 9 | 47 / 43 |
| Containers with opa_layer | 39 / 13 | 7 / 7 | 33 / 27 |
| Containers with scrolling | 20 / 8 | 4 / 4 | 9 / 8 |
| Widgets demo | 20 / 18 | 2 / 2 | 4 / 4 |


## PARTIAL 使用 presenter 共享 tile 的复测

同一天、同一模式和 LVGL 工作负载；V2 改用 `pv2_lvgl_create_pooled()`，V1 基线和共享池 V2 在同一固件依次运行。上一轮 V2 是独立 LVGL 缓冲复制版；每组均为单轮，不视为统计稳定值。

| 板子 | V1 FPS | V2 原复制版 FPS | V2 共享 tile FPS | V2 原版 / 共享版 flush ms |
| --- | ---: | ---: | ---: | ---: |
| P4 | 42 | 22 | 25 | 21 / 11 |
| S3 | 15 | 14 | 14 | 23 / 16 |
| S31 | 37 | 33 | 34 | 11 / 10 |

正常绘制不再执行 LVGL→tile 的 memcpy；格式转换、GRAM 传输和 FB 合成仍存在。共享版仍预留安全备用绘制缓冲（LVGL 9.5 不会因 render-start 准入失败而中止本轮绘制），因此不宣称减少了该内存。日志与固件哈希见 `artifacts/lvgl-pooled-20261102/`；本轮设备固件构建后又补了旋转容量边界和参数检查，板测实际固件以保存的二进制为准。真实 GSP 任务未上板联测，Host 用独立任务 rebind 验证了同池交接。


## P4：DMA2D + 后台推进 + 延后取 tile

在 ACM0 / MAC `e8:f6:0a:e7:a7:8d` 上重新执行同一 LVGL 9.5.0 官方 16 场景。仍是 1024×600、RGB565、0°、DOUBLE_PARTIAL、16 行；V2 显式注入 DMA2D executor（仅准入不支持才允许 CPU 回退）、开启 background service，并将下一 tile 的获取放到单缓冲 `LV_EVENT_FLUSH_WAIT_START`。保持 fallback 绘制缓冲。

| 指标 | 本轮 V1 | 前轮 V2 共享池 | 本轮 V2 三项组合 |
| --- | ---: | ---: | ---: |
| LVGL 场景平均 FPS | 42 | 25 | 41 |
| 平均 render ms | 25 | 41 | 32 |
| 平均 flush ms | 8 | 11 | 1 |

通过链接包装实测 V1 `esp_display_present_dma2d_copy_sync`：45547 次调用、45547 次成功。V2 dispatcher：43635 次硬件执行、0 CPU 回退、0 执行错误，证明本轮实际使用硬件而非仅启用配置。

这三个改动一起测量，不能分配每项独立收益，也不证明此前差距全部由它们造成。当前 render 仍为 32 vs 25 ms；整体 FPS 已接近，后续要定位这一差值需独立分段计时。V2 等待回调只切换数据地址，不重置 LVGL 已 reshape 的 band header；Host 回归覆盖异步借用、真实像素、准入失败、旋转、跨任务交接及 flush 回调内不提前取下一块，普通和 ASan/UBSan/LSan 均 29/29。

原始日志、固件 SHA256、benchmark 源码和配置见 `artifacts/lvgl-tuned-p4-20261102/`。本轮仅重刷 P4；S3/S31 未重测此组合。DMA2D 和 background_service 的启用属于显式应用配置，框架不自动推断并启用硬件执行器。
