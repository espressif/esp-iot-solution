# LVGL V1/V2 全部 Benchmark 结果 · 2026-11-02

本页为历史固件与配置的结果；当前提交的同条件三板对照和逐场景数据见 [当前验证](../../VALIDATION.md)。

共 7 组同板 V1/V2 对照，14 次完整运行，每次 16 个有效官方场景。全部来自原始日志；各组仅一次运行，未做重复统计。最新三项组合只在 P4 测试，S3/S31 最新为共享 tile 版本。

| 板子 | 阶段 | 版本 | 分辨率 | 平均 FPS | render ms | flush ms | LVGL CPU % | 场景数 |
| --- | --- | --- | --- | ---: | ---: | ---: | ---: | ---: |
| P4 | 原复制版 | V1 | 1024x600 | 42 | 25 | 8 | 100 | 16 |
| P4 | 原复制版 | V2 | 1024x600 | 22 | 42 | 21 | 100 | 16 |
| S3 | 原复制版 | V1 | 480x480 | 15 | 100 | 14 | 100 | 16 |
| S3 | 原复制版 | V2 | 480x480 | 14 | 94 | 23 | 100 | 16 |
| S31 | 原复制版 | V1 | 480x480 | 37 | 39 | 7 | 100 | 16 |
| S31 | 原复制版 | V2 | 480x480 | 33 | 42 | 11 | 100 | 16 |
| P4 | 共享 tile | V1 | 1024x600 | 42 | 25 | 8 | 100 | 16 |
| P4 | 共享 tile | V2 | 1024x600 | 25 | 41 | 11 | 100 | 16 |
| S3 | 共享 tile | V1 | 480x480 | 15 | 99 | 14 | 100 | 16 |
| S3 | 共享 tile | V2 | 480x480 | 14 | 99 | 16 | 100 | 16 |
| S31 | 共享 tile | V1 | 480x480 | 37 | 38 | 7 | 100 | 16 |
| S31 | 共享 tile | V2 | 480x480 | 34 | 41 | 10 | 100 | 16 |
| P4 | DMA2D＋后台 service＋延后取 tile | V1 | 1024x600 | 42 | 25 | 8 | 100 | 16 |
| P4 | DMA2D＋后台 service＋延后取 tile | V2 | 1024x600 | 41 | 32 | 1 | 100 | 16 |

配置：LVGL 9.5.0 `lv_demo_benchmark`，RGB565、0°、16 行、2 个后端 tile、单软件绘制单元、刷新周期 15 ms、DPI 60、性能优化编译、FreeRTOS 1000 Hz。P4：ACM0 / 1024×600 MIPI / CPU 400 MHz、PSRAM 200 MHz；S3：USB0 / 480×480 RGB / CPU 240 MHz、PSRAM 80 MHz；S31：ACM1 / 480×480 QSPI、无 TE / CPU 320 MHz、PSRAM 200 MHz。S3/P4 是双 FB PARTIAL。

原复制版使用独立 LVGL 缓冲；共享 tile 版去掉正常路径 LVGL→tile 的复制，仍保留异常备用缓冲。P4 最后一组另外显式注入 DMA2D、启用后台 service、将取下一 tile 延后到绘制前等待点。三项一起改，不能由本组数据分配各项独立收益。原阶段 V2 没配置硬件 executor。

硬件计数：P4 最后一组 V1 DMA2D 45547 次调用全部成功；V2 硬件执行 43635 次、CPU 回退 0、错误 0。新绑定 Host/LVGL、ASan/UBSan/LSan 均 29/29 通过，真实 GSP→LVGL 交接仍只做了 Host 独立任务模拟。

数值说明：FPS 为日志输出的整数场景均值，render/flush 为 LVGL 统计整数 ms；0 ms 不表示零耗时。CPU 所有场景都报告 100%，不能当成每核 CPU 占用；render 与 flush 不应直接相加后换算 FPS。旋转、FULL/DIRECT、TE 的官方 benchmark 不在本组测量范围。

## P4 · 原复制版

| 场景 | V1 FPS | V2 FPS | V1 render ms | V2 render ms | V1 flush ms | V2 flush ms |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Empty screen | 57 | 17 | 6 | 21 | 8 | 32 |
| Moving wallpaper | 40 | 12 | 15 | 48 | 8 | 31 |
| Single rectangle | 60 | 60 | 1 | 2 | 15 | 8 |
| Multiple rectangles | 60 | 27 | 4 | 11 | 11 | 20 |
| Multiple RGB images | 60 | 29 | 1 | 10 | 12 | 19 |
| Multiple ARGB images | 59 | 23 | 6 | 21 | 7 | 16 |
| Rotated ARGB images | 36 | 27 | 15 | 22 | 10 | 10 |
| Multiple labels | 57 | 26 | 10 | 19 | 3 | 13 |
| Screen sized text | 7 | 4 | 126 | 154 | 9 | 34 |
| Multiple arcs | 30 | 12 | 25 | 44 | 3 | 33 |
| Containers | 60 | 40 | 4 | 11 | 11 | 10 |
| Containers with overlay | 20 | 11 | 35 | 47 | 11 | 28 |
| Containers with opa | 59 | 34 | 5 | 15 | 10 | 12 |
| Containers with opa_layer | 39 | 13 | 18 | 54 | 6 | 15 |
| Containers with scrolling | 20 | 8 | 42 | 80 | 5 | 32 |
| Widgets demo | 20 | 18 | 93 | 113 | 7 | 25 |

