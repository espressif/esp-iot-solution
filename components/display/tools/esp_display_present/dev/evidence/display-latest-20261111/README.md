# 五板最终显示对比 — 2026-11-11

[打开网页](comparison.html) · [CSV](comparison.csv) · [原始汇总](matrix-data.json)

五板顶部Tab：S3 RGB480×480、S31 Korvo RGB800×480、P4 MIPI DSI1024×600、S31 MOSAICO QSPI TE480×480、ESP-BOX-3 SPI320×240。四角度帧率并排，内存按模式及剩余/已用选择，场景与诊断用抽屉展开。剩余内存越多越好，差值放present单元格中。

当前有效性能/内存181项（present108、adapter73），完整独立驱动观察132项，共5008个场景；当前无未完成项。历史失败现场仍归档，不混入当前成绩。不是五板全部重复烧录：已核对有效项复用，本轮新增48项性能、24项观察。

- RGB/MIPI仅XIP ON，不含NONE；S3开bounce，S31 RGB关闭bounce。
- P4：PPA修复后的原5项保留，本轮剩余19项完整通过；24项独立驱动观察全部重新完成。普通帧率来自干净IDF，隔离观察补丁不计入性能基线。
- TE：present16项和adapter9项都带同样样式的顶部模式标签。C/T与SYNC_FULL/ASYNC_FULL/ASYNC_BOUNCE分别列出，不强行定义等价，不计算不同布局差值。USB早期中断片段排除，最后完整9项另有原始日志证明。
- SPI：present20项已更新为当前版本；adapter四项0°配置及源码核对后复用，不支持的软件旋转和AUTO记−。GRAM允许撕裂。

内存展示打开前、打开后、运行最小剩余、关闭后四点。静态SRAM另取普通测试固件ELF与公共启动堆，包含TE adapter本次实际ELF；不从剩余堆倒推、不假定扣除JPEG，不冒充组件独占占用。各行按来源展示实测bytes。P4的19项与此前5项构建case列表不同，公共PSRAM总量相差512B，未人为修正。

串口通过与地址观察不能证明无脏块或撕裂。测试结束未恢复原固件。详细测试条件、USB中断及首次内存保留见相邻display-final-supplement-20261111原始归档。

复现网页（无硬件操作，在esp_display_present目录执行）：

```sh
python dev/tools/build_latest_dashboard.py --output /tmp/display-latest/matrix-data.json
python dev/tools/render_comparison.py --data /tmp/display-latest/matrix-data.json --output /tmp/display-latest/comparison.html
```

历史来源路径指向 [开发快照](../../ARCHIVE.md)；重建网页只读取本目录最终 JSON。后续源码修改未追记为本轮已测。
