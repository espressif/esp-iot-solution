ESP LV FS
=============
:link_to_translation:`en:[English]`

`esp_lv_fs` 允许 LVGL 使用文件系统来访问资源。该组件依赖 `esp_mmap_assets`，以高效管理文件操作，并支持多个分区。


功能
-----------------------

- 基于 `esp_mmap_assets` 用于文件系统创建。

- 支持标准文件操作：fopen, fclose, fread, ftell, fseek。

- 使用 `esp_partition_read` API 实现高效文件访问。

- 支持多个分区。


依赖
-----------------------

依赖组件 `esp_mmap_assets <esp_mmap_assets.html>`__ 。 它提供文件偏移索引关系。


应用示例
---------------------

注册文件系统
^^^^^^^^^^^^^^^^^^^

在 LVGL 启动后注册文件系统。

.. code:: c

   #include "esp_lv_fs.h"
   #include "esp_mmap_assets.h"
   
   esp_lv_fs_handle_t fs_drive_a_handle;
   mmap_assets_handle_t mmap_drive_a_handle;
   
   const mmap_assets_config_t asset_cfg = {
      .partition_label = "assets_A",
      .max_files = MMAP_DRIVE_A_FILES,
      .checksum = MMAP_DRIVE_A_CHECKSUM,
      .flags = {
         .mmap_enable = true,
      }
   };
   mmap_assets_new(&asset_cfg, &mmap_drive_a_handle);

   const fs_cfg_t fs_drive_a_cfg = {
      .fs_letter = 'A',
      .fs_assets = mmap_drive_a_handle,
      .fs_nums = MMAP_DRIVE_A_FILES
   };
   esp_lv_fs_desc_init(&fs_drive_a_cfg, &fs_drive_a_handle);


API 参考
-----------------

.. include-build-file:: inc/esp_lv_fs.inc