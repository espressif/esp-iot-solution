# 兼容范围与迁移入口

当前组件由公开 Presenter、资源装配和唯一内部 engine 组成；LVGL 通过相邻
`esp_lv_present` 接入。不再要求应用引入独立 V2 组件。

- 主要 `esp_display_presenter_*` 调用入口保留；内部 `src/` 接口不承诺兼容。
- 原公开 geometry 头及部分类型已收为内部，依赖它们的应用需要迁移，不能宣称完整源码或 ABI 兼容。
- RGB/MIPI 按产品边界仅支持多 FB scanout，NONE 和单 FB 已删除；AUTO 按实际池选双/三缓冲 PARTIAL。
- Panel IO 保留 NONE 和 TE；TE 效果依赖真实面板时序，运行时旋转与存储限制仍须按当前合同检查。
- 超时和 cancel 不证明硬件停止；迟到完成只推进安全退休，不清除原故障。

当前行为以 [模式与缓冲合同](docs/MODE_ARCHITECTURE.md)、
[TE 时序](docs/TE_TIMING.md) 和 [生命周期规则](STATE_MACHINE.md) 为准。
构建/测试入口见 [开发说明](dev/README.md)，板测来源见
[最新五板记录](dev/evidence/display-latest-20261111/README.md)。

迁移阶段的 C1/C2、旧目录布局、旧默认模式和阶段性缺口保存在
[历史兼容记录 (archive)](dev/ARCHIVE.md)，不作为当前 API 清单。
源码迁移不代表 Registry 发布或全部外部应用兼容性已经验收。
