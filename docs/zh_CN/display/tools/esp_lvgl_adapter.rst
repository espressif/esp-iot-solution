ESP LVGL ADAPTER
================
:link_to_translation:`en:[English]`

``esp_lvgl_adapter`` 为 ESP-IDF 工程提供统一的 LVGL 适配层,涵盖显示注册、防撕裂策略、LVGL 线程安全访问,以及文件系统/图片解码/FreeType 字体的可选集成。兼容 LVGL v8 与 v9。


功能
-------

- **统一的显示注册接口**: 支持 MIPI DSI / RGB / QSPI / SPI / I2C / I80 等多种显示接口
- **防撕裂策略**: 提供多种防撕裂模式
- **线程安全**: 提供 ``lock/unlock`` API,保证多线程环境下的安全访问
- **可选模块** (通过 Kconfig 开关控制):

  - 文件系统桥接 (``esp_lv_fs``)
  - 图片解码 (``esp_lv_decoder``)
  - FreeType 矢量字体
  - FPS 统计、Dummy Draw(不实际输出到显示器)

- **多屏支持**: 可同时管理多个显示设备


添加到工程
-----------

通过组件服务添加
^^^^^^^^^^^^^^^^

.. code:: bash

   idf.py add-dependency "espressif/esp_lvgl_adapter"


在 ``idf_component.yml`` 中声明
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code:: yaml

   dependencies:
     espressif/esp_lvgl_adapter: "*"


系统要求
^^^^^^^^

- **ESP-IDF**: >= 5.5
- **LVGL**: 8.x 或 9.x(自动适配版本)
- **可选依赖**: 根据启用的功能需要满足相应的组件依赖


快速开始
---------

以下是一个最小化的使用示例,展示如何初始化适配器并注册显示设备:

.. code:: c

   #include "esp_lv_adapter.h"  // 包含显示与输入适配接口

   void app_main(void)
   {
       // 步骤 0: 使用 esp_lcd API 创建面板和面板 IO(此处省略具体实现)
       esp_lcd_panel_handle_t panel = /* ... */;
       esp_lcd_panel_io_handle_t panel_io = /* ... 或 NULL */;

       // 步骤 1: 初始化适配器
       esp_lv_adapter_config_t cfg = ESP_LV_ADAPTER_DEFAULT_CONFIG();
       ESP_ERROR_CHECK(esp_lv_adapter_init(&cfg));

       // 步骤 2: 注册显示设备(根据接口类型选择相应的默认配置宏)
       esp_lv_adapter_display_config_t disp_cfg = ESP_LV_ADAPTER_DISPLAY_RGB_DEFAULT_CONFIG(
           panel,           // LCD 面板句柄
           panel_io,        // LCD 面板 IO 句柄(某些接口可为 NULL)
           800,             // 水平分辨率
           480,             // 垂直分辨率
           ESP_LV_ADAPTER_ROTATE_0 // 旋转角度
       );
       lv_display_t *disp = esp_lv_adapter_register_display(&disp_cfg);
       assert(disp != NULL);

       // 步骤 3: (可选) 注册输入设备
       // 使用 esp_lcd_touch API 创建触摸句柄(此处省略具体实现)
       // esp_lcd_touch_handle_t touch_handle = /* ... */;
       // esp_lv_adapter_touch_config_t touch_cfg = ESP_LV_ADAPTER_TOUCH_DEFAULT_CONFIG(disp, touch_handle);
       // lv_indev_t *touch = esp_lv_adapter_register_touch(&touch_cfg);
       // assert(touch != NULL);

       // 步骤 4: 启动适配器任务
       ESP_ERROR_CHECK(esp_lv_adapter_start());

       // 步骤 5: 使用 LVGL API 绘制界面(需要先加锁)
       if (esp_lv_adapter_lock(-1) == ESP_OK) {
           lv_obj_t *label = lv_label_create(lv_scr_act());
           lv_label_set_text(label, "Hello LVGL!");
           lv_obj_center(label);
           esp_lv_adapter_unlock();
       }
   }


