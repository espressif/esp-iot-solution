ESP MMAP ASSETS
================
:link_to_translation:`en:[English]`

该模块主要用于打包资源（如图像、字体等），将其直接映射以供用户访问。同时，它内部也集成了丰富的图像预处理功能。

功能
-----------

**添加导入文件类型**
   - 支持多种文件格式，如 ``.bin``、``.jpg``、``.ttf`` 等。

**多种访问模式**
   - **分区模式**: 从 ESP32 分区访问资源（默认）
   - **内存映射模式**: 直接内存访问，性能最佳
   - **文件系统模式**: 从文件系统访问资源，用于开发和测试

**启用分片 JPG**
   - 需要使用 ``SJPG`` 来解析。参见 LVGL `SJPG <https://docs.lvgl.io/8.4/libs/sjpg.html>`__。

**启用分片 PNG**
   - 需要使用 ``esp_lv_decoder`` 来解析。参见组件 `esp_lv_decoder <esp_lv_decoder.html>`__。

**设置分片高度**
   - 当开启 split 图片（ ``MMAP_SUPPORT_SJPG`` 、 ``MMAP_SUPPORT_SPNG`` 或 ``MMAP_SUPPORT_SQOI``）处理功能，可以灵活设置图片分割的高度。

**支持 QOI**
   - 支持将图像从 JPG、PNG 格式转换为 QOI。
   - 支持 split qoi，需要使用 ``esp_lv_decoder`` 来解析。

**支持多分区**
   - 使用 spiffs_create_partition_assets 挂载分区时，支持挂载多个分区，且为每个分区设置不同的处理逻辑。

**支持 LVGL Image Converter**
   - 通过开启 ``MMAP_SUPPORT_RAW`` 以及相关配置，可以将 JPG、PNG 转换为 ``Bin``， 以便在 ``LVGL`` 中使用它们。

CMake 选项
------------------
支持以下选项。这些选项允许您配置图像处理的各个方面。

.. code:: cmake

   set(options
       FLASH_IN_PROJECT,
       FLASH_APPEND_APP,
       MMAP_SUPPORT_SJPG,
       MMAP_SUPPORT_SPNG,
       MMAP_SUPPORT_QOI,
       MMAP_SUPPORT_SQOI,
       MMAP_SUPPORT_RAW,
       MMAP_RAW_DITHER,
       MMAP_RAW_BGR_MODE)

   set(one_value_args
       MMAP_FILE_SUPPORT_FORMAT,
       MMAP_SPLIT_HEIGHT,
       MMAP_RAW_FILE_FORMAT,
       IMPORT_INC_PATH,
       COPY_PREBUILT_BIN)

选项说明
~~~~~~~~~~~~~~~~~~~~

通用选项
^^^^^^^^^^^^^^^^^^^^

   - ``FLASH_IN_PROJECT``: 用户可以选择在 ``idf.py flash`` 时自动将图像与应用程序二进制文件、分区表等一起烧录。
   
   - ``FLASH_APPEND_APP``: 启用将二进制数据（``bin``）附加到应用程序二进制文件（``app_bin``）。

   - ``IMPORT_INC_PATH``: 指定生成头文件的目标路径。默认与引用组件位置相同。
   
   - ``COPY_PREBUILT_BIN``: 复制预生成的二进制文件到目标目录。此选项允许您使用外部生成的资产二进制文件，而不是从源文件构建。
   
   - ``MMAP_FILE_SUPPORT_FORMAT``: 指定支持的文件格式（例如，``.png``、``.jpg``、``.ttf``）。
   
   - ``MMAP_SPLIT_HEIGHT``: 定义图像分割的高度以减少内存使用。依赖于：

      - ``MMAP_SUPPORT_SJPG``
      - ``MMAP_SUPPORT_SPNG``
      - ``MMAP_SUPPORT_SQOI``

通用示例
""""""""""""""""

.. code:: c

   spiffs_create_partition_assets(
      my_spiffs_partition
      my_folder
      FLASH_IN_PROJECT
      MMAP_FILE_SUPPORT_FORMAT ".jpg,.png,.ttf"
   )

预构建二进制资源示例
"""""""""""""""""""""""""""""""

.. code:: c

   spiffs_create_partition_assets(
      my_spiffs_partition
      "${ASSETS_DIR}"
      FLASH_IN_PROJECT
      COPY_PREBUILT_BIN "${ASSETS_DIR}/prebuilt.bin"
   )

支持的图像格式
^^^^^^^^^^^^^^^^^^^^

   - ``MMAP_SUPPORT_SJPG``: 启用对 SJPG 格式的支持。
   - ``MMAP_SUPPORT_SPNG``: 启用对 SPNG 格式的支持。
   - ``MMAP_SUPPORT_QOI``: 启用对 QOI 格式的支持。
   - ``MMAP_SUPPORT_SQOI``: 启用对 SQOI 格式的支持。依赖于：

      - ``MMAP_SUPPORT_QOI``

图像分割示例
""""""""""""""""

.. code:: c

   spiffs_create_partition_assets(
      my_spiffs_partition
      my_folder
      FLASH_IN_PROJECT
      MMAP_FILE_SUPPORT_FORMAT ".jpg"
      MMAP_SUPPORT_SJPG
      MMAP_SPLIT_HEIGHT 16
   )

