:orphan:

HMI WorkFlow
============

:link_to_translation:`en:[English]`

概述
----

本文档将介绍使用 ESP32 提供的硬件资源和软件资源开发人机交互界面 HMI
的方法。

ESP32 HMI 资源
--------------

-  硬件资源

   -  LCD 接口：SPI、I2S
   -  外部存储器 Cache

-  软件资源

   -  μGFX 图形库
   -  LittlevGL 图形库

ESP32 图形库特性
----------------

-  多国语言支持
-  易移植外设驱动架构
-  丰富的 GUI 例程
-  绘制 UI 工具 (μGFX)
-  PC 模拟器 (LittlevGL)

μGFX 介绍
---------

-  2D 图像库

   -  丰富的绘图 API
   -  图像显示功能 (bmp、jpeg、png、gif)
   -  Alpha 混合

-  多种字体

   -  可导入用户字体
   -  支持多国语言
   -  抗锯齿

-  支持嵌入式操作系统
-  清晰的驱动接口，方便用户快速移植 LCD 驱动
-  提供从简单到复杂的例程

   -  快速上手
   -  加快应用开发速度

-  丰富的 GUI 控件

   -  可自定义 GUI 控件

μGFX 结构
~~~~~~~~~

.. figure:: ../../_static/hmi_solution/ugfx/500px-Architecture2.png
    :align: center

μGFX 开发步骤
~~~~~~~~~~~~~

1. 准备资源

   图片/字体等资源

2. 代码转换

   将资源文件（图片/字体）转换为代码或者存储在外部存储器

3. UI 原型设计

   将 UI 的原型通过 PS 或 μGFX-Studio 等软件呈现出来

4. ESP32 平台选择

5. 基于 ESP32 μGFX 库进行移植、相关驱动开发

6. 基于 μGFX-Studio 平台开发应用程序（可选）

7. 应用代码开发

8. 调试运行

资源准备示例
~~~~~~~~~~~~

1. 图片资源处理

   通过 ``./file2c -dcs romfs_img_ugfx.png romfs_img_ugfx.h`` 命令，将
   ``png`` 图片资源生成相应的 ``.h`` 文件

2. 字体资源处理

   通过\ `在线字体转换工具 <https://ugfx.io/font-converter>`_ 将
   ``*.ttf`` 字体资源生成相应的 ``.c`` 文件

使用 μGFX-Studio 开发应用程序
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

-  μGFX 可运行在 Windows
-  拖放式 UI 设计
-  创建多个显示页面
-  FontManager - 使用您想要的任何字体
-  ImageManager - 使用您想要的任何图像
-  自定义绘制方式

.. figure:: ../../_static/hmi_solution/ugfx/ugfx_studio.jpg
    :align: center


LittlevGL 介绍
--------------

-  内置良好主题样式
-  出色的性能

   -  更低的内存消耗
   -  更易实现动画效果

-  抗锯齿、不透明度、平滑滚动
-  清晰的驱动接口，方便用户快速移植 LCD 驱动
-  丰富的 GUI 控件

   -  可自定义 GUI 控件

.. figure:: ../../_static/hmi_solution/littlevgl/obj_types.jpg
    :align: center

.. figure:: ../../_static/hmi_solution/littlevgl/lv_theme_intro.png
    :align: center

LittlevGL 结构
~~~~~~~~~~~~~~

.. figure:: ../../_static/hmi_solution/littlevgl/sys.jpg
    :align: center

LittlevGL 开发步骤
~~~~~~~~~~~~~~~~~~

1. 准备资源

   图片/字体等资源

2. 代码转换

   将资源文件（图片/字体）转换为代码或者存储在外部存储器

3. UI 原型设计

   将 UI 的原型通过 PS 等软件呈现出来

4. ESP32 平台选择

5. 基于 ESP32 LittlevGL 库进行移植、相关驱动开发

6. 使用 PC 模拟器开发应用程序（可选）

7. 应用代码开发

8. 调试运行

资源准备示例
~~~~~~~~~~~~

1. 图片资源处理

   通过\ `在线图片转换工具 <https://littlevgl.com/image-to-c-array>`__\ 将图片资源转换为相应的
   ``.c`` 文件