使用说明
---------

推荐的 SDKCONFIG 配置
^^^^^^^^^^^^^^^^^^^^^^

在项目的 ``sdkconfig.defaults`` 中添加以下配置以获得最佳 LVGL 性能:

核心 LVGL 配置
""""""""""""""

.. code:: ini

   # FreeRTOS 时钟频率(提高定时器精度)
   CONFIG_FREERTOS_HZ=1000

   # LVGL 操作系统集成
   CONFIG_LV_OS_FREERTOS=y

   # 使用标准 C 库函数
   CONFIG_LV_USE_CLIB_MALLOC=y
   CONFIG_LV_USE_CLIB_STRING=y
   CONFIG_LV_USE_CLIB_SPRINTF=y

   # 显示刷新率(单位:毫秒,15ms ≈ 66 FPS)
   CONFIG_LV_DEF_REFR_PERIOD=15

   # 启用样式缓存以提高性能
   CONFIG_LV_OBJ_STYLE_CACHE=y


多核优化(ESP32-S3/P4)
""""""""""""""""""""""

对于多核芯片,启用并行渲染:

.. code:: ini

   # 使用 2 个绘制线程以提高性能
   CONFIG_LV_DRAW_SW_DRAW_UNIT_CNT=2


PSRAM 优化(如果可用)
""""""""""""""""""""

对于带有 PSRAM 的设备,优化缓存和 XIP 设置:

**ESP32-S3:**

.. code:: ini

   CONFIG_SPIRAM_XIP_FROM_PSRAM=y
   CONFIG_ESP32S3_INSTRUCTION_CACHE_32KB=y
   CONFIG_ESP32S3_DATA_CACHE_64KB=y
   CONFIG_ESP32S3_DATA_CACHE_LINE_64B=y


**ESP32-P4:**

.. code:: ini

   CONFIG_SPIRAM_XIP_FROM_PSRAM=y
   CONFIG_CACHE_L2_CACHE_256KB=y
   CONFIG_CACHE_L2_CACHE_LINE_128B=y


决策流程
^^^^^^^^

在开始使用前,请按以下流程做出决策:

1. **选择接口类型**: 确定使用的显示接口(MIPI DSI / RGB / QSPI / SPI / I2C / I80)
2. **选择默认配置宏**: 根据接口类型选择对应的 ``ESP_LV_ADAPTER_DISPLAY_XXX_DEFAULT_CONFIG`` 宏
3. **选择合适的防撕裂模式** (仅 RGB/MIPI DSI 接口支持):

   - 计算所需的帧缓冲数量 (``num_fbs``)
   - 将 ``num_fbs`` 传入 esp_lcd 面板配置

4. **调节缓冲大小**: 根据内存与吞吐需求调整 ``buffer_height``


显示注册与防撕裂
^^^^^^^^^^^^^^^^^

接口类型与默认配置宏
""""""""""""""""""""

根据您的显示接口选择对应的配置宏:

.. list-table::
   :widths: 20 40 40
   :header-rows: 1

   * - 接口类型
     - 默认配置宏
     - 默认防撕裂模式
   * - MIPI DSI
     - ``ESP_LV_ADAPTER_DISPLAY_MIPI_DEFAULT_CONFIG(...)``
     - ``ESP_LV_ADAPTER_TEAR_AVOID_MODE_DEFAULT_MIPI_DSI``
   * - RGB
     - ``ESP_LV_ADAPTER_DISPLAY_RGB_DEFAULT_CONFIG(...)``
     - ``ESP_LV_ADAPTER_TEAR_AVOID_MODE_DEFAULT_RGB``
   * - SPI/I2C/I80/QSPI (带 PSRAM)
     - ``ESP_LV_ADAPTER_DISPLAY_SPI_WITH_PSRAM_DEFAULT_CONFIG(...)``
     - ``ESP_LV_ADAPTER_TEAR_AVOID_MODE_DEFAULT`` (即 ``NONE``)
   * - SPI/I2C/I80/QSPI (无 PSRAM)
     - ``ESP_LV_ADAPTER_DISPLAY_SPI_WITHOUT_PSRAM_DEFAULT_CONFIG(...)``
     - ``ESP_LV_ADAPTER_TEAR_AVOID_MODE_DEFAULT`` (即 ``NONE``)


.. note::

   - 仅 MIPI DSI 和 RGB 支持防撕裂模式
   - SPI/I2C/I80/QSPI 等接口在适配器中统称为 "OTHER" 接口,仅支持 ``NONE`` 模式


计算帧缓冲数量
""""""""""""""

在 RGB/MIPI DSI 初始化前,需要计算所需的帧缓冲数量:

.. code:: c

   uint8_t num_fbs = esp_lv_adapter_get_required_frame_buffer_count(
       ESP_LV_ADAPTER_TEAR_AVOID_MODE_DEFAULT_RGB,  // 防撕裂模式
       ESP_LV_ADAPTER_ROTATE_0                // 旋转角度
   );
   // 将 num_fbs 传入 esp_lcd 面板配置


**帧缓冲数量规则**:

- 90°/270° 旋转 或 三缓冲模式: 需要 **3** 个帧缓冲
- 双缓冲模式: 需要 **2** 个帧缓冲
- 单缓冲模式: 需要 **1** 个帧缓冲


防撕裂模式选择(RGB/MIPI DSI)
""""""""""""""""""""""""""""

