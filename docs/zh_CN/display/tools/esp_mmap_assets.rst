ESP MMAP ASSETS
================
:link_to_translation:`en:[English]`

该模块主要用于打包资源（如图像、字体等），并将其直接映射以供用户访问。

功能
-----------

**添加导入文件类型**
   - 支持多种文件格式，如 ``.bin``、``.jpg``、``.ttf`` 等。

**启用分片 JPG**
   - 需要使用 ``SJPG`` 来解析。参见 LVGL `SJPG <https://docs.lvgl.io/8.4/libs/sjpg.html>`__。

**启用分片 PNG**
   - 需要使用 ``esp_lv_spng`` 来解析。参见组件 `esp_lv_spng <esp_lv_spng.html>`__。

**设置分片高度**
   - 设置分片高度，依赖于 ``MMAP_SUPPORT_SJPG`` 或 ``MMAP_SUPPORT_SPNG``。

CMake
---------
用户可以选择将图像与应用程序二进制文件、分区表等一起自动刷写到设备上，通过在 idf.py flash 时指定 FLASH_IN_PROJECT。例如：

.. code:: c

   /* partitions.csv
    * --------------------------------------------------------
    * | Name               | Type | SubType | Offset | Size  | Flags     |
    * --------------------------------------------------------
    * | my_spiffs_partition | data | spiffs  |        | 6000K |           |
    * --------------------------------------------------------
    */
    spiffs_create_partition_assets(my_spiffs_partition my_folder FLASH_IN_PROJECT)


应用示例
---------------------

生成头文件 (assets_generate.h)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
该头文件自动生成，包含内存映射资源的基本定义。

.. code:: c

   #include "esp_mmap_assets.h"

   #define TOTAL_MMAP_FILES      2
   #define MMAP_CHECKSUM         0xB043

   enum MMAP_FILES {
      MMAP_JPG_JPG = 0,   /*!< jpg.jpg */
      MMAP_PNG_PNG = 1,   /*!< png.png */
   };

创建资源句柄
^^^^^^^^^^^^^^^^^^^^^
资源初始化配置确保与 ``assets_generate.h`` 一致。它设置了 ``max_files`` 和 ``checksum``，用来验证头文件和内存映射的二进制文件是否匹配。

.. code:: c

   mmap_assets_handle_t asset_handle;

   const mmap_assets_config_t config = {
      .partition_label = "my_spiffs_partition",
      .max_files = TOTAL_MMAP_FILES,
      .checksum = MMAP_CHECKSUM,
   };

   ESP_ERROR_CHECK(mmap_assets_new(&config, &asset_handle));

资源使用
^^^^^^^^^^^^^^^^^^^^^
可以使用 ``assets_generate.h`` 中定义的枚举来获取资源信息。

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
