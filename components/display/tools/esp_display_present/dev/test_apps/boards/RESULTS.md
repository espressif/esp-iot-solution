# 历史迁移验收 · 2026-10-30

以下数值对应当时测试固件和口径，不代表当前提交的结果。最新同配置 LVGL benchmark、测试项及限制见 [当前验证](../../VALIDATION.md)；其中旧 64×16 TE ROI 不作为物理整屏 TE 验收。

用户本轮确认三块板画面均正常。重新识别身份后烧录；下表只统计完成的整轮，不计尾部未完成循环。

| 板子 / 身份 | 本轮路径 | 结果 |
| --- | --- | --- |
| P4 `e8:f6:0a:e7:a7:8d` / 1024×600 MIPI | **V1 vs V2 同板稳态吞吐基准 (`BENCH_V1_V2`)**：7 组模式同负载对比 | **V2 全面追平或反超 V1**：`DOUBLE_FULL_0` `30.1 vs 30.1 FPS (33121 µs)`；`DOUBLE_DIRECT_0` `60.3 vs 60.3 FPS (16557 µs)`；`DOUBLE_PARTIAL_0` `60.3 vs 60.3 FPS (16562 µs)`；`TRIPLE_PARTIAL_0` V1 `60.3` vs **V2 `60.8 FPS`**；`TRIPLE_PARTIAL_90` V1 `60.3` vs **V2 `61.2 FPS`**；`DOUBLE_FULL_90` V1 `18.2` vs **V2 `30.1 FPS` (+65%)** |
| P4 同板 | **FB 2/3 缓冲异步拷贝 × 跨帧流水切换衔接剖析 (`FB_HANDOFF_PASS`)**：5 模式连续 in-flight 排队提交 | 5 模式全部通过；2 FB `used_mask=0x3`、3 FB **`used_mask=0x7`**（证三块物理 FB 全部轮转）；`DOUBLE_DIRECT` 平均封帧耗时仅 **`557 µs`**，`DOUBLE_PARTIAL` 平均封帧 **`559 µs`**；`TRIPLE_FULL` 平均 VSYNC 等待仅 **`2598 µs`**；`cancel_drain_us = 59~69 µs` |
| P4 同板 | 规划入口，FULL/DIRECT/PARTIAL，双/三 FB，PARTIAL 两 tile | 五模式完整轮次，逐像素对照与确认；stop/recreate 后 heap 回归 |
| P4 同板 | CPU/PPA/DMA2D 独立算子对照（内部/PSRAM，RGB565/RGB888、四方向） | 32 例：PPA 16、DMA2D 4、明确 CPU 回退 12 |
| P4 同板 | **显示组合**：`DOUBLE_FULL` / `DOUBLE_DIRECT` / `TRIPLE_PARTIAL` × (`async-cpu` / `async-ppa` / `async-dma2d`) × 5 次动态旋转 (`0°→90°→180°→270°→0°`) | 9 组全部 `COMBO_PASS`（每组 10 帧逐像素旋转比对通过）；`DOUBLE_FULL` 下 `async-ppa` 平均等待 **34.2 ms**（对比 `async-cpu` 375.4 ms 提速 11 倍）；`TRIPLE_PARTIAL` 下 `async-dma2d` 平均等待 **70.7 ms**（对比 `async-cpu` 96.7 ms） |
| S3 `90:e5:b1:d0:a9:d8` / 480×480 RGB bounce | **V1 vs V2 同板稳态吞吐基准 (`BENCH_V1_V2`)**：7 组模式同负载对比 | **6 项全面追平或反超 V1**：`DOUBLE_FULL_0` `60.3 vs 60.3 FPS (16574 µs)`；`TRIPLE_FULL_0` `60.7 vs 60.3 FPS (16574 µs)`；`DOUBLE_DIRECT_0` `60.3 vs 60.3 FPS (16575 µs)`；`DOUBLE_PARTIAL_0` `60.3 vs 60.3 FPS (16575 µs)`；`TRIPLE_PARTIAL_0` V1 `60.3` vs **V2 `63.3 FPS`**；`TRIPLE_PARTIAL_90` V1 `60.3` vs **V2 `63.3 FPS`** |
| S3 同板 | **FB 2/3 缓冲异步拷贝 × 跨帧流水切换衔接剖析 (`FB_HANDOFF_PASS`)**：5 模式连续 in-flight 排队提交 | 5 模式全部通过；2 FB `used_mask=0x3`、3 FB **`used_mask=0x7`**；`DOUBLE_DIRECT / PARTIAL` 平均封帧耗时仅 **`771 / 774 µs`**；`TRIPLE_FULL` 平均 VSYNC 切换等待 **`1869 µs`**；`cancel_drain_us = 63~95 µs` |
| S3 同板 | 五模式基线 + **显示组合**：`DOUBLE_FULL` / `DOUBLE_DIRECT` / `TRIPLE_PARTIAL` × `async-cpu` × 5 次动态旋转 (`0°→90°→180°→270°→0°`) | 五模式 240 帧 + 3 组旋转异步组合（30 帧）全部 `PASS` / `COMBO_PASS`；每组销毁后内部空闲稳定回归 **352319 B**、PSRAM **16774784 B**（峰值最低内部剩余 222752 B） |
| S31 `30:ed:a0:f4:0b:34` / 480×480 QSPI | **V1 vs V2 同板稳态吞吐基准 (`BENCH_V1_V2`)**：GRAM 全屏 + TE 单/双 compose | `GRAM_FULL_480x480` V1 `37.0 FPS (27000 µs)` vs **V2 `38.0 FPS (26303 µs)`（反超）**；`TE_COMPOSE2_FULL` V1 `39.6 FPS (25250 µs)` vs **V2 `39.7 FPS (25162 µs)`（反超）**；`TE_COMPOSE2_PARTIAL` V1 `39.8 FPS` vs **V2 `38.6 FPS`** |
| S31 同板 | **全屏动态相位 TE (`TE_FULL_DYNAMIC_PHASE`)**：`period=16728 µs, scan=16190 µs, edge_to_scan=538 µs, guard=681 µs, budget=24940 µs` | **24 帧全部通过**；理论安全窗口 `[681, 7278) µs`，微秒级准点入窗后实测全部 24 帧提交相位精准锁定在 **`[704, 715] µs`（抖动仅 11 µs）**，最大传输 `23855 µs <= budget` |
| S31 同板 | **TE 2×2 池组合 × `cancel_frame` × 分段时延/空等待剖析**：`(compose=1..2) × (tiles=1..2)` | 4 组全部 `TE_MATRIX_PASS`；`avg_draw_sub_us` 降至 **`4910 µs`**，`compose=2, tiles=2` 平均封帧到发送等待仅 **`7129 µs`**（最大 **`9293 µs`**） |
| S31 同板 | GRAM 全屏 / TE 全屏双 compose（PSRAM→SPI2） | GRAM 8 帧（240 次传输）；TE 12 帧（最长传输 23740 µs）；边沿门控验证 |
| S31 同板 | **显示组合**：480×480 全屏 `GRAM_ASYNC_ROT`、`TE_ASYNC_ROT (单 compose=1)`、`TE_ASYNC_ROT (双 compose=2)` × 5 次动态旋转 (`0°→90°→180°→270°→0°`) | 3 组全部 `COMBO_PASS`（GRAM 300 次传输 `max_tx=1090 µs`；单/双 compose TE 各 10 帧 `max_tx=23862/23861 µs`）；销毁后内部空闲稳定回归 **443811 B**、PSRAM **16774284 B** |
| S31 同板 | GRAM 64×16 / TE 64×16 | GRAM 32 帧；TE 连续多轮 120 帧/轮，零记录窗口超出 |