根据您的应用场景选择合适的防撕裂模式:

.. list-table::
   :widths: 20 25 15 15
   :header-rows: 1

   * - 防撕裂模式
     - 适用场景
     - 帧缓冲数
     - 内存占用
   * - ``TRIPLE_PARTIAL``
     - 旋转(90°/270°), 高分辨率流畅 UI
     - 3
     - 高
   * - ``TRIPLE_FULL``
     - 整屏/大区域刷新, 内存充足
     - 3
     - 高
   * - ``DOUBLE_FULL``
     - 大区域刷新, 内存较紧
     - 2
     - 中
   * - ``DOUBLE_DIRECT``
     - 小区域更新, 控件/局部变化
     - 2
     - 中
   * - ``NONE``
     - 静态 UI, 超低内存
     - 1
     - 低


.. warning::

   - RGB/MIPI DSI 在 ``TEAR_AVOID_MODE_NONE`` 下 **不支持旋转** (任何非 0 旋转会被拒绝)
   - OTHER (SPI/I2C/I80/QSPI) 接口仅支持 ``NONE`` 模式; 如需旋转,请在 LCD 初始化阶段配置面板方向(交换 XY/镜像),并相应调整触摸坐标映射


内存估算
""""""""

整帧显存大小 ≈ ``水平分辨率 × 垂直分辨率 × 每像素字节数``

**示例** (RGB565 格式,2 字节/像素):

- 800×480 分辨率: ≈ 750 KB
- 三缓冲: ≈ 2.2 MB
- **结论**: 通常需要 PSRAM 支持


缓冲调优
^^^^^^^^

默认 ``buffer_height`` 值
""""""""""""""""""""""""""

不同接口的 ``buffer_height`` 默认值:

.. list-table::
   :widths: 25 25 50
   :header-rows: 1

   * - 接口类型
     - 默认 ``buffer_height``
     - 说明
   * - MIPI DSI
     - 50
     - 中等条带高度
   * - RGB
     - 50
     - 中等条带高度
   * - OTHER + PSRAM
     - ``ver_res`` (整屏高度)
     - 整屏高度分区,吞吐更高
   * - OTHER 无 PSRAM
     - 10
     - 小条带以节省 RAM


调优建议
""""""""

- **提高** ``buffer_height``:

  - ✅ 提升吞吐量(减少 flush 次数、增大条带)
  - ❌ 增加 RAM 占用

- **降低** ``buffer_height``:

  - ✅ 节省 RAM
  - ❌ 增加 flush 次数,可能降低性能