原始日志：`artifacts/lvgl-standard-20261102/p4-run.log`。

## S3 · 原复制版

| 场景 | V1 FPS | V2 FPS | V1 render ms | V2 render ms | V1 flush ms | V2 flush ms |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Empty screen | 16 | 12 | 64 | 48 | 15 | 39 |
| Moving wallpaper | 6 | 6 | 125 | 115 | 19 | 36 |
| Single rectangle | 55 | 57 | 6 | 5 | 9 | 9 |
| Multiple rectangles | 20 | 17 | 38 | 32 | 14 | 28 |
| Multiple RGB images | 19 | 15 | 37 | 34 | 11 | 19 |
| Multiple ARGB images | 16 | 14 | 45 | 41 | 10 | 21 |
| Rotated ARGB images | 25 | 27 | 27 | 25 | 7 | 6 |
| Multiple labels | 17 | 16 | 44 | 40 | 19 | 18 |
| Screen sized text | 2 | 2 | 293 | 276 | 18 | 36 |
| Multiple arcs | 27 | 27 | 14 | 13 | 18 | 16 |
| Containers | 10 | 10 | 76 | 71 | 11 | 16 |
| Containers with overlay | 7 | 7 | 104 | 101 | 16 | 25 |
| Containers with opa | 8 | 9 | 84 | 78 | 18 | 16 |
| Containers with opa_layer | 7 | 7 | 104 | 101 | 12 | 16 |
| Containers with scrolling | 4 | 4 | 195 | 181 | 18 | 38 |
| Widgets demo | 2 | 2 | 357 | 343 | 18 | 33 |

原始日志：`artifacts/lvgl-standard-20261102/s3-run.log`。

## S31 · 原复制版

| 场景 | V1 FPS | V2 FPS | V1 render ms | V2 render ms | V1 flush ms | V2 flush ms |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Empty screen | 23 | 17 | 22 | 25 | 12 | 22 |
| Moving wallpaper | 15 | 12 | 46 | 53 | 15 | 26 |
| Single rectangle | 66 | 66 | 3 | 2 | 1 | 2 |
| Multiple rectangles | 46 | 31 | 12 | 18 | 6 | 10 |
| Multiple RGB images | 64 | 58 | 5 | 7 | 3 | 4 |
| Multiple ARGB images | 62 | 52 | 7 | 9 | 2 | 4 |
| Rotated ARGB images | 65 | 64 | 9 | 9 | 2 | 3 |
| Multiple labels | 37 | 28 | 17 | 21 | 5 | 9 |
| Screen sized text | 6 | 5 | 138 | 144 | 15 | 25 |
| Multiple arcs | 65 | 63 | 4 | 4 | 0 | 2 |
| Containers | 50 | 45 | 17 | 17 | 3 | 5 |
| Containers with overlay | 14 | 13 | 54 | 55 | 13 | 17 |
| Containers with opa | 47 | 43 | 18 | 18 | 3 | 5 |
| Containers with opa_layer | 33 | 27 | 27 | 32 | 3 | 6 |
| Containers with scrolling | 9 | 8 | 81 | 87 | 15 | 25 |
| Widgets demo | 4 | 4 | 167 | 172 | 14 | 24 |

原始日志：`artifacts/lvgl-standard-20261102/s31-run.log`。

## P4 · 共享 tile

| 场景 | V1 FPS | V2 FPS | V1 render ms | V2 render ms | V1 flush ms | V2 flush ms |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Empty screen | 57 | 26 | 7 | 11 | 8 | 19 |
| Moving wallpaper | 36 | 15 | 15 | 41 | 11 | 21 |
| Single rectangle | 60 | 61 | 1 | 9 | 15 | 1 |
| Multiple rectangles | 60 | 29 | 4 | 16 | 11 | 14 |
| Multiple RGB images | 60 | 30 | 1 | 23 | 12 | 5 |
| Multiple ARGB images | 60 | 28 | 6 | 25 | 7 | 5 |
| Rotated ARGB images | 33 | 29 | 16 | 27 | 11 | 3 |
| Multiple labels | 58 | 27 | 10 | 22 | 3 | 9 |
| Screen sized text | 7 | 5 | 126 | 139 | 9 | 23 |
| Multiple arcs | 30 | 15 | 25 | 40 | 4 | 20 |
| Containers | 60 | 45 | 4 | 16 | 11 | 3 |
| Containers with overlay | 20 | 13 | 35 | 50 | 11 | 15 |
| Containers with opa | 59 | 42 | 5 | 17 | 10 | 2 |
| Containers with opa_layer | 39 | 15 | 18 | 49 | 6 | 6 |
| Containers with scrolling | 20 | 10 | 42 | 71 | 5 | 22 |
| Widgets demo | 20 | 19 | 92 | 104 | 7 | 16 |

