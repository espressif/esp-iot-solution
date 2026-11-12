# RGB / MIPI 扫描边界专项

目的：验证驱动观测到的旧 framebuffer 源访问结束，先于 V2 释放该缓冲和允许下一次构建。此工程不修改生产 V2、IDF 原文件或公开接口，也不证明光学显示完成／无撕裂。

## 两类验收

| 路径 | 工程行为 | 可得结论 |
| --- | --- | --- |
| S3 RGB bounce，连续刷新 | 实际 V2，2/3 FB × PARTITION/FULL × 0/90/180/270°，每例 6 帧，异步复制、后台 service | 驱动事件下的源访问／释放／写入顺序 |
| P4 MIPI DPI | 同上；额外订阅 VSYNC 以启用观察中断 | DMA 完成、下一帧选择、V2 退休的顺序 |
| RGB 无 bounce 连续刷新 | 原始 IDF API，2/3 FB，记录描述符链重连及可用事件 | 只做 CHARACTERIZED，不宣称 V2 支持 |
| RGB 无 bounce 单次刷新 | 原始 IDF API，尾链结束，显式 `refresh()` 重启 | 只做 CHARACTERIZED，不宣称 V2 支持 |
| PSRAM 压力 | 512 KiB 源＋512 KiB 目标，循环复制，间隔 5 ms；仅在帧处理期间启用，导出期间暂停；2/3 FB × PARTITION/FULL，90°，每例 6 帧 | 该压力条件下的时序和 underrun 记录，不等于最坏带宽 |
| RGB 显式 restart | 通过 IDF restart API 请求，下一 VSYNC 执行 | 覆盖显式 restart；不等同于自然缺 EOF／硬件故障注入 |

V2 的 RGB 适配器仍明确拒绝非 bounce；单次刷新也不属于其周期完成事件契约。Host 测试固定了拒绝行为。

## 可复现隔离构建

先准备现有 `../boards/deps`（参见 boards/README.md），然后：

```sh
python3 prepare.py --idf "$IDF_PATH" --output /tmp/pv2-boundary-run
idf.py -C /tmp/pv2-boundary-run -B /tmp/pv2-boundary-run/build-s3 \
  -DIDF_TARGET=esp32s3 -DSDKCONFIG=/tmp/pv2-boundary-run/sdkconfig.s3 build
idf.py -C /tmp/pv2-boundary-run -B /tmp/pv2-boundary-run/build-p4 \
  -DIDF_TARGET=esp32p4 -DSDKCONFIG=/tmp/pv2-boundary-run/sdkconfig.p4 build
```

`prepare.py` 复制 IDF esp_lcd、V2 的 src/include，以及已确认的板级配置。每个注入锚点必须唯一匹配，否则报错，不继续猜测驱动布局。输出目录必须不存在。`instrumentation_manifest.json` 保存被修改源文件的输入／输出 SHA256；生成目录是源码快照，生产代码变化后必须重新生成。

所有修改只存在于生成目录：RGB EOF/读源/VSYNC/restart/link-switch，MIPI DMA 结束/重新装载/VSYNC，以及 V2 提交/设防/确认/释放/构建边界。MIPI 专项额外注册一个不执行工作的 VSYNC 回调，启用该观察事件；生产适配器不增加中断订阅。

通过 `idf.py menuconfig` 的 “V2 scanout boundary test” 选择：

- 默认：正常 V2 矩阵，分析时 `--expected-cases 16`。
- `BOUNDARY_STRESS=y`：四例代表性矩阵，`--expected-cases 4`。
- S3 `BOUNDARY_INJECT_RESTART=y`：四例显式 restart，`--expected-cases 4`，可与压力同时使用。
- S3 `BOUNDARY_RAW_RGB=y`＋`BOUNDARY_NO_BOUNCE=y`：原始连续扫描，`--expected-cases 2`。
- 再加 `BOUNDARY_ON_DEMAND=y`：原始单次扫描，`--expected-cases 2`。