线程安全
^^^^^^^^

LVGL API **不是线程安全的**,多线程访问时必须使用适配器锁:

.. code:: c

   if (esp_lv_adapter_lock(-1) == ESP_OK) {
       lv_obj_t *btn = lv_btn_create(lv_scr_act());
       lv_obj_set_size(btn, 100, 50);
       esp_lv_adapter_unlock();
   }


输入设备
^^^^^^^^

触摸屏
""""""

适配器提供了触摸设备注册的默认配置宏 ``ESP_LV_ADAPTER_TOUCH_DEFAULT_CONFIG``,简化配置过程:

.. code:: c

   // 使用默认配置宏注册触摸设备
   esp_lv_adapter_touch_config_t touch_cfg = ESP_LV_ADAPTER_TOUCH_DEFAULT_CONFIG(disp, touch_handle);
   lv_indev_t *touch = esp_lv_adapter_register_touch(&touch_cfg);
   assert(touch != NULL);


**参数说明**:

- ``disp``: 关联的 LVGL 显示设备
- ``touch_handle``: 通过 ``esp_lcd_touch`` API 创建的触摸句柄
- 默认缩放比例: x = 1.0, y = 1.0


旋钮/编码器
"""""""""""

需要启用 Kconfig 选项 ``ESP_LV_ADAPTER_ENABLE_KNOB``。详见 ``esp_lv_adapter_input.h``。


按键导航
""""""""

需要启用 Kconfig 选项 ``ESP_LV_ADAPTER_ENABLE_BUTTON``。详见 ``esp_lv_adapter_input.h``。


文件系统桥接(可选)
^^^^^^^^^^^^^^^^^^

启用 ``ESP_LV_ADAPTER_ENABLE_FS`` 后,可将 ``esp_mmap_assets`` 生成的资源分区挂载为 LVGL 文件系统:

.. code:: c

   #include "esp_lv_fs.h"
   #include "esp_mmap_assets.h"

   // 1. 创建 mmap 资源句柄
   mmap_assets_handle_t assets;
   const mmap_assets_config_t mmap_cfg = {
       .partition_label = "assets_A",
       .max_files = MMAP_DRIVE_A_FILES,
       .checksum  = MMAP_DRIVE_A_CHECKSUM,
       .flags = { .mmap_enable = true },
   };
   ESP_ERROR_CHECK(mmap_assets_new(&mmap_cfg, &assets));

   // 2. 挂载为 LVGL 文件系统
   esp_lv_fs_handle_t fs_handle;
   const fs_cfg_t fs_cfg = {
       .fs_letter = 'A',           // 盘符(A 到 Z)
       .fs_assets = assets,
       .fs_nums = MMAP_DRIVE_A_FILES,
   };
   ESP_ERROR_CHECK(esp_lv_adapter_fs_mount(&fs_cfg, &fs_handle));

   // 3. 在 LVGL 中使用文件路径,如 "A:image.png"
   lv_obj_t *img = lv_img_create(lv_scr_act());
   lv_img_set_src(img, "A:my_image.png");


依赖组件 `esp_lv_fs <esp_lv_fs.html>`__ 和 `esp_mmap_assets <esp_mmap_assets.html>`__ 。


图片解码(可选)
^^^^^^^^^^^^^^

启用 ``ESP_LV_ADAPTER_ENABLE_DECODER`` 后,适配器会集成 ``esp_lv_decoder``,支持直接使用 LVGL 图片控件加载各种格式的图片:

.. code:: c

   // 使用 LVGL 图片控件加载图片
   lv_obj_t *img = lv_img_create(lv_scr_act());
   lv_img_set_src(img, "I:red_png.png");  // I: 为图片解码器的盘符


**支持的格式**:

- 标准格式: JPG、PNG、QOI
- 分片格式: SJPG、SPNG、SQOI(针对嵌入式优化)
- 硬件加速: JPEG、PJPG(如果芯片支持)

详细说明请参考 `esp_lv_decoder <esp_lv_decoder.html>`__ 。


FreeType 字体(可选)
^^^^^^^^^^^^^^^^^^^^

启用 ``ESP_LV_ADAPTER_ENABLE_FREETYPE`` 并在 LVGL 中开启 FreeType 后,可以从文件或内存加载矢量字体:

.. code:: c

   #include "esp_lv_adapter.h"

   // 从文件加载字体
   esp_lv_adapter_ft_font_handle_t fh;
   esp_lv_adapter_ft_font_config_t cfg = {
       .name = "F:my_font.ttf",         // 字体文件路径(注意:mmap 资源格式,冒号后不带斜杠)
       .size = 30,                      // 字号
       .style = ESP_LV_ADAPTER_FT_FONT_STYLE_NORMAL,
   };
   ESP_ERROR_CHECK(esp_lv_adapter_ft_font_init(&cfg, &fh));
   const lv_font_t *font30 = esp_lv_adapter_ft_font_get(fh);

   // 使用字体
   lv_obj_set_style_text_font(label, font30, 0);


**堆栈配置**:

.. list-table::
   :widths: 20 40 40
   :header-rows: 1

   * - LVGL 版本
     - 必要配置
     - 说明
   * - v8
     - ``CONFIG_ESP_MAIN_TASK_STACK_SIZE=32768``
     - 字体初始化在调用线程执行
   * - v9
     - ``CONFIG_LV_DRAW_THREAD_STACK_SIZE=32768``
     - 字体渲染在绘图线程执行


.. note::

   - LVGL v8: 不支持 LVGL 虚拟文件系统(``lv_fs``),需使用直接文件路径或内存缓冲区
   - LVGL v9: 支持虚拟文件系统路径(如 ``"F:font.ttf"``)


FPS 与 Dummy Draw
^^^^^^^^^^^^^^^^^^

FPS 统计
""""""""

需要启用 ``ESP_LV_ADAPTER_ENABLE_FPS_STATS``。

.. code:: c

   // 启用 FPS 统计
   ESP_ERROR_CHECK(esp_lv_adapter_fps_stats_enable(disp, true));

   // 获取当前 FPS
   uint32_t fps;
   if (esp_lv_adapter_get_fps(disp, &fps) == ESP_OK) {
       printf("Current FPS: %lu\n", fps);
   }


Dummy Draw
""""""""""