S31 初次双全屏内部池创建失败（内部剩余 443811 B，一张图 460800 B）。没有缩小全屏测试，而是核查本地 IDF 的 S31 SPI2 外部 DMA 支持后，显式打开 LCD IO 和 adapter 的 `psram_dma_direct`，将 compose 放入 PSRAM。后续各轮内部空闲值回到 443811 B。原失败日志保留，不能把配置不可能创建误报为通过。

全屏 TE 固件明确打印 `TE_FULL_EDGE_SYNC_UNVERIFIED`：其发送预算 33442 µs、边沿门控 50163 µs，不是光学无撕裂证明。539 µs TE 高脉冲仅用于保守 ROI。动态线性模型已通过 Host 数学/边界测试，真实面板扫描规律和最坏写入包络仍需单独测量。

本轮 Host/LVGL 29/29，ASan/UBSan 29/29，`detect_leaks=0`。S3/P4+LVGL 和 S31 最终离线构建通过。延迟 copy/发送、明确拒收与不确定接受、cancel/stop 重试、版本 repair 及多实例以 Host 测试验收；三板补充验收已覆盖 S3/P4/S31 的运行时五次方向切换 (`0°→90°→180°→270°→0°`)、异步 copy、单/双 compose 切换及 P4 PPA/DMA2D 硬件加速与显示的端到端组合（日志见 `artifacts/{s3,p4,s31}-combo.log`）。

