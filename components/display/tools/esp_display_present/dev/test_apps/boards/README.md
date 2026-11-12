> 当前统一入口见 [dev/README.md](../../README.md)，板子身份以 [boards.json](../../boards/boards.json) 为准。下面保留历史专项命令与验收记录；端口号可能已经变化。P4跨轴问题已在2026-11-06修复并通过Host，旧诊断的失败描述不代表最新代码状态。

# Display presenter board acceptance application

This independent ESP-IDF application exercises the current presenter engine,
its application facade and panel adapters using driver-owned framebuffers. It
does not link a separate V1 implementation, LVGL, Wi-Fi, audio, storage or touch
services. It is not a power benchmark. See [current benchmark and validation
results](../../VALIDATION.md) for the separate LVGL application.

## 2026-11-03 模式／旋转矩阵

[完整结果、帧率及已发现问题 (historical archive)](../../ARCHIVE.md)。三板运行前需按 MAC 识别、备份并校验整片 Flash，结束后恢复并独立验证；程序会重设面板并暂时覆盖原固件。P4 和 S3 测试期间会切换彩条，S31 的 TE 测试始终使用物理 480×480 整屏传输。

从本目录执行（分别指定临时构建目录与各自的 sdkconfig）：

```sh
source /home/xuxin/esp_work/esp-idf-qspi/export.sh
idf.py --preview -B /tmp/present-matrix-p4 -DIDF_TARGET=esp32p4 -DSDKCONFIG=/tmp/present-matrix-p4.sdkconfig -DPRESENT_MODE_ROTATION_MATRIX=ON build
idf.py --preview -B /tmp/present-matrix-s3 -DIDF_TARGET=esp32s3 -DSDKCONFIG=/tmp/present-matrix-s3.sdkconfig -DPRESENT_MODE_ROTATION_MATRIX=ON build
idf.py --preview -B /tmp/present-matrix-s31 -DIDF_TARGET=esp32s31 -DSDKCONFIG=/tmp/present-matrix-s31.sdkconfig -DPRESENT_MODE_ROTATION_MATRIX=ON build
```

`PRESENT_MODE_ROTATION_MATRIX` 检查公开入口、输出区域和整帧像素；S31 还检查 4 像素对齐的局部输入及 TE 整屏输出。每个实例先渲染、再尝试同轴运行时旋转。`PRESENT_NATIVE_ROTATION_MATRIX=ON` 用于 P4／S3 的 native 跨轴旋转、脏区和多帧在途缓冲检查，需与前一选项互斥。

P4 已知运行时跨轴问题可用 `PRESENT_MODE_ROTATION_MATRIX=ON` 与 `PRESENT_P4_CROSS_AXIS_PROBE=ON` 单独重现：当前应记录 `OUT_OF_PANEL` 和失败，**这不是通过项**。该诊断会显示几帧图像，仅用于明确授权的测试板。需要覆盖 P4 全模式、四个起始方向及三个其他目标方向时，改用 `PRESENT_P4_TRANSITION_MATRIX=ON`（同样需 `PRESENT_MODE_ROTATION_MATRIX=ON`）：程序会分别记录 `TRANSITION_PASS` 与 `TRANSITION_UNSAFE`，并输出汇总。测试中的合成帧率与 LVGL benchmark 的统计口径不同。

## Confirmed hardware

| Target | Board / panel | Identity | Console |
|---|---|---|---|
| S3 | LCD EV Board, R16 module, GC9503 480×480 RGB565 | `90:e5:b1:d0:a9:d8`, revision 0.2, Flash/PSRAM 16 MB | `/dev/ttyUSB0` |
| P4 | Function EV Board, EK79007 1024×600 MIPI-DSI RGB565 | `e8:f6:0a:e7:a7:8d`, revision 3.2, Flash 16 MB | `/dev/ttyACM0` |