Dummy Draw 模式绕过 LVGL 渲染,直接控制显示:

.. code:: c

   ESP_ERROR_CHECK(esp_lv_adapter_set_dummy_draw(disp, true));
   ESP_ERROR_CHECK(esp_lv_adapter_dummy_draw_blit(disp, 0, 0, 800, 480, framebuffer, true));
   ESP_ERROR_CHECK(esp_lv_adapter_set_dummy_draw(disp, false));


立即刷新
^^^^^^^^

强制进行同步刷新:

.. code:: c

   esp_lv_adapter_refresh_now(disp);


配置项 (Kconfig)
-----------------

通过 ``idf.py menuconfig`` 可以配置以下选项:

.. list-table::
   :widths: 40 60
   :header-rows: 1

   * - 配置项
     - 说明
   * - ``ESP_LV_ADAPTER_ENABLE_FS``
     - 启用文件系统桥接 (``esp_lv_fs``)
   * - ``ESP_LV_ADAPTER_ENABLE_DECODER``
     - 启用图片解码 (``esp_lv_decoder``)
   * - ``ESP_LV_ADAPTER_ENABLE_FREETYPE``
     - 启用 FreeType 矢量字体支持
   * - ``ESP_LV_ADAPTER_ENABLE_FPS_STATS``
     - 启用 FPS 统计功能
   * - ``ESP_LV_ADAPTER_ENABLE_BUTTON``
     - 启用按键导航输入
   * - ``ESP_LV_ADAPTER_ENABLE_KNOB``
     - 启用旋钮/编码器输入


