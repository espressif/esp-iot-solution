ESP Display Present
===================
:link_to_translation:`en:[English]`

``esp_display_present`` 用于连接渲染器生成的像素表面与 ``esp_lcd`` 面板，不依赖
场景、控件或输入语义。送屏路径从 GUI 中抽出，LVGL、GSP 或其他自绘 GUI 可以共用
同一套 Presenter。

它不是 ``esp_lvgl_adapter``。LVGL 9 请使用
`ESP LV Present <esp_lv_present.html>`__。

渲染器负责逻辑脏区和像素生成；Presenter 负责物理缓冲区一致性和面板提交。

功能
----

- 校验显示目标并选择合适的呈现策略
- 租用渲染表面或有界绘制缓冲区
- 提交整帧或脏区更新
- 处理帧缓冲区所有权、TE 同步、旋转、字节序、缓存同步和可选硬件拷贝
- 按顺序跟踪完成状态，并在关闭时避免提前释放传输中的缓冲区

添加到工程
----------

.. code:: bash

    idf.py add-dependency "espressif/esp_display_present"

或在 ``idf_component.yml`` 中：

.. code:: yaml

    dependencies:
      espressif/esp_display_present: "*"

公共 API
--------

包含 ``esp_display_present.h`` 即可使用公共 API。配置和共享类型分别定义在
``esp_display_present_config.h`` 和 ``esp_display_present_types.h`` 中。

主要对象为 ``esp_display_presenter_t``。

面板初始化前，使用 ``esp_display_present_get_required_frame_buffer_count()``，
按明确的面板类型、模式、旋转和 TE 配置查询驱动的 ``num_fbs``。
查询与创建共用静态策略，不访问面板句柄。IO/GRAM 返回 0，绘制缓冲和
TE compose 缓冲另行管理。创建面板后填入句柄，用同一目标配置创建 Presenter。
使用驱动自有缓冲时，保持 ``target.fb.frame_buffer_count = 0``。

渲染器随后按以下顺序工作：

1. 使用 ``esp_display_present_target_config_t`` 创建 Presenter
2. 获取帧契约
3. 渲染请求的完整表面或分区
4. 提交逻辑覆盖范围，或取消当前帧
5. 等待生产端工作排空后停止并删除 Presenter

绘制契约：

===========  ===========================================
契约         渲染器职责
===========  ===========================================
PARTITION    将请求区域渲染到 Presenter 持有的绘制缓冲区
DIRECT       将脏区渲染到完整逻辑表面
FULL         重绘完整逻辑表面
===========  ===========================================

呈现模式通常应保持 ``ESP_DISPLAY_PRESENT_MODE_AUTO``。在目标配置中提供准确的
面板类型、Framebuffer、TE、旋转和字节序信息，组件即可选择正确路径。

内部 RAM 充足时，PARTIAL 和 TE 模式会使用第二个绘制缓冲区，使条带拷贝与渲染
重叠。内存紧张的应用可设置 ``drawbuf.buffers = 1`` 保持单个绘制缓冲区。

关屏或拆除面板前，先停止帧生产并调用 ``esp_display_presenter_quiesce()``。
在 quiesce 成功之前必须保持面板 IO 和完成中断有效，然后再关屏并删除 Presenter。

更多说明
--------

参见组件 README：

- `English <https://github.com/espressif/esp-iot-solution/blob/master/components/display/tools/esp_display_present/README.md>`__
- `中文 <https://github.com/espressif/esp-iot-solution/blob/master/components/display/tools/esp_display_present/README_CN.md>`__

LVGL benchmark example 位于 ``examples/display/gui/lvgl_present_benchmark``。
GSP/LVGL 所有权切换 example 位于 ``examples/display/gui/gsp_lvgl_present_handoff``。
Unity test app 位于 ``components/display/tools/esp_display_present/test_apps``。

API 参考
--------

.. include-build-file:: inc/esp_display_present.inc
.. include-build-file:: inc/esp_display_present_config.inc
.. include-build-file:: inc/esp_display_present_types.inc
.. include-build-file:: inc/esp_display_present_geometry.inc