证据位于忽略目录 `artifacts/`：`migration-summary.json`、`migration-{p4,s3,s31}.bin` 和 `pv2-all-*.log`。JSON SHA256 对应实际烧入文件。板测之后增加的规划阶段非法时序/长度/反向 band 对齐检查已独立 Host 与离线编译；不把随后重建的 bin 哈希冒充已烧版本。

下面是各早期切片的历史记录。

---

# 首轮上板验收 · 2026-10-30

范围：V2 Presenter → FB → LCD scanout；CPU、RGB565、DOUBLE_FULL、零旋转。
S3 480×480 RGB bounce，P4 1024×600 MIPI-DSI。两块板完整原 Flash 已备份；
本地 ESP-IDF 6.2 构建通过，esptool 写入校验通过。未修改 V2 生产代码。

| 板子 | 日志确认帧范围 | 最大等待时间 | 采样内部空闲 heap | pending / sticky | 画面 |
|---|---|---|---|---|---|
| S3 | 0..240 | 34055 µs | 300635 B | 0 / ESP_OK | 用户确认正常 |
| P4 | 0..240 | 34324 µs | 513363 B | 0 / ESP_OK | 用户确认正常 |

frame 从 0 开始，每帧等待实际 PRESENT_CONFIRMED，再释放 receipt；前 8 帧与每
120 帧打印日志。因此记录证明至少 241 次确认，非只有打印的 10 次。两板各抓取
45 秒控制台，采样 heap 不变，未发现 FAIL、assert 或崩溃。首次提交后的等待也
无需第二次提交即可完成，验证了两次帧事件确认路径在当前驱动/板子上能够推进。

用户确认纯色、彩条和移动图案显示正常。这是有限时长、目视的基线验收，不是
光学无撕裂证明、并发穷举或长期内存泄漏证明。等待时间包含两次帧边界与调度，
不是像素绘制耗时或吞吐测试。S3 初始化报告 GPIO3 已配置的警告（BSP 要求先
拉高 VSYNC）；后续初始化与帧确认成功，未将该警告作为隐藏失败忽略。

本地证据位于忽略目录 `artifacts/`：`s3-run1.log`、`p4-run1.log`、两板 build/flash
日志、`acceptance.json`（本次烧入 bin 的 SHA256）、`original-backups.json` 及
完整备份。源码在采集后仅按仓库风格格式化，记录的哈希仍对应本次实际烧入文件。

下一轮依次验收：PARTIAL / DIRECT、三缓冲、LVGL、stop/recreate、旋转/触摸、
硬件加速和长时间压力。GRAM/TE 需要相应接口屏幕，不能用本次 RGB/MIPI 结果替代。

## 第二轮：五模式与停止重建 · 2026-10-30

两板均重新编译、烧录并核对 MAC，写入哈希校验成功。V2 生产代码未改动。
测试范围为同步 CPU、RGB565、零旋转，每项 48 帧后真实停止/销毁面板及
Presenter，再释放板级总线/IO/expander/LDO，重新创建下一模式。