原始日志：`artifacts/lvgl-pooled-20261102/p4-run.log`。

## S3 · 共享 tile

| 场景 | V1 FPS | V2 FPS | V1 render ms | V2 render ms | V1 flush ms | V2 flush ms |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Empty screen | 16 | 12 | 64 | 55 | 16 | 30 |
| Moving wallpaper | 6 | 6 | 122 | 119 | 22 | 32 |
| Single rectangle | 55 | 57 | 6 | 11 | 9 | 3 |
| Multiple rectangles | 20 | 18 | 37 | 39 | 13 | 20 |
| Multiple RGB images | 18 | 17 | 37 | 42 | 12 | 10 |
| Multiple ARGB images | 17 | 13 | 45 | 56 | 8 | 9 |
| Rotated ARGB images | 25 | 25 | 28 | 30 | 7 | 4 |
| Multiple labels | 17 | 16 | 38 | 46 | 20 | 11 |
| Screen sized text | 2 | 2 | 295 | 279 | 19 | 34 |
| Multiple arcs | 27 | 27 | 14 | 26 | 18 | 3 |
| Containers | 10 | 9 | 74 | 78 | 11 | 11 |
| Containers with overlay | 7 | 7 | 102 | 101 | 18 | 14 |
| Containers with opa | 9 | 9 | 82 | 86 | 16 | 11 |
| Containers with opa_layer | 7 | 7 | 104 | 107 | 13 | 12 |
| Containers with scrolling | 4 | 4 | 193 | 181 | 18 | 33 |
| Widgets demo | 2 | 2 | 350 | 340 | 18 | 30 |

原始日志：`artifacts/lvgl-pooled-20261102/s3-run.log`。

## S31 · 共享 tile

| 场景 | V1 FPS | V2 FPS | V1 render ms | V2 render ms | V1 flush ms | V2 flush ms |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Empty screen | 24 | 18 | 19 | 24 | 14 | 20 |
| Moving wallpaper | 14 | 12 | 50 | 53 | 16 | 24 |
| Single rectangle | 65 | 66 | 3 | 2 | 1 | 2 |
| Multiple rectangles | 46 | 34 | 12 | 13 | 6 | 12 |
| Multiple RGB images | 63 | 60 | 5 | 7 | 3 | 3 |
| Multiple ARGB images | 61 | 55 | 8 | 10 | 2 | 2 |
| Rotated ARGB images | 64 | 65 | 10 | 10 | 2 | 1 |
| Multiple labels | 38 | 29 | 16 | 20 | 6 | 9 |
| Screen sized text | 6 | 5 | 139 | 145 | 14 | 23 |
| Multiple arcs | 65 | 64 | 3 | 4 | 1 | 2 |
| Containers | 52 | 48 | 15 | 16 | 4 | 4 |
| Containers with overlay | 14 | 13 | 54 | 55 | 11 | 16 |
| Containers with opa | 49 | 44 | 16 | 18 | 4 | 4 |
| Containers with opa_layer | 32 | 28 | 28 | 31 | 5 | 6 |
| Containers with scrolling | 9 | 8 | 78 | 84 | 18 | 23 |
| Widgets demo | 4 | 4 | 167 | 168 | 14 | 22 |

原始日志：`artifacts/lvgl-pooled-20261102/s31-run.log`。

## P4 · DMA2D＋后台 service＋延后取 tile

| 场景 | V1 FPS | V2 FPS | V1 render ms | V2 render ms | V1 flush ms | V2 flush ms |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Empty screen | 57 | 53 | 6 | 9 | 8 | 3 |
| Moving wallpaper | 38 | 30 | 15 | 25 | 9 | 3 |
| Single rectangle | 60 | 61 | 1 | 13 | 14 | 0 |
| Multiple rectangles | 60 | 61 | 4 | 10 | 10 | 1 |
| Multiple RGB images | 60 | 59 | 1 | 13 | 12 | 0 |
| Multiple ARGB images | 60 | 59 | 6 | 12 | 7 | 0 |
| Rotated ARGB images | 34 | 38 | 15 | 22 | 11 | 0 |
| Multiple labels | 58 | 59 | 10 | 12 | 3 | 0 |
| Screen sized text | 7 | 6 | 128 | 139 | 8 | 4 |
| Multiple arcs | 30 | 22 | 25 | 35 | 4 | 3 |
| Containers | 60 | 65 | 4 | 6 | 11 | 0 |
| Containers with overlay | 20 | 20 | 35 | 40 | 11 | 4 |
| Containers with opa | 60 | 65 | 6 | 7 | 9 | 0 |
| Containers with opa_layer | 34 | 33 | 20 | 28 | 8 | 1 |
| Containers with scrolling | 19 | 15 | 43 | 58 | 5 | 4 |
| Widgets demo | 19 | 19 | 92 | 95 | 7 | 3 |

原始日志：`artifacts/lvgl-tuned-p4-20261102/p4-run.log`。