Port names can change. Verify chip and MAC before flashing. S3 R16 uses I2C
SDA47/SCL48 and RGB D6=8/D7=18; R8 pins are different. P4 is built for revision
3.1+, not pre-v3 silicon. Pin/timing references are Espressif BSP S3 **4.1.0** and
P4 **noglib 5.2.3**. Only the panel/expander drivers are linked, not those BSPs.

## Build offline

Use the audited local ESP-IDF 6.2 tree. `prepare_deps.py` copies pinned official
drivers from Component Manager's local cache, retaining licenses and sources.
It replaces dependency CMake files with explicit IDF requirements and version
macros; no dependency downloads are performed. `deps/manifest.json` records
original source hashes. Pass `--cache PATH` for another cache location.

```sh
source /home/xuxin/esp_work/esp-idf-qspi/export.sh
python3 prepare_deps.py
idf.py -B build-s3 -DIDF_TARGET=esp32s3 -DSDKCONFIG=sdkconfig.s3 build
idf.py -B build-p4 -DIDF_TARGET=esp32p4 -DSDKCONFIG=sdkconfig.p4 build
```

After confirming identity and backing up the original flash:

```sh
idf.py -B build-s3 -DSDKCONFIG=sdkconfig.s3 -p /dev/ttyUSB0 flash monitor
idf.py -B build-p4 -DSDKCONFIG=sdkconfig.p4 -p /dev/ttyACM0 flash monitor
```

The build writes bootloader, partition table and test application. Full original
16 MB flash backups are kept locally under ignored `artifacts/`; restoring the
matching full backup restores the original flash layout/content. Never restore
one board's backup to the other board. The bootloader/partition table are part
of this test firmware, so application-only updates are insufficient initially.

## First acceptance stage

- Two physical framebuffers; FB0 is declared initially scanning.
- S3 continuous RGB uses two internal bounce buffers of 10 scanlines.
- P4 uses two DSI lanes at 1000 Mbps and the driver's 60 Hz EK79007 timing.
- Native RGB565; zero rotation; no DMA2D/PPA, async copy or background worker.
- Each frame follows begin/acquire/draw/submit/commit/wait/release/service.
- Wait requires `PRESENT_CONFIRMED`, terminal status and `ESP_OK` within 2 s.
  The V2 adapter fences submissions with two subsequent frame-complete events.
- Red, green, blue, white, black, yellow, cyan and magenta appear for ~1 s each;
  then eight vertical color bars, a white border, a moving white bar and a binary
  frame counter. The sequence repeats every 240 frames.
- Logs report confirmed frames, wait time, pending receipts, sticky errors and
  internal free heap. A failure retains resources and prints the failing stage
  every 5 s; it does not free memory that DMA might still access or reboot-loop.

This first runner intentionally keeps the panel and board buses alive until
reset. It does **not yet test stop/recreate, touch, LVGL, DIRECT/PARTIAL, rotation,
RGB888 or acceleration**. Those are separate acceptance stages after this
baseline. Receipt success proves callback progress, not correct visible pixels
or tear-free timing. Confirm color order, border, moving bar and corruption on
the physical screen separately.

## Current runner: five-mode lifecycle suite

The current `main/main.c` supersedes the initial FULL-only pattern loop above.
It cycles through DOUBLE_FULL, TRIPLE_FULL, DOUBLE_DIRECT, DOUBLE_PARTIAL and
TRIPLE_PARTIAL, 48 frames per case, then repeats. One through five white squares
at the top identify the mode. A 37×59 white box moves over fixed color bars.

The first frame covers the whole screen. Subsequent DIRECT/PARTIAL frames only
redraw the previous and current box rectangles. PARTIAL uses 11-row scratch
bands; dirty regions have a three-pixel padding in their source stride. The
whole submitted framebuffer is compared against the expected image after each
PRESENT_CONFIRMED, covering preservation outside damage as well as band copies.
This is CPU memory verification, supplemented by physical visual inspection;
it does not independently measure DMA bus output or optical tearing.