| 模式 | S3：确认帧数 / 最大 commit→确认时间 | P4：确认帧数 / 最大 commit→确认时间 | 整帧像素对照 |
|---|---|---|---|
| DOUBLE_FULL | 144 帧 / 34.12 ms | 96 帧 / 30.19 ms | 通过 |
| TRIPLE_FULL | 144 帧 / 34.11 ms | 96 帧 / 30.18 ms | 通过 |
| DOUBLE_DIRECT | 144 帧 / 65.81 ms | 96 帧 / 68.35 ms | 通过 |
| DOUBLE_PARTIAL | 144 帧 / 30.73 ms | 96 帧 / 33.60 ms | 通过 |
| TRIPLE_PARTIAL | 144 帧 / 30.75 ms | 96 帧 / 33.62 ms | 通过 |

- S3 完成 3 轮，共 720 帧；P4 最终修正版完成 2 轮，共 480 帧。只统计完整循环，尾部未完循环不计入。
- 每帧要求 PRESENT_CONFIRMED、terminal、ESP_OK；无 pending receipt 遗留或 sticky fault。
- DIRECT/PARTIAL 首帧全屏覆盖，随后更新旧/新两个 37×59 脏矩形；PARTIAL 以 11 行拆 band，脏区源 stride 带 3 像素 padding。逐帧核对整个提交缓冲，包含脏区之外的保留内容。
- 每项 stop/destroy 成功，heap 完整性检查成功。销毁后 S3 内部空闲 353087 B、PSRAM 16774784 B；P4 分别 594891 B、33551868 B，已完成轮次/模式之间均一致。
- 用户对两板各模式目视反馈“OK”。切换模式时短暂黑屏是实际删除并重新初始化屏幕的预期行为。
- 初轮 P4 暴露测试工程未释放背光 GPIO26 的重复配置警告；补上 `gpio_reset_pin(26)` 后重新烧录，最终记录未再出现该警告。未改 V2 状态机。

证据：`artifacts/s3-suite2.log`、`p4-suite2-final.log`、两份 `*-suite2-summary.json`、
`*-suite2-build.log`、`*-suite2-flash.log`、`suite2-firmware.json`（实际烧入固件及测试源码 SHA256）。
P4 初轮 `p4-suite2.log` 仅保留定位警告的过程，不计入最终表格。

**验收边界**：每帧等待完成后才继续。三缓冲验证配置/分配与生命周期，尚未验证
第三块缓冲必然被使用、满队列、背压或连续流水吞吐。逐像素对照读取 CPU 可见
的提交缓冲，不等同于独立的 DMA 总线/光学采样。有限轮次 heap 稳定不代表长期无泄漏。

下一轮：三缓冲排队/背压与实际第三缓冲使用 → LVGL PARTIAL/FULL/DIRECT →
旋转/触摸 → DMA2D/PPA 对照及长时压力。RGB888、SPI/TE 实屏仍需独立验收。

## 第三轮：S31 QSPI GRAM / TE 与流水线收敛 · 2026-10-30

新增板身份：ESP32-S31 rev0.0，MAC `30:ed:a0:f4:0b:34`，ACM1，16 MB Flash。
eFuse 硬件版本 `0x0100`，CO5300 480×480，CLK44/RST42/TE43/CS50。
完整原 Flash 已备份并计算 SHA256，测试固件写入校验通过。

| 工况 | 验收结果 |
|---|---|
| TE 边沿测量 | 1秒 60 个上升沿，周期 16717～16754 µs，最短高脉冲 539 µs |
| 全屏 GRAM | 480×480，8帧，240次发送 / 240次完成回调；单band最大1012 µs |
| ROI GRAM 校准 | 64×16，32帧，32次发送 / 32次回调；最大252 µs |
| TE ROI | 64×16，窗口539 µs、预算352 µs；10轮 / 1200帧全部完成 |
| TE ROI 实测 | 最大发送274 µs，最大提交边沿年龄44 µs，逐笔完成检查越窗0次 |
| 生命周期 | 每轮 Presenter/backend/SPI adapter 停止销毁并重建；回调数量一致，内部空闲内存持续444451 B |
| 画面 | 用户对所描述彩条及中央移动区域回复“是的”；非光学采样验证 |

保留首轮128×64的拒绝记录：预算1696 µs超过539 µs高脉冲窗口，因此没有强行发送。
40MHz四线QSPI全屏RGB565纯载荷至少23.04ms，大于实测约16.75ms周期。本轮不声称
全屏TE无撕裂，也不将高脉冲等同于已验证光学消隐区。SPI驱动的回调替换警告发生在
排空后的注销/重新注册，计数平衡。屏幕、SPI IO 与 GPIO事件服务保留到复位，未测面板电源循环。