无 bounce 模式不会伪造 frame_buf_complete 或凭 VSYNC 释放 V2 FB。S3 非 bounce 连续扫描的 EOF 本身不是可靠退休契约；缺少 link-switch 事件的平台只记录实际可用事件。

烧录前重新读取芯片 MAC，使用确认后的 by-id 端口；不要沿用历史 ACM 编号。建议保留当前固件备份。使用生成工程的 flash 参数烧录，不覆盖其他板子。

```sh
# P4 使用 --usb；S3 UART 不加。capture 会复位板子并运行一次专项。
python3 capture.py CONFIRMED_PORT /tmp/boundary.log --usb
python3 analyze.py /tmp/boundary.log --expected-cases 16 --output /tmp/boundary.json
```

## 记录与判断规则

记录器在内部 RAM 中预分配 8192 项（S3 为 192 KiB，测试专用），通过 ISR/task 共用临界区串行写入，临界区内采集单调时间戳和序号。不分配、不输出日志、不调用像素操作，也不等待任务工作。满后停止保存新项并累计 dropped，禁止悄悄覆盖证据。全部源停止后 freeze，任务侧输出文本。

RESTART detail 位 1/2/4 分别记录配置强制重启、API 请求、观测 EOF 数不足。重启可终止一个未完整填充的源帧区间，但 SELECT 仍保护它。

RGB 每次 bounce EOF 均记录；首块 memcpy 前到末块 memcpy 后记录整帧源读取区间，避免逐块双埋点淹没记录。SELECT 还保守保护了下一帧将要读取的源。MIPI 在 DMA 完成回调记录旧源读完，在重新启用 DMA 前记录新源 SELECT/READ_BEGIN。它们是驱动层访问标记，不是总线逻辑分析仪的最后一拍时间。

V2 WRITE_BEGIN 标记候选物理 FB 被构建预留，WRITE_END 在转换完成／READY 前；FULL 旋转时它也保守覆盖共享逻辑画布绘制阶段。RELEASE 标记旧扫描 FB 进入 FREE 前的位置。

检查器必须看到完整 suite、规定数量与完整模式组合的 case、连续序号、单调时间、零 dropped 和成功 stop。V2 还要求：

1. 所有构建目标已注册，写入期间没有源读区间重叠，也未被驱动选为扫描源。
2. SUBMIT／ARM／CONFIRM 的 cookie 匹配，默认两次已设防 callback 后才能确认。
3. 确认目标等于观测到的 selected FB；释放对象必须是它的前驱。
4. 释放时旧 FB 没有读引用且不再 selected，并存在该地址的末读证据。
5. 构建和事务全部结束；观察到 underrun 一律 FAIL。

结果分为 TRACE_PASS、CHARACTERIZED、FAIL、INCOMPLETE。TRACE_PASS 仅适用于 V2 轨迹；原始驱动观察永远不升级为 V2 通过。`min_release_margin_us` 是末读标记到释放标记的观测间隔，包含 ISR/调度延迟，不应直接作为可缩短 fence 的裕量。

## 本地验证及边界

生产组件 Host suite 已集成 C 记录器和 Python 分析器测试（15 个 Python 用例，完整 Host/LVGL 31 个 CTest 项）。分析器包含正常双/三缓冲 RGB/MIPI、在读时写、提前释放、单边界确认、cookie 错配、截断、溢出、序号异常和 underrun 等负例；记录器覆盖多线程、满缓冲、freeze 和 reset。通过根目录 `tools/dev.py test --with-lvgl` 和 `--sanitize` 运行。

本轮只验证默认双边界确认；不外推到 `single_boundary_retire=true`。MIPI 原生 underrun 目前仅打印日志，生产适配器的 FAULT 转发／恢复不在本轮实现内，专项观察到则失败。

本轮仍不覆盖：所有 IDF 版本／SoC、cache-disabled/Flash 擦写场景、任意 DMA 预取深度的形式化证明、故意屏蔽 EOF 的故障注入、所有总线饱和组合、光学无撕裂。注入记录器本身有时序扰动，实测不得当成无埋点固件的延迟保证。

实测记录和未完成项见 [RESULTS.md](RESULTS.md)。