Every case drains receipts, stops/destroys Presenter and scanout, deletes the
board's IO/bus/expander/LDO resources, and checks heap integrity. S3's 3-wire IO
is already deleted by the GC9503 constructor. Each subsequent case creates a
new driver pool and restores the FB0 initial-scanout reservation. Logs include
`PASS`, `CLEAN`, and `SUITE_PASS`. A brief blank at mode changes is expected
because the actual panel is deleted and reinitialized.

The producer waits for each frame before the next one; therefore this suite
validates three-buffer configuration/allocation and lifetime, **not a saturated
three-frame pipeline, guaranteed use of the third buffer, or backpressure stress**. Rotation, RGB888, LVGL,
acceleration and touch remain separate stages.

Capture and summarize at least two complete cycles (no automatic reset):

```sh
python capture.py /dev/ttyUSB0 artifacts/s3-suite2.log --seconds 180
python analyze_suite.py artifacts/s3-suite2.log --min-cycles 2
```

Run a separate capture for P4. The analyzer rejects failure/pixel-corruption
logs, repeated startup, incomplete mode sets, duplicate completed cases and
varying per-mode post-destroy heap across completed cycles. A trailing partial
cycle is excluded from totals. It does not replace visual confirmation.

## S31 Mosaico QSPI / TE acceptance

