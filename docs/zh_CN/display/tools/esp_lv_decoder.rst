ESP LV DECODER
================
:link_to_translation:`en:[English]`

允许使用 LVGL 中的图像。除此之外，它还允许使用自定义格式（称为分割图像），该格式可以在嵌入式系统上以更优化的方式进行解码。

需要 ESP-IDF >= 5.3 和 LVGL >= 8.3、< 10。

分割图像，参考 `SJPG <https://docs.lvgl.io/8.4/libs/sjpg.html>`__ 的实现。


功能
-------

- 支持标准和自定义分割图像格式，包括 JPG、PNG 和 `QOI <https://github.com/phoboslab/qoi>`__。

- 解码标准图像需要与完整未压缩图像大小相当的 RAM，建议用于具有更多 RAM 的设备。

- 分割图像是基于标准图像格式的自定义格式，专为 LVGL 设计。

- 文件存储和 C 数组均实现了文件读取。

- 分割图像分段解码，因此不支持缩放和旋转。


设置图片转换格式
-------------------

依赖组件 `esp_mmap_assets <esp_mmap_assets.html>`__ 。 它会在编译过程中自动打包图像并将其转换为所需的格式。

JPG 转换成 SJPG
^^^^^^^^^^^^^^^^^^^^^^^^
.. code:: c

   spiffs_create_partition_assets(
        my_spiffs_partition
        my_folder
        FLASH_IN_PROJECT
        MMAP_FILE_SUPPORT_FORMAT ".jpg"
        MMAP_SUPPORT_SJPG
        MMAP_SPLIT_HEIGHT 16)

PNG 转换成 SPNG
^^^^^^^^^^^^^^^^^^^^^^^^
.. code:: c

   spiffs_create_partition_assets(
        my_spiffs_partition
        my_folder
        FLASH_IN_PROJECT
        MMAP_FILE_SUPPORT_FORMAT ".png"
        MMAP_SUPPORT_SPNG
        MMAP_SPLIT_HEIGHT 16)

PNG、JPG 转换成 QOI
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
.. code:: c

   spiffs_create_partition_assets(
        my_spiffs_partition
        my_folder
        FLASH_IN_PROJECT
        MMAP_FILE_SUPPORT_FORMAT ".jpg,.png"
        MMAP_SUPPORT_QOI)

PNG、JPG 转换成 SQOI
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
.. code:: c

   spiffs_create_partition_assets(
        my_spiffs_partition
        my_folder
        FLASH_IN_PROJECT
        MMAP_FILE_SUPPORT_FORMAT ".jpg,.png"
        MMAP_SUPPORT_QOI
        MMAP_SUPPORT_SQOI
        MMAP_SPLIT_HEIGHT 16)

PNG 转换成 PJPG
^^^^^^^^^^^^^^^^^^^^^^^

PJPG 将颜色和透明度分别存为 JPEG，属于有损格式。通过 ``MMAP_SUPPORT_PJPG`` 转换：

.. code:: cmake

   spiffs_create_partition_assets(
        my_spiffs_partition
        my_folder
        FLASH_IN_PROJECT
        MMAP_FILE_SUPPORT_FORMAT ".png"
        MMAP_SUPPORT_PJPG)

- 硬件 JPEG 不可用或未启用时，在本组件和 ``esp_new_jpeg`` 均支持的目标上使用软件解码。
- 可定义 ``ESP_LV_ENABLE_HW_JPEG=0`` 强制软解；PJPG 硬解失败会返回错误，不自动重试软解。
- 默认输出 RGB565A8，图像内存约为 ``宽 × 高 × 3`` 字节，另需工作缓冲区。
- LVGL v8 需配置 ``LV_COLOR_DEPTH=16``。
- 转换尺寸补齐至 16 的倍数；任一边小于 128 像素或透明度复杂的图片可能保留为 PNG。

应用示例
---------------------

注册解码器
^^^^^^^^^^^^^^^^^^^

在 LVGL 初始化后注册；使用 ``esp_lvgl_adapter`` 且启用 ``CONFIG_ESP_LVGL_ADAPTER_ENABLE_DECODER`` 时自动注册。

.. code:: c

   esp_lv_decoder_handle_t decoder_handle = NULL;
   esp_lv_decoder_init(&decoder_handle);

文件源需先挂载 LVGL 文件系统，也可使用内存图片描述符。


API 参考
-----------------

.. include-build-file:: inc/esp_lv_decoder.inc