LVGL Bin 支持
^^^^^^^^^^^^^^^^^^^^

   - ``MMAP_SUPPORT_RAW``: 将图像转换为 LVGL 支持的 **Binary** 数据。
      
      **参考:**
         - LVGL v8: `详细参考 <https://github.com/W-Mai/lvgl_image_converter>`__
         - LVGL v9: `详细参考 <https://github.com/lvgl/lvgl/blob/master/scripts/LVGLImage.py>`__

   - ``MMAP_RAW_FILE_FORMAT``: 指定 RAW 图像的文件格式。

      - LVGL v8: ``{true_color, true_color_alpha, true_color_chroma, indexed_1, indexed_2, indexed_4, indexed_8, alpha_1, alpha_2, alpha_4, alpha_8, raw, raw_alpha, raw_chroma}``
      - LVGL v9: 未使用。

   - ``MMAP_RAW_COLOR_FORMAT``: 指定 RAW 图像的颜色格式。

      - LVGL v8: ``{RGB332, RGB565, RGB565SWAP, RGB888}``
      - LVGL v9: ``{L8, I1, I2, I4, I8, A1, A2, A4, A8, ARGB8888, XRGB8888, RGB565, RGB565A8, ARGB8565, RGB888, AUTO, RAW, RAW_ALPHA}``

   - ``MMAP_RAW_DITHER``: 启用 RAW 图像的 **抖动**。

      - LVGL v8: 需要抖动。
      - LVGL v9: 未使用。

   - ``MMAP_RAW_BGR_MODE``: 启用 RAW 图像的 **BGR 模式**。

      - LVGL v8: 未使用。
      - LVGL v9: 未使用。

LVGL v9 示例
""""""""""""""""

.. code:: c

   spiffs_create_partition_assets(
       .........
       MMAP_FILE_SUPPORT_FORMAT ".png"
       MMAP_SUPPORT_RAW
       MMAP_RAW_COLOR_FORMAT "ARGB8888"
   )

LVGL v8 示例
""""""""""""""""

.. code:: c

   spiffs_create_partition_assets(
       .........
       MMAP_FILE_SUPPORT_FORMAT ".png"
       MMAP_SUPPORT_RAW
       MMAP_RAW_FILE_FORMAT "true_color_alpha"
       MMAP_RAW_COLOR_FORMAT "RGB565SWAP"
   )

应用示例
----------

生成头文件 (mmap_generate_my_spiffs_partition.h)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
该头文件自动生成，包含内存映射资源的基本定义。

.. code:: c

   #include "mmap_generate_my_spiffs_partition.h"

   #define TOTAL_MMAP_FILES      2
   #define MMAP_CHECKSUM         0xB043

   enum MMAP_FILES {
      MMAP_JPG_JPG = 0,   /*!< jpg.jpg */
      MMAP_PNG_PNG = 1,   /*!< png.png */
   };

创建资源句柄
~~~~~~~~~~~~~~
资源初始化配置确保与 ``mmap_generate_my_spiffs_partition.h`` 一致。它设置了 ``max_files`` 和 ``checksum``，用来验证头文件和内存映射的二进制文件是否匹配，当然你也可以跳过此检验。


分区模式（默认）
^^^^^^^^^^^^^^^^^^^^

.. code:: c

   mmap_assets_handle_t asset_handle;

   const mmap_assets_config_t config = {
      .partition_label = "my_spiffs_partition",
      .max_files = TOTAL_MMAP_FILES,
      .checksum = MMAP_CHECKSUM,
      .flags = {
         .mmap_enable = false,  // 使用分区模式
         .use_fs = false,       // 不使用文件系统
         .app_bin_check = true,
      }
   };

   ESP_ERROR_CHECK(mmap_assets_new(&config, &asset_handle));

内存映射模式
^^^^^^^^^^^^^^^^^^^^

.. code:: c

   const mmap_assets_config_t config = {
      .partition_label = "my_spiffs_partition",
      .max_files = TOTAL_MMAP_FILES,
      .checksum = MMAP_CHECKSUM,
      .flags = {
         .mmap_enable = true,   // 启用内存映射
         .use_fs = false,       // 不使用文件系统
         .app_bin_check = true,
      }
   };

   ESP_ERROR_CHECK(mmap_assets_new(&config, &asset_handle));

文件系统模式
^^^^^^^^^^^^^^^^^^^^

.. code:: c

   const mmap_assets_config_t config = {
      .partition_label = "/spiffs/assets.bin",  // 文件路径而不是分区名称
      .max_files = TOTAL_MMAP_FILES,
      .checksum = MMAP_CHECKSUM,
      .flags = {
         .mmap_enable = false,  // 禁用内存映射
         .use_fs = true,        // 使用文件系统
         .app_bin_check = true,
      }
   };

   ESP_ERROR_CHECK(mmap_assets_new(&config, &asset_handle));

资源使用
~~~~~~~~~
可以使用 ``mmap_generate_my_spiffs_partition.h`` 中定义的枚举来获取资源信息。

.. code:: c

    const char *name = mmap_assets_get_name(asset_handle, MMAP_JPG_JPG);
    const void *mem = mmap_assets_get_mem(asset_handle, MMAP_JPG_JPG);
    int size = mmap_assets_get_size(asset_handle, MMAP_JPG_JPG);
    int width = mmap_assets_get_width(asset_handle, MMAP_JPG_JPG);
    int height = mmap_assets_get_height(asset_handle, MMAP_JPG_JPG);

    ESP_LOGI(TAG, "Name:[%s], Mem:[%p], Size:[%d bytes], Width:[%d px], Height:[%d px]", name, mem, size, width, height);

API 参考
~~~~~~~~~~

.. include-build-file:: inc/esp_mmap_assets.inc