2. 字体资源处理

   通过\ `在线字体转换工具 <https://littlevgl.com/ttf-font-to-c-array>`__\ 将
   ``*.ttf`` 字体资源生成相应的 ``.c`` 文件

使用 LittlevGL 模拟器开发应用程序
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

-  独立的硬件抽象层便于平台移植
-  模拟器可运行在 Linux、Windows、Mac OS
-  支持 Eclipse



开发示例说明
------------

本节将以一个MP3开发
为例具体说明 HMI 界面开发过程（在这里将不进行
`ESP-ADF <https://github.com/espressif/esp-adf>`__ 相关介绍和使用）

1. 准备资源

   -  在 mp3\_example 中我们使用的字体为系统默认字体: DejaVu 字体、20
      px；
   -  并使用内置符号字体：SYMBOL\_AUDIO、SYMBOL\_LIST、SYMBOL\_SETTINGS、SYMBOL\_PREV、SYMBOL\_PLAY、SYMBOL\_NEXT、SYMBOL\_PAUSE。

   所以，我们不要额外准备字体、图片等资源文件。

2. 代码转换

   因为在 mp3\_example 中未使用外部资源，所以不需要进行资源转换。

3. UI 原型设计

   这里我们只进行页面简要框架设计：

   ::

       +---------+---------+---------+     +---------+---------+---------+     +---------+---------+---------+
       |         |         |         |     |         |         |         |     |         |         |         |
       |         |         |         |     |         |         |         |     |         |         |         |
       +---------+---------+---------+     +---------+---------+---------+     +---------+---------+---------+
       |                             |     |                             |     |                             |
       |       +-------------+       |     | +-------------------------+ |     |            +--------+       |
       |       |             |       |     | +-------------------------+ |     | +-------+                   |
       |       +-------------+       |     |                             |     | |       |  +--------+       |
       |                             |     | +-------------------------+ |     | +-------+                   |
       |    +--+    +---+    +--+    |     | +-------------------------+ |     |            +--------+       |
       |    |  |    |   |    |  |    |     |                             |     |                             |
       |    +--+    +---+    +--+    |     | +-------------------------+ |     |                             |
       |                             |     | |-------------------------| |     |                             |
       +-----------------------------+     +-----------------------------+     +-----------------------------+

       +--------播放控制页面-----------+     +-----------歌曲选择页面--------+     +-----------设置页面-----------+ 

   主要包含 3
   个页面：播放控制页面、歌曲选择页面、设置页面；通过点击屏幕顶部的 3
   个按钮进行切换，按钮上显示意义相近的符号。

   -  播放控制页面：显示当前选择的 mp3
      文件名称；上/下一曲、播放/暂停按钮，按钮上显示意义相近的符号
   -  歌曲选择页面：该页面中显示从 SD-Card 中读取的 MP3
      文件名称列表，在每一个列表项前都显示一个音乐符号
   -  设置页面：设置页面中显示设置项以及对应可选的参数，在 mp3\_example
      中只进行主题设置

4. ESP32 平台选择

   在 mp3\_example 中使用 LittlevGL GUI
   库进行开发，对内存资源要求低，所以选择 `ESP32
   DevKitC <https://docs.espressif.com/projects/esp-idf/en/stable/hw-reference/modules-and-boards.html#esp32-devkitc-v4>`__
   开发板搭配
   `ESP-WROOM32 <https://docs.espressif.com/projects/esp-idf/en/stable/hw-reference/modules-and-boards.html#esp32-wroom-32>`__
   模组进行开发

5. 基于 ESP32 LittlevGL 库进行移植、相关驱动开发

   mp3\_example 使用的 LittlevGL GUI 已经移植到
   `esp-iot-solution <https:404>`__\ ，mp3\_example
   中使用外部设备为：2.8 inch、240\*320 pixel、 ILI9341 显示屏 和
   XPT2046 触摸屏，这两者的驱动在
   `esp-iot-solution <https:404>`__
   中都有提供，无需再次移植；若选择其他型号的显示屏或者触摸屏，需要进行相关驱动开发。

6. 使用 PC 模拟器开发应用程序（可选）

   在该示例开发过程中没有使用 LittlevGL PC 模拟器进行开发，如果需要使用
   PC 模拟器，可以参考 `PC
   Simulator <https://docs.littlevgl.com/#PC-simulator>`__ 。