限制与注意事项
---------------

接口与防撕裂支持
^^^^^^^^^^^^^^^^

.. list-table::
   :widths: 40 60
   :header-rows: 1

   * - 接口类型
     - 支持的防撕裂模式
   * - RGB / MIPI DSI
     - ``NONE``, ``DOUBLE_FULL``, ``TRIPLE_FULL``, ``DOUBLE_DIRECT``, ``TRIPLE_PARTIAL``
   * - OTHER (SPI/I2C/I80/QSPI)
     - 仅 ``NONE``


旋转限制
^^^^^^^^

- **RGB/MIPI DSI**:

  - 在 ``TEAR_AVOID_MODE_NONE`` 下 **不支持旋转** (任何非 0 旋转会被拒绝)

- **OTHER (SPI/I2C/I80/QSPI)**:

  - 适配器不对 90°/270° 进行旋转处理
  - 如需旋转,请在 LCD 初始化阶段配置面板方向(交换 XY/镜像)
  - 同时需要相应调整触摸坐标映射


缓冲与渲染模式
^^^^^^^^^^^^^^

- **模式映射**:

  - ``DOUBLE_FULL`` / ``TRIPLE_FULL`` → FULL 渲染模式
  - ``DOUBLE_DIRECT`` → DIRECT 渲染模式
  - 其余(含 ``TRIPLE_PARTIAL``) → PARTIAL 渲染模式

- **LVGL 绘制缓冲数量**:

  - ``TRIPLE_PARTIAL``: 1 块
  - ``TRIPLE_FULL`` / ``DOUBLE_*``: 2 块
  - FULL/DIRECT 模式默认: 2 块
  - 其他: 1 块

- **OTHER + NONE**:

  - 不申请面板帧缓冲
  - 仅使用 LVGL 绘制缓冲的条带方式发送数据


PPA/DMA2D 加速
^^^^^^^^^^^^^^

- 启用 PPA 加速时,建议设置 ``LV_DRAW_SW_DRAW_UNIT_CNT == 1``
- 旋转操作优先使用 PPA; 否则使用 CPU 拷贝路径(带缓存局部性的块拷贝)
- **效果与预期**:

  - PPA 可显著降低 CPU 占用
  - 通常不会带来明显的 FPS 提升


反初始化
---------

关闭 UI 时的推荐资源释放顺序:

.. code:: c

   // 1. 注销输入设备
   esp_lv_adapter_unregister_touch(touch);
   // esp_lv_adapter_unregister_encoder(encoder);
   // esp_lv_adapter_unregister_navigation_buttons(buttons);

   // 2. 注销显示设备
   esp_lv_adapter_unregister_display(disp);

   // 3. 卸载文件系统(如果使用)
   esp_lv_adapter_fs_unmount(fs_handle);
   // 释放 mmap 资源
   mmap_assets_del(assets);

   // 4. 反初始化适配器
   // 注意:如果启用了 FreeType,字体资源会自动清理
   esp_lv_adapter_deinit();


兼容性
-------

- **ESP-IDF**: >= 5.5
- **LVGL**: >= 8, < 10(支持 v8 和 v9)


ESP-IDF 补丁说明
^^^^^^^^^^^^^^^^^

PPA 卡死问题修复(ESP32-P4)
""""""""""""""""""""""""""

如果您在 ESP32-P4 上使用 ``ESP_LV_ADAPTER_TEAR_AVOID_MODE_TRIPLE_PARTIAL`` 模式并启用屏幕旋转时遇到画面卡死问题,需要应用以下补丁:

**适用版本**: ESP-IDF release/v5.5 (commit ``62beeae461bd3692c2028f96a93c84f11291e155``)

**补丁文件**: ``0001-bugfix-lcd-Fixed-PPA-freeze.patch``

**应用方法**:

在 ESP-IDF 仓库根目录下执行:

.. code:: bash

   cd $IDF_PATH
   git apply /path/to/esp_lvgl_adapter/0001-bugfix-lcd-Fixed-PPA-freeze.patch


