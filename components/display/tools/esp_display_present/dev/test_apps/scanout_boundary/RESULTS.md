# 扫描边界实板验收 — 2026-11-02

基于 V2 `628e131a6`、本地 IDF `7b9cc1ac79f` 的测试专用埋点副本；生产 V2/IDF 驱动未修改。芯片 MAC 烧录前重新读取：S3 `90:e5:b1:d0:a9:d8`（480×480 RGB GC9503），P4 `e8:f6:0a:e7:a7:8d`（1024×600 MIPI EK79007）。未烧录 S31。

## 验收结果

| 场景 | 场景数 | 确认/释放 FB 次数 | bounce EOF / 帧完成 / VSYNC | 最小末读→释放间隔 | 结论 |
| --- | ---: | ---: | ---: | ---: | --- |
| S3 RGB bounce 正常 | 16 | 96 / 96 | 15615 / 326 / 310 | 16281 μs | TRACE_PASS |
| P4 MIPI 正常 | 16 | 96 / 96 | 0 / 428 / 428 | 16579 μs | TRACE_PASS |
| S3 RGB bounce 带宽压力 | 4 | 24 / 24 | 14649 / 304 / 302 | 16449 μs | TRACE_PASS |
| P4 MIPI 带宽压力 | 4 | 24 / 24 | 0 / 231 / 234 | 16613 μs | TRACE_PASS |
| S3 显式 restart | 4 | 24 / 24 | 4408 / 92 / 88 | 16606 μs | TRACE_PASS |
| S3 无 bounce 连续链 | 2 | 0 / 0 | 0 / 37 / 37 | — | CHARACTERIZED，非 V2 支持 |
| S3 无 bounce 单次刷新 | 2 | 0 / 0 | 0 / 12 / 12 | — | CHARACTERIZED，非 V2 支持 |

正常矩阵为 2/3 FB × PARTITION/FULL × 四方向；压力与显式 restart 矩阵为 2/3 FB × PARTITION/FULL × 90°。每例 6 帧，异步 CPU copy + 后台 service；允许连续提交，再回收前序 receipt。压力使用两块各 512 KiB 的 PSRAM 区域循环复制，间隔 5 ms，仅帧处理期间启用，日志导出暂停。不是极限带宽或加速器压力结论。

合计 **44 个 V2 场景、264 次扫描确认及旧 FB 释放**，均未观察到旧 FB 在仍被选中/读取时释放或再次开放写入。另有 4 个原始 RGB 场景、24 次扫描请求；它们只证明驱动观察链路，不代表 V2 已支持无 bounce 或单次刷新。

所有计入结果的记录均零溢出、序号/时间完整、正常停止、无 watchdog/panic/underrun。末读→释放是驱动标记之间的观测间隔，不能当作总线最后一拍时间，也不能据此直接削减双边界 fence。

## 硬件路径得到的具体证据

- RGB bounce：两块 10 行内部 bounce，共 **6 个 DMA 描述符节点**。每次 EOF 填下一块，完整源帧取数边界与 LCD VSYNC 分开记录。
- RGB 无 bounce 连续：每张 480×480 RGB565 FB 对应 **113 个描述符节点**，驱动在提交时重连各链尾到目标 FB 链头。S3 本轮没有 link-switch 专用事件，不能拿普通 EOF 推导可靠退休。
- RGB 单次：同样 113 个节点，尾节点结束而非持续循环；每次显式 refresh，分别等待 DMA EOF 与 LCD VSYNC。实测 **EOF 早于 VSYNC 330～331 μs**。仅等 EOF 就再启动会绕过显示帧结束边界，最终工程已改为双事件等待。
- P4 MIPI：每张 FB 使用 **1 个完整帧描述符**；DMA 完成回调选取 `cur_fb_index`，重新装载/启用下一帧，再回调上层。专项额外订阅真实 bridge VSYNC，原生产适配器仍只订阅 FB 完成。
- 显式 restart：**24 次 API 请求、24 次实际 restart，原因位全部为 2（API 请求）**。本轮没有把它当成受控丢 EOF 的等价验证。
- S3 正常矩阵另记录过 1 次 restart；该轮较早的埋点未含原因位，不能归因于缺 EOF。后续压力轮未观察到自动 restart 或 underrun。

## 已收敛及保留边界

1. 保留生产默认双边界策略，没有凭经验改成一次；检查器同时验证实际 selected FB 与旧源访问区间。`single_boundary_retire=true` 不属于本轮证据。
2. 无 bounce/单次路径仍在 V2 适配器支持范围外；新增 Host 回归固定非 bounce 被拒绝，不通过配置开关偷偷扩大支持。
3. 未受控屏蔽 EOF，也未成功诱发 MIPI underrun。MIPI 原生 underrun 仅日志，生产 FAULT 转发及恢复语义仍需独立设计/故障注入，不标记完成。
4. 未做 cache-disabled/Flash 擦写、跨 IDF 版本、任意 DMA 预取深度、长期竞态穷举或光学无撕裂验证。记录器本身会扰动时序。

## 本地回归与采集完整性

完整 Host/LVGL **31/31**；ASan/UBSan + `ASAN_OPTIONS=detect_leaks=1` **31/31**，泄漏检测开启。包含 15 个 Python 分析器用例及 C 多线程记录器测试。S3/P4 专项均离线编译通过。

采集开发中遇到的记录缓冲溢出、USB 日志缺块，以及早期压力试跑出现的 idle watchdog 均被判为 INCOMPLETE 或无效，不纳入通过数据。最终记录器为内部 RAM 8192 项（S3 192 KiB，仅测试使用），导出节流，压力在导出前暂停。较早已通过轮次使用较小容量，准确源码/配置随每轮固件归档。

机器可读结果见 [results.json](results.json)；每例计数、确认、释放及间隔均从原始日志计算。原始日志、实际固件/ELF、sdkconfig、被埋点源码、SHA256 和注入清单保存在本目录被忽略的 `artifacts/20261102/`，未将大文件加入 Git。测试前的 Flash 备份保留在本机 `/tmp/pv2-boundary-{s3,p4}-before.bin`，不加入仓库。

S3/P4 均已回刷测试前 Flash 备份，写入哈希校验通过并复位；未将设备留在专项完成后删除 panel 的黑屏状态。
