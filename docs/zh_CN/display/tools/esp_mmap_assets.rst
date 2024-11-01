ESP MMAP ASSETS
================
:link_to_translation:`en:[English]`

该模块主要用于打包资源（如图像、字体等），将其直接映射以供用户访问。同时，它内部也集成了丰富的图像预处理功能。

功能
-----------

**添加导入文件类型**
   - 支持多种文件格式，如 ``.bin``、``.jpg``、``.ttf`` 等。

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

CMake options
------------------
用户可以选择将图像与应用程序二进制文件、分区表等一起自动刷写到设备上，通过在 idf.py flash 时指定 FLASH_IN_PROJECT。例如：

.. code:: c

   /* partitions.csv
    * -----------------------------------------------------------------------
    * | Name                | Type | SubType | Offset | Size  | Flags     |
    * --------------------------------------------------------
    * | my_spiffs_partition | data | spiffs  |        | 6000K |           |
    * -----------------------------------------------------------------------
    */
    spiffs_create_partition_assets(
        my_spiffs_partition
        my_folder
        FLASH_IN_PROJECT
        MMAP_FILE_SUPPORT_FORMAT ".png")

组件还支持以下选项，这些选项允许您在编译时，启用对图像的各种预处理。

.. code:: c

   set(options FLASH_IN_PROJECT,           // Defines storage type (flash in project)
                MMAP_SUPPORT_SJPG,         // Enable support for SJPG format
                MMAP_SUPPORT_SPNG,         // Enable support for SPNG format
                MMAP_SUPPORT_QOI,          // Enable support for QOI format
                MMAP_SUPPORT_SQOI,         // Enable support for SQOI format
                MMAP_SUPPORT_RAW,          // Enable support for RAW format (LVGL conversion only)
                MMAP_RAW_DITHER,           // Enable dithering for RAW images (LVGL conversion only)
                MMAP_RAW_BGR_MODE)         // Enable BGR mode for RAW images (LVGL conversion only)

    set(one_value_args MMAP_FILE_SUPPORT_FORMAT,    // Specify supported file format (e.g., .png, .jpg)
                   MMAP_SPLIT_HEIGHT,               // Define the height for image splitting
                   MMAP_RAW_FILE_FORMAT)            // Specify the file format for RAW images (LVGL conversion only)

应用示例
---------------------

生成头文件 (mmap_generate_my_spiffs_partition.h)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
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
^^^^^^^^^^^^^^^^^^^^^
资源初始化配置确保与 ``mmap_generate_my_spiffs_partition.h`` 一致。它设置了 ``max_files`` 和 ``checksum``，用来验证头文件和内存映射的二进制文件是否匹配，当然你也可以跳过此检验。

.. code:: c

   mmap_assets_handle_t asset_handle;

   const mmap_assets_config_t config = {
      .partition_label = "my_spiffs_partition",
      .max_files = TOTAL_MMAP_FILES,
      .checksum = MMAP_CHECKSUM,
      .flags = {
            .mmap_enable = true,
            .app_bin_check = true,
        },
   };

   ESP_ERROR_CHECK(mmap_assets_new(&config, &asset_handle));

资源使用
^^^^^^^^^^^^^^^^^^^^^
可以使用 ``mmap_generate_my_spiffs_partition.h`` 中定义的枚举来获取资源信息。

.. code:: c

    const char *name = mmap_assets_get_name(asset_handle, MMAP_JPG_JPG);
    const void *mem = mmap_assets_get_mem(asset_handle, MMAP_JPG_JPG);
    int size = mmap_assets_get_size(asset_handle, MMAP_JPG_JPG);
    int width = mmap_assets_get_width(asset_handle, MMAP_JPG_JPG);
    int height = mmap_assets_get_height(asset_handle, MMAP_JPG_JPG);

    ESP_LOGI(TAG, "Name:[%s], Mem:[%p], Size:[%d bytes], Width:[%d px], Height:[%d px]", name, mem, size, width, height);

API 参考
-----------------

.. include-build-file:: inc/esp_mmap_assets.inc