**问题说明**:

- 该补丁修复了 ESP32-P4 在 ``ESP_LV_ADAPTER_TEAR_AVOID_MODE_TRIPLE_PARTIAL`` 模式下使能旋转时,PPA 硬件加速导致画面卡死的问题
- 仅影响同时满足以下条件的场景:

  - 使用 ESP32-P4 芯片
  - 使用 ``TRIPLE_PARTIAL`` 防撕裂模式
  - 启用了屏幕旋转(90°/270°)

- 其他配置组合或芯片不需要此补丁


**验证补丁是否已应用**:

检查 ``components/esp_driver_ppa/src/ppa_srm.c`` 文件中是否包含以下代码:

.. code:: c

   PPA.sr_byte_order.sr_macro_bk_ro_bypass = 1;


术语表
-------

.. list-table::
   :widths: 30 70
   :header-rows: 1

   * - 术语
     - 说明
   * - **面板帧缓冲**
     - 由显示面板驱动持有的硬件帧缓冲区
   * - **LVGL 绘制缓冲**
     - LVGL 用于渲染条带或整帧后再刷新到面板的 RAM 区域
   * - **分区刷新**
     - 仅更新屏幕的部分区域(脏区域)而非整屏刷新
   * - **直写(Direct)**
     - LVGL 直接渲染到面板帧缓冲,减少内存拷贝; 适合小区域刷新
   * - **PPA**
     - Pixel Processing Accelerator(像素处理加速器),用于硬件加速旋转/缩放/内存拷贝等操作
   * - **防撕裂**
     - 通过多缓冲等技术避免屏幕更新时出现撕裂现象


应用示例
---------

.. list-table::
   :widths: 30 70
   :header-rows: 1

   * - 示例
     - 说明
   * - ``lvgl_common_demo``
     - 统一的 LVGL 演示,支持多种接口
   * - ``lvgl_multi_screen``
     - 多显示器管理示例
   * - ``lvgl_decode_image``
     - 图片解码功能演示
   * - ``lvgl_freetype_font``
     - FreeType 字体加载与使用
   * - ``lvgl_dummy_draw``
     - Dummy Draw 切换


常见问题
---------

FreeType 崩溃或显示异常
^^^^^^^^^^^^^^^^^^^^^^^^

**解决方法**:

- 确认 LVGL 配置中已启用 FreeType(``CONFIG_LV_USE_FREETYPE=y``)
- LVGL v8: 设置 ``CONFIG_ESP_MAIN_TASK_STACK_SIZE=32768``
- LVGL v9: 设置 ``CONFIG_LV_DRAW_THREAD_STACK_SIZE=32768``


屏幕撕裂或闪烁
^^^^^^^^^^^^^^^

**可能原因**:

- 防撕裂模式选择不当
- 帧缓冲数量不正确
- 旋转配置不当

**解决方法**:

1. 根据应用场景选择合适的防撕裂模式
2. 使用 ``esp_lv_adapter_get_required_frame_buffer_count()`` 计算正确的帧缓冲数


图片解码或文件系统不可用
^^^^^^^^^^^^^^^^^^^^^^^^^

**可能原因**:

- Kconfig 选项未启用
- 组件依赖缺失
- 资源分区未正确烧录

**解决方法**:

1. 检查相应的 Kconfig 开关
2. 确认 ``esp_mmap_assets`` 等依赖组件已添加
3. 验证资源分区是否正确打包并烧录到 Flash


LVGL API 调用崩溃
^^^^^^^^^^^^^^^^^^

**可能原因**:

- 在多线程环境中未加锁访问 LVGL API

**解决方法**:

.. code:: c

   // 所有 LVGL API 调用都必须加锁保护
   if (esp_lv_adapter_lock(-1) == ESP_OK) {
       // LVGL API 调用
       esp_lv_adapter_unlock();
   }


API 参考
---------

.. include-build-file:: inc/esp_lv_adapter.inc

