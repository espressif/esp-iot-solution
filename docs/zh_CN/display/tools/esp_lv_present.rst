ESP LV Present
==============
:link_to_translation:`en:[English]`

``esp_lv_present`` 是 ``esp_display_present`` 的 LVGL 9 flush 挂钩。应用持有
``esp_lcd`` 面板和 Presenter；本组件只把 LVGL display 接到这个 Presenter 上。

它不是第二套 display 方案。``display`` 留在 ``esp_display_present`` 上，本组件
名字表示 LVGL 这一侧。

依赖
----

- ESP-IDF >= 6.0
- 仅 LVGL 9.x（不支持 LVGL 8）
- 应用已创建好的 ``esp_display_presenter_t``

关系
----

::

    LVGL 9 控件
          │
    esp_lv_present           （本组件，仅 LVGL 9）
          │
    esp_display_present      （缓冲区、TE、旋转、送屏）
          │
    esp_lcd 面板

和 ``esp_lvgl_adapter`` 的区别
------------------------------

``esp_lvgl_adapter`` 是完整 LVGL 适配（v8 和 v9）：显示注册、工作任务、锁、
触摸/编码器，以及可选 FS / 解码 / FreeType。界面不走
``esp_display_present`` 时用它。

``esp_lv_present`` 只在已有 Presenter 上安装 LVGL 9 的 flush 路径。调用
``start()`` 的任务必须自己跑 ``lv_timer_handler()``。同一块面板不要同时走两套。

如何搭配 Presenter
------------------

1. 初始化面板（例如 ``hw_init``）。
2. 创建 Presenter（模式一般用 ``AUTO``）。
3. 在即将泵 LVGL 的任务里调用 ``esp_lv_present_start()``。
4. 把 Presenter 交给其他生产者之前：先 quiesce，再
   ``esp_lv_present_stop()``。Presenter 不会被删除。

.. code:: yaml

    dependencies:
      espressif/esp_lv_present: "^0.1.0"

.. code:: c

   #include "esp_lv_present.h"

   lv_display_t *display = NULL;
   ESP_ERROR_CHECK(esp_lv_present_start(presenter, &display));

更多说明
--------

参见组件 README：

- `English <https://github.com/espressif/esp-iot-solution/blob/master/components/display/tools/esp_lv_present/README.md>`__
- `中文 <https://github.com/espressif/esp-iot-solution/blob/master/components/display/tools/esp_lv_present/README_CN.md>`__

示例：``examples/display/gui/lvgl_present_benchmark``、
``examples/display/gui/gsp_lvgl_present_handoff``。
Test app：``components/display/tools/esp_lv_present/test_apps``。

另见：`ESP Display Present <esp_display_present.html>`__、
`ESP LVGL Adapter <esp_lvgl_adapter.html>`__。

API 参考
--------

.. include-build-file:: inc/esp_lv_present.inc
