# ESP LV Present

[![组件服务](https://components.espressif.com/components/espressif/esp_lv_present/badge.svg)](https://components.espressif.com/components/espressif/esp_lv_present)

[English](README.md) | 中文

`esp_lv_present` 是 [`esp_display_present`](../esp_display_present/README_CN.md)
的 **LVGL 9 flush 挂钩**。应用自己持有 `esp_lcd` 面板和 Presenter；本组件只把
LVGL display 接到这个 Presenter 上。

它不是第二套 display 方案，也不是
[`esp_lvgl_adapter`](../esp_lvgl_adapter/README_CN.md) 的替代品。

```text
LVGL 9 控件
      │  flush / 脏区
esp_lv_present          ← 本组件（仅 LVGL 9）
      │  Presenter 契约（PARTITION / DIRECT / FULL）
esp_display_present     ← 缓冲区、TE、旋转、送屏
      │
esp_lcd 面板
```

名字里的 `present` 对应 `esp_display_present`；`display` 留在送屏组件上，避免
再被看成一套独立的 display / present 引擎。

## 依赖

- **ESP-IDF**：>= 6.0
- **LVGL**：仅 9.x（`>=9,<10`），不支持 LVGL 8
- **esp_display_present**：必须先由应用创建 Presenter

## 和 `esp_lvgl_adapter` 的区别

| | `esp_lv_present` | `esp_lvgl_adapter` |
|---|---|---|
| 职责 | 把 LVGL 接到已有的 `esp_display_present` | 完整 LVGL 适配：显示、输入、锁，以及可选 FS / 解码 / FreeType |
| LVGL | 仅 9.x | 8.x 和 9.x |
| LCD 初始化 | 应用负责（`esp_lcd` + Presenter） | 应用负责 `esp_lcd`；adapter 自己管 LVGL 显示缓冲 |
| LVGL 任务 / 锁 | 无。调用 `esp_lv_present_start()` 的任务必须自己跑 `lv_timer_handler()` | 工作任务 + `esp_lv_adapter_lock()` / `unlock()` |
| 触摸 / 编码器 | 不包含 | 包含 |
| 适用场景 | 走 Presenter 送屏、GSP/LVGL 切换、present 模式对比 | 普通 LVGL 界面，不使用 `esp_display_present` |

同一块面板不要同时走两套。adapter 和本组件都会认为自己拥有 LVGL display 和刷新路径。

## 如何和 `esp_display_present` 搭配

1. 初始化 `esp_lcd` 面板（例如用 `hw_init`）。
2. 填写 `esp_display_present_target_config_t`，调用
   `esp_display_presenter_create()`。模式一般用 `AUTO` 即可；契约和 TE 见
   [Presenter README](../esp_display_present/README_CN.md)。
3. 在即将泵 LVGL 的任务里调用 `esp_lv_present_start(presenter, &disp)`。挂钩会
   把 Presenter 的 producer 绑到该任务。
4. 在 `disp` 上创建控件，并在**同一任务**里调用 `lv_timer_handler()`。
5. 把 Presenter 交给其他生产者（例如 GSP）之前：先 quiesce，再
   `esp_lv_present_stop()`。`stop()` 不会删除 Presenter。

```c
#include "esp_display_present.h"
#include "esp_lv_present.h"

esp_display_presenter_t *presenter = NULL;
ESP_ERROR_CHECK(esp_display_presenter_create(&presenter_config, &presenter));

lv_display_t *disp = NULL;
ESP_ERROR_CHECK(esp_lv_present_start(presenter, &disp));

/* 创建 LVGL 界面，并在同一任务中泵 LVGL。 */
for (;;) {
    uint32_t delay_ms = lv_timer_handler();
    vTaskDelay(pdMS_TO_TICKS(delay_ms > 0 ? delay_ms : 1));
}

ESP_ERROR_CHECK(esp_display_presenter_quiesce(presenter, 2000));
ESP_ERROR_CHECK(esp_lv_present_stop());
ESP_ERROR_CHECK(esp_display_presenter_delete(presenter));
```

同一时刻只能有一个挂钩。第一次成功的 `start()` 会初始化 LVGL 并安装
`esp_timer` tick。

契约：

- **PARTITION**：LVGL 借用 Presenter 的绘制缓冲池；LVGL 最终脏区作为帧缓冲
  repair 覆盖范围。
- **DIRECT / FULL**：LVGL 直接画进 Presenter 租出的帧缓冲。本组件不再另分配
  一整块全屏缓冲。

## 添加到工程

```
idf.py add-dependency "espressif/esp_lv_present"
```

或在 `idf_component.yml` 中：

```yml
dependencies:
  espressif/esp_lv_present: "^0.1.1"
```

本仓库内使用：

```yml
dependencies:
  espressif/esp_lv_present:
    override_path: "components/display/tools/esp_lv_present"
```

`esp_display_present` 和 LVGL 9 会随依赖自动拉入。

## 示例和 test app

- 示例：`examples/display/gui/lvgl_present_benchmark` — 在应用持有的 Presenter
  上跑 LVGL 9 benchmark。
- 示例：`examples/display/gui/gsp_lvgl_present_handoff` — GSP ↔ LVGL Presenter
  所有权切换。
- Test app：`test_apps/` — Unity 检查 start/stop 以及 flush。使用软件 stub 面板，
  不需要 LCD，形式与 `esp_lv_decoder` / `esp_lv_fs` 相同。

```sh
cd components/display/tools/esp_lv_present/test_apps
idf.py set-target esp32c3 build flash monitor
```

## 参见

- [`esp_display_present`](../esp_display_present/README_CN.md) — Presenter 与送屏
- [`esp_lvgl_adapter`](../esp_lvgl_adapter/README_CN.md) — 不走本 Presenter 的完整 LVGL 适配
