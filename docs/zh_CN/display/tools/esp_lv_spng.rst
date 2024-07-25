ESP LV SPNG
=============
:link_to_translation:`en:[English]`

允许在 LVGL 中使用 PNG 图像。此外，还支持一种自定义格式，称为分片 PNG（SPNG），这种格式在嵌入式系统上可以更优化地解码。
参考 `SJPG <https://docs.lvgl.io/8.4/libs/sjpg.html>`__ 的实现。


功能
-----------------------

- 支持标准 PNG 和自定义 SPNG 格式。

- 解码标准 PNG 需要的 RAM 大小是整个未压缩图像的大小（建议在内存较大的设备上使用）。

- SPNG 是基于标准 PNG 的自定义格式，专门为 LVGL 设计。

- SPNG 是一种分片 PNG 格式，由多个小 PNG 片段和一个 SPNG 头部组成。

- SPNG 图像是分段部分解码，因此不支持缩放或旋转。


将 PNG 转换为 SPNG
-----------------------

依赖组件 `esp_mmap_assets <esp_mmap_assets.html>`__ 。 它将在编译过程中自动打包并转换 PNG 图像为 SPNG 格式。

.. code:: c

   [12/1448] Move and Pack assets...
   --support_format: .jpg,.png
   --support_spng: ON
   --support_sjpg: ON
   --split_height: 16
   Input: temp_icon.png    RES: 90 x 90    splits: 6
   Completed, saved as: temp_icon.spng

应用示例
---------------------

注册解码器
^^^^^^^^^^^^^^^^^^^

在 LVGL 启动后注册解码器函数。

.. code:: c

   esp_lv_split_png_init();


API 参考
-----------------

.. include-build-file:: inc/esp_lv_spng.inc