本轮同时将 FB 内部推进改为 IDLE/PENDING/COMPLETE/FAILED，区分封帧尝试与已接受
操作的错误收尾，不改变公共 API。普通 Host + LVGL 21/21、ASan/UBSan 21/21
（detect_leaks=0）；scanout/repair/fb各重复100次通过；S3核心、P4+LVGL、S31板测
固件离线构建通过。S3/P4本轮未重新烧录，其此前板测不算新重构代码的实板回归。

证据在 `artifacts/s31-qspi-summary.json`、`s31-qspi-roi64.log` 及构建/烧录/Host日志。
现有两板 FULL/DIRECT/PARTIAL、QSPI GRAM 和有限ROI TE 已具备基线；后续保持范围：
三缓冲排队实板 → LVGL实板。全屏TE策略、旋转、硬件加速及业务并发压力分别验收。

## 第四轮：执行器插件 P4 验收 · 2026-10-30

ACM0经esptool重新识别为ESP32-P4 rev3.2，MAC `e8:f6:0a:e7:a7:8d`，与原备份身份一致。新固件烧录哈希校验通过。启动先运行执行器内存测试，再进入原CPU显示五模式循环。

| 路径 | 内部内存 | PSRAM | 对照结果 |
| --- | ---: | ---: | --- |
| PPA硬件，RGB565/RGB888，四方向（RGB565含swap） | 8 | 8 | 全3072B目标与CPU参考一致 |
| DMA2D硬件，两格式，0° | 2 | 2 | 全目标一致 |
| DMA2D不支持旋转，显式CPU回退 | 6 | 6 | 返回TRANSFORM原因，全目标一致 |

合计32例：硬件20例、CPU回退12例。每例先probe/选择，再将所记录的执行器直接注入异步Pixel Engine；等待ticket完成、stop/join/release/destroy后才释放执行器与像素。不是只打印配置中的硬件名称。结束heap完整性检查通过。

随后CPU显示五模式完成一轮240帧，pending=0、sticky=ESP_OK，逐帧整图CPU对照通过；各模式销毁后内部空闲592275B、PSRAM33551868B一致。此次未请求新的目视确认，不宣称加速LCD输出或光学无撕裂。

Host普通+LVGL及ASan/UBSan均25/25（detect_leaks=0，未做泄漏检测），包括CPU-only、PPA-only、DMA2D-only、双硬件四种独立链接组合。S3及P4+LVGL离线构建通过，S3未重新烧录。

证据：忽略目录`artifacts/executor-summary.json`记录32例路径、固件和源码SHA256；同目录`executor-board-run.log`、`executor-board-flash.log`、`executor-board-build.log`以及Host/ASan/离线编译日志。仅证明当前驱动/硬件/内存布局组合，未穷举cache/PSRAM边界，也不是全架构迁移完成。


## S0 规划兼容入口实板回归（2026-10-30）

重新读取 MAC 确认 P4 `e8:f6:0a:e7:a7:8d` 后烧录。`main.c` 五种 FB 模式改为先读取 scanout caps、resolve，再调用 create_planned；PARTIAL 显式 tile_count=1，与尚未迁移的 FB 运行时一致。

- Executor：32/32 像素对照通过（硬件20、CPU fallback12）。
- DOUBLE_FULL/TRIPLE_FULL/DOUBLE_DIRECT/DOUBLE_PARTIAL/TRIPLE_PARTIAL 各48帧，合计240帧，均 pixels=ALL、pending=0、sticky=ESP_OK。
- 五轮清理后 internal=592147、PSRAM=33551868 字节，均一致。
- 日志/固件和源码哈希：`artifacts/s0-s1-summary.json`、`s0-s1-board-run.log`；Host与离线构建日志同前缀归档。
- 本轮未再次取得肉眼确认；像素/回执结果不能证明真实无撕裂。此处只验证新规划入口的兼容 FB 路径，不覆盖新旋转拓扑、TE 流水或 S1 GRAM 实板迁移。