Reference: [Mosaico BSP](https://github.com/esp-mosaico/esp-mosaico-bsp/tree/master/components/esp-mosaico-bsp).
Local reference revision: `c973747027a14e14c795136fc7ed2ed33599d3b1`; initialization is
adapted from local BSP sources, not a full BSP dependency. CO5300 driver is pinned
at 2.2.0 in `prepare_deps.py`. eFuse USER_DATA selects board v1.0/v1.1/v1.2 pins;
unknown revisions fail instead of probing arbitrary pins. Identified board:
ESP32-S31 rev0.0, MAC `30:ed:a0:f4:0b:34`, Flash16MB, `/dev/ttyACM1`.

From the V2 component root:

```sh
python3 test_apps/boards/prepare_deps.py
idf.py -C test_apps/boards -B build-s31 -DIDF_TARGET=esp32s31 -DSDKCONFIG=sdkconfig.s31 build
```

The S31 target selects `qspi_main.c`/`board_qspi.c`. CPU/native RGB565 is converted
to swapped wire order by V2; SPI adapter sources remain internal DMA memory.
QSPI=40MHz, TE GPIO43 (rising edges forwarded, both edges measured), panel 480×480.
The test uses source-release completion, not unsupported optical/present confirmation.

1. Measure TE period and minimum high-pulse duration for 1 s.
2. Render 8 full-screen GRAM frames in 16-row bands, checking receipt results and
   exact transmission/callback counts.
3. Calibrate a centered 64×16 GRAM ROI over 32 frames. Set transfer budget to the
   measured maximum plus 100us, require it to fit the measured TE high pulse.
4. Repeatedly create/stop/destroy a TE Presenter for 120 ROI frames. ISR publishes
   only edges; the owner task sends. Count edge-to-completion overruns against the
   pulse-derived window and retain resources on failure. Clear the ISR's Presenter
   pointer under the same critical section before stop/destroy. The panel/IO and
   GPIO ISR service intentionally live until reset; this is Presenter lifecycle
   testing, not panel power-cycle testing.

At 40MHz quad SPI, 480×480×16 bits requires at least 23.04ms of payload time;
full-screen transfer cannot be assumed to fit a measured ~16.75ms refresh cycle.
A measured high pulse is only a diagnostic timing bound, **not a verified optical
blanking interval**. Passing ROI scheduling does not prove full-screen tear-free
TE support. This runner deliberately refuses an oversized transfer instead of
inflating the configured window. Initial 128×64 ROI was rejected for that reason.

The vendor IO logs callback replacement when the adapter unregisters/re-registers
between runs; each run first drains transfers. SPI max_inflight is 1. A later
stress test is needed for queue saturation and concurrent business traffic.
Original full Flash backup and its SHA256 are under ignored `artifacts/`.

## P4 executor plugin acceptance

P4 builds additionally run `main/executor_check.c` before the existing display
suite. The check executes 32 CPU-reference memory comparisons through the queued
Pixel Engine: PPA/DMA2D × internal/PSRAM × RGB565/RGB888 × four rotations. It
requires hardware selection for supported internal-memory cases, reports every
actual executor and reason, compares the entire 3072-byte destination allocation
(including untouched pixels), and checks heap integrity after teardown. No
accelerator is silently replaced after execution starts. The subsequent display
suite remains CPU-driven; this check does not claim accelerated LCD presentation.

Build from the component root after activating the local IDF:

```sh
idf.py -C test_apps/boards -B build-p4-executors -DIDF_TARGET=esp32p4 -DSDKCONFIG=sdkconfig.p4 build
```

Expect `EXECUTOR_SUITE_PASS cases=32 hardware=20 cpu_fallback=12` on the tested
P4 revision and layouts. Counts are evidence for this firmware/platform, not a
required capability assumption for other targets or stricter external memory.


## 当前整体迁移测试

S3/P4 测试通过 `pv2_presenter_create_planned()` 使用 PARTIAL 两 tile 与独立图像池；P4 先运行执行器 32 例。S31 在全屏 GRAM 后运行 12 帧显式双 compose 全屏 TE，然后进入 ROI 循环。S31 图像池在 PSRAM，SPI2/LCD IO/adapter 均显式开启外部 DMA；区域和长度限制必须由能力协商满足。

全屏 TE 测试名含 `UNVERIFIED`，其较宽门控用于验证完整图像发送和池复用，不代表面板扫描安全窗口已经实测。最新证据与硬件身份见 RESULTS.md。固件依旧是专用测试工程，不修改产品构建依赖。


## S31 full-frame TE visual comparison (optional)

This diagnostic is selected only with `PRESENT_S31_TE_VISUAL=ON`; the normal
S31 runner and S3/P4 applications are unchanged. After preparing the offline
dependencies as above, activate the same local ESP-IDF and build, for example:

```sh
idf.py -C . -B /tmp/present-s31-te-visual \
  -DIDF_TARGET=esp32s31 -DPRESENT_S31_TE_VISUAL=ON \
  -DSDKCONFIG=/tmp/present-s31-te-visual.sdkconfig build
```

Identify the S31 by chip and MAC, back up its flash, and start serial capture
before the test starts; port numbers can change. The display repeats three
stages: direct panel IO without TE for at least 30 s (cyan upper-left marker),
the stable Presenter API with automatic TE measurement for at least 60 s
(magenta marker), and direct panel IO again for at least 30 s. All use the same
black background, colored border, fixed white marker and moving white block;
except for the stage marker the pixels are identical. Coordinates are aligned
to four pixels. The complete image is redrawn for every frame.

The direct baseline writes byte-swapped RGB565 into a full-screen, aligned
PSRAM buffer, requires successful cache synchronization, and waits for the
panel IO completion callback before reuse. The TE stage instead renders 16-row
tiles through Presenter with two compose buffers and waits for quiescence. The
upstream buffer and scheduling paths therefore differ. An instrumentation wrapper
counts calls to `esp_lcd_panel_draw_bitmap`; each stage requires exactly one
accepted 480×480 call per completed frame and zero other calls or errors. It
checks the driver-facing region, not the physical QSPI waveform.

Look for `STAGE_START`, `PROGRESS`, `STAGE_PASS` and `COMPARISON_CYCLE_PASS` in
the serial log; `FAIL` retains resources and stops progress. The TE scheduler
also reports its measured and estimated timing. The nominal 40 MHz quad-SPI
payload estimate for 460800 bytes is 23040 µs, excluding protocol overhead.
Passing the diagnostic or seeing a stable picture does not establish panel
optical timing or prove that tearing cannot occur.
