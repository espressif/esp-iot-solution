# ESP Display Present

`esp_display_present` 提供稳定的 `esp_display_presenter_*` 应用接口、平台资源装配和唯一显示引擎；`esp_lv_present` 将 LVGL 接入同一引擎。应用继续持有 panel、IO 和总线资源。

公开应用头位于 `include`；`src` 内头文件均为内部实现，不作为应用 API 导出。当前版本不再公开旧几何头和其中的坐标／尺寸类型；依赖这些接口的应用需单独迁移，不能将其视为完全源码兼容。

本地依赖只需 `esp_display_present`，LVGL 场景再加入 `esp_lv_present`，不再需要 V2 组件。源码合并不代表已发布 Registry 包或确认全部外部兼容。开发和验证入口见 [迁移状态](MIGRATION_STATUS.md)，历史设计见 [交接设计 (archive)](dev/ARCHIVE.md)。

当前模式契约见[模式与 LCD 适配](docs/MODE_ARCHITECTURE.md)：RGB/MIPI 仅保留双／三缓冲防撕裂 scanout；NONE 和单 FB 不支持。AUTO 在创建时按实际驱动池选择 TRIPLE_PARTIAL（三张）或 DOUBLE_PARTIAL（两张），不回退到 NONE。GRAM/TE 的 NONE/AUTO 不变。

LVGL FULL 和原生 0° DIRECT 先取得可写租约再绑定像素，提交／取消前撤销引用，不再分配全屏占位画布；旋转 DIRECT 的实际绘图画布、PARTIAL 的 tile 兜底保留。不要在 LVGL 绑定尚存活时停止或删除 presenter。

`CONFIG_ESP_DISPLAY_PRESENT_FB_IRAM` 及整组对象链接映射已移除；显式 ISR/pixel 的 IRAM_ATTR 保留。当前性能网页及各轮配置、SRAM 口径见[测试入口](dev/README.md)，2026-11-11 的生命周期变更单独验证，不将旧成绩标成新版本。串口完成及交接检查不能单独证明光学无撕裂。

普通内部 tile 的 GRAM 可试用 `CONFIG_ESP_DISPLAY_PRESENT_GRAM_BACKGROUND`，默认关闭。它复用引擎 progress task，将可能阻塞的驱动提交移出引擎 gate，不强制增加像素复制；旋转／PSRAM 路径继续采用原有调度。实验 progress task 优先级由 `CONFIG_ESP_DISPLAY_PRESENT_GRAM_BACKGROUND_PRIORITY` 设置，默认 7。适用条件、RAM 成本和实测收益见 [GRAM 后台提交首版 (archive)](dev/ARCHIVE.md)。

2026-11-06 的 [架构检查与 Host 收敛 (archive)](dev/ARCHIVE.md)修复了公开接口运行时跨轴旋转的尺寸权威冲突：panel 物理尺寸固定，由 backend 更新逻辑宽高，资源层不再修改 scanout 物理视图。旋转后需重新查询 caps。新增了创建失败回滚和固定物理屏幕的 RGB/DSI/GRAM 旋转验证；该修复尚未重新上板。

当前提交的 [三板 LVGL 帧率、Host 测试项与证据](dev/VALIDATION.md) 单独列出测试条件和边界。[历史模式与旋转矩阵 (archive)](dev/ARCHIVE.md)记录了原 P4 跨轴旋转缺陷；后续几何修复和硬件/PPA 回归见 [组件状态](MIGRATION_STATUS.md)，不将旧缺陷直接列为当前未修复问题。S31 GRAM 缺少旋转目标存储时仍可能拒绝运行时旋转。TE 的时序假设见 [TE 说明](docs/TE_TIMING.md)。

测试工程、板级配置、LVGL benchmark 与 adapter 对比统一入口：[dev/README.md](dev/README.md)。