7. 应用代码开发
   
   `本节只介绍界面相关开发，涉及到的其余相关外设使用不进行介绍。`

   -  主体框架：考虑到 mp3\_example 中的三个主页面以及通过 3
      个按钮进行切换，选择
      :doc:`tableview <littlevgl_guide>` 进行三个页面的管理最为合适，在 tabview
      中添加三个子页面并且为每个页面的按钮指定 1 个符号字体： 
      ::
      
          /* LittlevGL GUI 初始化，相关显示屏以及触摸屏初始化*/
          lvgl_init();

          /* 当前主题设置 */
          lv_theme_t *th = lv_theme_zen_init(100, NULL);
          lv_theme_set_current(th);

          /* tabview 创建 */ 
          v_obj_t *tabview = lv_tabview_create(lv_scr_act(), NULL);

          /* 子页面添加、指定符号字体 */ 
          lv_obj_t *tab1 = lv_tabview_add_tab(tabview, SYMBOL_AUDIO); 
          lv_obj_t *tab2 = lv_tabview_add_tab(tabview, SYMBOL_LIST); 
          lv_obj_t *tab3 = lv_tabview_add_tab(tabview, SYMBOL_SETTINGS);
        

   -  播放控制页面： 显示当前选择的 mp3
      文件名称；上/下一曲、播放/暂停按钮，这些控件我们通过 1 个 :doc:`container <littlevgl_guide>` 进行管理：
      ::

          /* container 创建 */
          lv_obj_t *cont = lv_cont_create(tab1, NULL);

          /* container 大小设置 */
          lv_obj_set_size(cont, LV_HOR_RES - 20, LV_VER_RES - 85);
          lv_cont_set_fit(cont, false, false);

      - 当前播放音频文件名称显示，使用 1 个 :doc:`label <littlevgl_guide>` 控件进行显示，显示内容可动态编辑：

      ::

          /* label 创建 */
          lv_obj_t *current_music = lv_label_create(cont, NULL);
          /* label 长模式设置 */
          lv_label_set_long_mode(current_music, LV_LABEL_LONG_ROLL);

          /* label 位置、大小、对齐方式设置 */
          lv_obj_set_pos(current_music, 50, 20);
          lv_obj_set_width(current_music, 200);
          lv_obj_align(current_music, cont, LV_ALIGN_IN_TOP_MID, 0, 20); /* Align to LV_ALIGN_IN_TOP_MID */

          /* label 显示内容编辑 */
          lv_label_set_text(current_music, "MP3 文件名称");
          ```

      -  播放控制按钮:

      ::

          /* 符号字体资源 */
          void *img_src[] = {SYMBOL_PREV, SYMBOL_PLAY, SYMBOL_NEXT, SYMBOL_PAUSE};
          
          /* 3 个按钮创建 */
          for (uint8_t i = 0; i < 3; i++) {
            button[i] = lv_btn_create(cont, NULL);

          /* 按钮大小设置 */
          lv_obj_set_size(button[i], 50, 50);

          /* img 创建 */
          img[i] = lv_img_create(button[i], NULL);

          /* img 显示内容设置 */
          lv_img_set_src(img[i], img_src[i]);

          }

          /* 3 个按钮位置、对齐方式设置 */
          lv_obj_align(button[0], cont, LV_ALIGN_IN_LEFT_MID, 35, 20);
          for (uint8_t i = 1; i < 3; i++) {
            lv_obj_align(button[i], button[i - 1], LV_ALIGN_OUT_RIGHT_MID, 40, 0);
          }

          /* 3 个按钮点击事件添加 */
          lv_btn_set_action(button[0], LV_BTN_ACTION_CLICK, audio_next_prev);
          lv_btn_set_action(button[1], LV_BTN_ACTION_CLICK, audio_control);
          lv_btn_set_action(button[2], LV_BTN_ACTION_CLICK, audio_next_prev);

   -  歌曲选择页面：显示 MP3 文件名称列表，在子页面添加
      `list <littlevgl/littlevgl_guide_cn.md#list-lv_list>`__ 控件即可：
      ::

          /* list 创建、大小设置 */
             lv_obj_t *list = lv_list_create(tab2, NULL);
          lv_obj_set_size(list, LV_HOR_RES - 20, LV_VER_RES - 85);

          /* list item 添加、并指定符号字体、添加点击事件 */
          for (uint8_t i = 0; i < filecount; i++) {
            list_music[i] = lv_list_add(list, SYMBOL_AUDIO, "MP3 文件名称", play_list);
          }
 

   -  设置页面：主题设置，需要添加 1 个 label 显示设置内容，1 个
      `roller <littlevgl/littlevgl_guide_cn.md#roller-lv_roller>`__
      显示可选项 
      
      ::

          /* label 创建、显示内容设置 */
          lv_obj_t *theme_label = lv_label_create(tab3, NULL);
          lv_label_set_text(theme_label, "Theme:");

          /* roller 创建、对齐方式设置 */
          lv_obj_t *theme_roller = lv_roller_create(tab3, NULL);
          lv_obj_align(theme_roller, theme_label, LV_ALIGN_OUT_RIGHT_MID, 20, 0);

          /* 可选项添加、显示设置、点击事件添加 */
          lv_roller_set_options(theme_roller, "Night theme\nAlien theme\nMaterial theme\nZen theme\nMono theme\nNemo theme");
          lv_roller_set_selected(theme_roller, 1, false);
          lv_roller_set_visible_row_count(theme_roller, 3);
          lv_ddlist_set_action(theme_roller, theme_change_action);

   -  相关点击事件：

      ::

       /* 播放/暂停点击事件 */
       static lv_res_t audio_control(lv_obj_t *obj)
        {
            /* img 符号字体改变 */
            play ? lv_img_set_src(img[1], img_src[1]) : lv_img_set_src(img[1], img_src[3]);
            play = !play;
            return LV_RES_OK;
        }

        /* 上/下一曲点击事件 */
        static lv_res_t audio_next_prev(lv_obj_t *obj)
        {
            if (obj == button[0]) {
                // prev song

                /* img 符号字体改变 */
                lv_img_set_src(img[1], img_src[3]);

                /* label 显示内容编辑 */
                lv_label_set_text(current_music, "MP3 文件名称");
                play = true;
            } else if (obj == button[1]) {
            } else if (obj == button[2]) {
                // next song

                /* img 符号字体改变 */
                lv_img_set_src(img[1], img_src[3]);

                /* label 显示内容编辑 */
                lv_label_set_text(current_music, "MP3 文件名称");
                play = true;
            }
            return LV_RES_OK;
        }

        /* 歌曲选择点击事件 */
        static lv_res_t play_list(lv_obj_t *obj)
        {
            for (uint8_t i = 0; i < MAX_PLAY_FILE_NUM; i++) {
                if (obj == list_music[i]) {

                    /* img 符号字体改变 */
                    lv_img_set_src(img[1], img_src[3]);

                    /* label 显示内容编辑 */
                    lv_label_set_text(current_music, "MP3 文件名称");
                    play = true;
                    break;
                }
            }
            return LV_RES_OK;
        }

        /* 主题选择点击事件 */
        static lv_res_t theme_change_action(lv_obj_t *roller)
        {
            lv_theme_t *th;
            /* 主题切换 */
            switch (lv_ddlist_get_selected(roller)) {
            case 0:
                th = lv_theme_night_init(100, NULL);
                break;

            case 1:
                th = lv_theme_alien_init(100, NULL);
                break;

            case 2:
                th = lv_theme_material_init(100, NULL);
                break;

            case 3:
                th = lv_theme_zen_init(100, NULL);
                break;

            case 4:
                th = lv_theme_mono_init(100, NULL);
                break;

            case 5:
                th = lv_theme_nemo_init(100, NULL);
                break;

            default:
                th = lv_theme_default_init(100, NULL);
                break;
            }
            lv_theme_set_current(th);
            return LV_RES_OK;
        }

8. 调试运行

   编译、下载，然后在实际设备上运行，对出现的问题进行相关记录，并在代码中进行相关修改、再次调试。

总结
----

-  ESP32 为用户界面应用开发提供了：

   -  强大的 CPU 处理能力及其丰富的外设接口
   -  μGFX 和 LittlevGL 图形库供开发工程师选择

-  ESP32 用户界面设计方案可广泛应用于：

   -  便携或穿戴式消费电子产品，智能楼宇和工业控制器、智能家电、个人医疗设备、保健点医疗设备，车载电子等


