:orphan:

LittlevGL Guide
===============

:link_to_translation:`en:[English]`

LittlevGL 简介
--------------

`LittlevGL <https://littlevgl.com/>`__
是一个免费的开源图形库，可构建全功能嵌入式 GUI。LittlevGL
有易于使用的图形元素以及良好的视觉效果和内存占用低等特点。LittlevGL
是一个完整的图形框架，您不需要考虑原始图形如何绘制。您可以使用已有图形元素来构建
GUI，例如按钮，图表，图像，列表，滑块，开关，键盘等。

LittlevGL 具有以下特点： - 图形元素：按钮、图表、列表、滑块、图像等 -
高级图形效果：具有动画、抗锯齿、不透明度、平滑滚动等效果 -
支持多种输入设备：触摸板、鼠标、键盘、编码器等 - 多语言支持：使用 UTF-8
编码 - 完全可自定义的图形元素 - 支持任意微控制器或显示器（不依赖于硬件）
- 高度可扩展：支持最小内存（80 KB Flash，10 KB RAM） - 支持
OS、外部存储器和 GPU（可选） - 单帧缓存操作：同样具有很好的图形效果 - 用
C 语言编写：具备很好的兼容性（兼容 C++） -
模拟器：在没有嵌入式硬件的情况下，可在 PC 上进行嵌入式 GUI 设计

操作系统中使用
--------------

LittlevGL 不是线程安全的。尽管如此，在操作系统中使用 LittlevGL
仍然非常简单。

简单的方法是不使用操作系统的任务，而是使用
``lv_tasks``\ 。\ ``_lv_task_`` 是 ``lv_task_handler``
中定期调用的函数。在 ``_lv_task_``
中，您可以获取传感器，缓冲区等的状态，并调用 LittlevGL 函数来刷新
GUI。调用
``lv_task_create（my_func，period_ms，LV_TASK_PRIO_LOWEST/LOW/MID/HIGH/HIGHEST，custom_ptr）``
函数创建 ``_lv_task_``

如果您需要使用其他任务或线程，则需要一个互斥锁，在调用
``lv_task_handler`` 之前应该获取它并在之后释放。此外，您必须在每个调用
LittlevGL（\ ``lv _...``\ ）
相关函数的任务和线程中使用该互斥锁。这样，您就可以在真正的多任务环境中使用
LittlevGL 只需使用互斥锁即可避免并发调用 LittlevGL 函数。

LittlevGL 介绍
--------------

-  `图形对象`_

   -  对象属性
   -  对象的工作机制
   -  创建/删除对象
   -  图层

-  `样式`_

   -  样式属性
   -  使用样式
   -  内置样式
   -  动画样式
   -  样式示例
   -  主题

-  `颜色`_
-  `字体`_

   -  内置字体
   -  支持 Unicode
   -  符号字体
   -  添加字体

-  `动画`_
-  `输入设备`_
-  `对象组`_
-  `绘图和渲染`_

   -  Buffered 和 Unbuffered
   -  抗锯齿

图形对象
~~~~~~~~

在 LittlevGL 中，用户界面的基本构建单位是对象。例如： - Button（按钮） -
Label（标签） - Image（图像） - List（列表） - Chart（图表） - Text
area（文本块）

`查看 LittlevGL 中已有的对象 <https://lvgl.io/demos>`__

对象属性
^^^^^^^^

1. 基本属性

   对象具有与其类型无关的基本属性：

   -  位置
   -  尺寸
   -  父对象
   -  拖动使能
   -  点击使能等

2. 特殊属性

   对象具有与其类型有关的特殊属性。例如，滑块具有：

   -  最小、最大值
   -  当前值
   -  回调函数
   -  样式

对象的工作机制
^^^^^^^^^^^^^^

-  父/子结构

   父对象可以被视为其子对象的容器。每个对象只有一个父对象（Screen
   除外），但父对象可以拥有任意数量的子对象。

-  Screen

   Screen 是一个没有父对象的特殊对象。默认情况下，LittlevGL
   会创建并加载一个 Screen。要获取当前有效的 Screen，可以使用
   ``lv_scr_act()`` 函数。Screen 可以由任何对象类型创建。

-  共同移动

   如果更改了父对象的位置，则子对象将与父对象一起移动。因此，所有位置都与父对象相关。因此（0,0）坐标意味着对象将独立于父对象的位置，始终在父对象的左上角。

.. figure:: ../../_static/hmi_solution/littlevgl/par_child1.jpg
    :align: center
   
    图 1. 共同移动
    
-  仅在父对象中可见

   如果子对象部分或完全脱离其父对象，那么在父对象外面的部分将不可见。

.. figure:: ../../_static/hmi_solution/littlevgl/par_child3.jpg
    :align: center

    图 2. 仅在父对象中可见


创建/删除对象
^^^^^^^^^^^^^

在 LittlevGL
中，可以在运行时动态创建和删除对象。这意味着只有当前创建的对象占用
RAM。例如：如果您需要图表，则可以在需要时创建图表，并在使用图表后删除。

每个对象类型都有自己的创建函数和一个统一的原型（样式等）。它需要两个参数：

-  指向父对象的指针
-  指向相同类型的对象的指针（可选）

如果第二个参数不为 NULL，则到新创建的对象将复制此指针所指向的对象。

要创建 Screen，请将父对象指针设置为
NULL。创建函数的返回值是指向创建的对象的指针。独立于对象类型，使用公共类型
``lv_obj_t``\ 。稍后可以使用此指针来设置或获取对象的属性。创建函数如下所示：

``lv_obj_t * lv_type_create(lv_obj_t * parent, lv_obj_t * copy);``

所有对象类型都使用相同的删除函数： - 使用
``void lv_obj_del(lv_obj_t * obj);`` 函数将删除对象本身及其所有子对象 -
使用 ``void lv_obj_clean(lv_obj_t * obj);``
函数只删除对象的子对象，但保留对象本身

图层
^^^^

先创建的对象（及其子对象）将先绘制，最后创建的对象将在其兄弟对象（都有相同的父对象）中位于顶部。这样就可以在同一级别的对象之间计算顺序。例如：可以通过创建
2 个对象（可以是透明的）添加图层，首先是 “A”，然后是 “B”。“A”
和它的子对象都处在下一层中，可以被 “B” 和它的子对象覆盖。

.. figure:: ../../_static/hmi_solution/littlevgl/par_child4.jpg
    :align: center



    图 3. 图层



样式
~~~~

可以通过设置样式改变对象的外观风格。样式是一个结构体变量，有颜色、内边距、可见性等属性。有一个共同的样式类型：\ ``lv_style_t``\ 。通过设置
``lv_style_t`` 结构体的字段，可以改变使用该样式对象的外观风格。

样式属性
^^^^^^^^

样式有 5
个主要部分：公共的、主体、文字、图像和线条。对象将使用与其类型相关的字段。例如：线条不使用
``letter_space``\ 。要查看对象类型使用哪些字段，请查阅\ `文档 <https://lvgl.io/demos>`__\ 。样式结构体的字段如下：

-  公共属性

   -  **glass
      1**\ ：禁止继承这个样式（如果一个样式是透明的，可以设置这个属性以便子对象使用其他的样式）

-  主体属性

   由类似矩形的对象使用：

   -  **body.empty** 不填充矩形（只是绘制边框和/或阴影）
   -  **body.main\_color** 主颜色
   -  **body.grad\_color** 渐变色
   -  **body.radius** 转角半径
   -  **body.opa** 不透明度
   -  **body.border.color** 边框颜色
   -  **body.border.width** 边框宽度
   -  **body.border.part** 边框显示位置
      (``LV_BORDER_LEFT/RIGHT/TOP/BOTTOM/FULL``)
   -  **body.border.opa** 边界不透明度
   -  **body.shadow.color** 阴影颜色
   -  **body.shadow.width** 阴影宽度
   -  **body.shadow.type** 阴影类型
   -  **body.padding.hor** 水平内边距
   -  **body.padding.ver** 垂直内边距
   -  **body.padding.inner** 内边距

-  文本属性

   由显示文本的对象使用：

   -  **text.color** 文本颜色
   -  **text.font** 文本所使用的字体
   -  **text.opa** 文本不透明度
   -  **text.letter\_space** 字间距
   -  **text.line\_space** 行间距

-  图像属性

   由类似图像的对象或对象上的图标使用：

   -  **image.color** 基于像素亮度的图像重新着色的颜色
   -  **image.intense** 重新着色强度
   -  **image.opa** 图像不透明度

-  线条属性

   由包含线条或线状元素的对象使用：

   -  **line.color** 线条颜色
   -  **line.width** 线条宽度
   -  **line.opa** 线条不透明度

使用样式
^^^^^^^^

每种对象类型都有一个单独的函数设置样式。对象使用的样式和样式属性在\ `文档 <https://lvgl.io/demos>`__\ 中可找到。

如果对象只有一个样式，比如标签：可以使用
``lv_label_set_style(label1, &style)``
函数设置新样式。如果对象有很多样式（如按钮每个状态有 5
个样式），则可以使用
``lv_btn_set_style(obj，LV_BTN_STYLE _...，＆rel_style)``
函数设置新样式。如果对象的样式为
NULL，则其样式将继承其父对象的样式。如果修改由一个或多个对象使用的样式，则必须通知对象有关样式的更改。有两种方式通知对象：

::

    void lv_obj_refresh_style(lv_obj_t * obj);      /*Notify an object of the style modification*/
    void lv_obj_report_style_mod(void * style);     /*Notify all objects of the style modification. Use NULL to notify all objects*/

内置样式
^^^^^^^^

LittlevGL 库中有几种内置样式：

.. figure:: ../../_static/hmi_solution/littlevgl/style-built-in.jpg
    :align: center

    图 4. 内置样式

如上图所示，有用于
Screen、按钮的普通样式和美化样式以及透明样式。\ ``lv_style_transp``\ 、\ ``lv_style_transp_fit``
和 ``lv_style_transp_tight`` 仅在填充方面有所不同：对于
``lv_style_transp_tight``\ ，所有填充都为零；对于
``lv_style_transp_fit``\ ，只有 ``hor`` 和 ``ver`` 填充为零。

内置样式是全局 ``lv_style_t`` 变量，因此您可以直接使用它们，例如：
``lv_btn_set_style(obj，LV_BTN_STYLE_REL，＆lv_style_btn_rel)``\ 。

您可以修改内置样式，也可以创建新样式。在创建新样式时，建议首先复制内置样式，以确保使用适当的值初始化所有字段。使用
``lv_style_copy(＆dest_style，＆src_style)`` 函数复制样式。

动画样式
^^^^^^^^

您可以使用 ``lv_style_anim_create(＆anim)``
函数为样式设置动画。在调用此函数之前，您必须初始化 ``lv_style_anim_t``
变量。动画会将从 ``style_1`` 样式淡化到 ``style_2`` 样式。

样式示例
^^^^^^^^

以下示例演示了上述样式用法：

.. figure:: ../../_static/hmi_solution/littlevgl/style-example.jpg
    :align: center

    图 5. 样式示例

::

    /*Create a style*/
    static lv_style_t style1;
    lv_style_copy(&style1, &lv_style_plain);    /*Copy a built-in style to initialize a new style*/
    style1.body.main_color = LV_COLOR_WHITE;
    style1.body.grad_color = LV_COLOR_BLUE;
    style1.body.radius = 10;
    style1.body.border.color = LV_COLOR_GRAY;
    style1.body.border.width = 2;
    style1.body.border.opa = LV_OPA_50;
    style1.body.padding.hor = 5;            /*Horizontal padding, used by the bar indicator below*/
    style1.body.padding.ver = 5;            /*Vertical padding, used by the bar indicator below*/
    style1.text.color = LV_COLOR_RED;

    /*Create a simple object*/
    lv_obj_t *obj1 = lv_obj_create(lv_scr_act(), NULL);
    lv_obj_set_style(obj1, &style1);                        /*Apply the created style*/
    lv_obj_set_pos(obj1, 20, 20);                           /*Set the position*/

    /*Create a label on the object. The label's style is NULL by default*/
    lv_obj_t *label = lv_label_create(obj1, NULL);
    lv_obj_align(label, NULL, LV_ALIGN_CENTER, 0, 0);       /*Align the label to the middle*/

    /*Create a bar*/
    lv_obj_t *bar1 = lv_bar_create(lv_scr_act(), NULL);
    lv_bar_set_style(bar1, LV_BAR_STYLE_INDIC, &style1);    /*Modify the indicator's style*/
    lv_bar_set_value(bar1, 70);                             /*Set the bar's value*/

主题
^^^^

主题是一个样式集，其中包含每种对象类型所需的样式。例如：按钮的 5
种样式，用于描述按钮的 5
种可能状态。更具体地说，主题是一个结构变量，它包含许多 ``lv_style_t *``
字段。查看\ `现有主题 <https://littlevgl.com/themes>`__\ 。例如：对于按钮有

::

    theme.btn.rel       /*Released button style*/
    theme.btn.pr        /*Pressed button style*/
    theme.btn.tgl_rel   /*Toggled released button style*/
    theme.btn.tgl_pr    /*Toggled pressed button style*/
    theme.btn.ina       /*Inactive button style*/

主题可以通过以下方式初始化： ``lv_theme_xxx_init(hue，font)``\ 。其中
``xxx`` 是主题的名称，\ ``hue`` 是 ``HSV``
颜色空间（0..360）的色调值，\ ``font`` 是主题中应用的字体（为 ``NULL``
时使用默认字体：\ ``LV_FONT_DEFAULT``\ ）。

颜色
~~~~

颜色模块处理所有颜色相关的功能，例如：改变颜色深度、从十六进制代码创建颜色、颜色深度之间的转换、混合颜色等。

以下变量类型由颜色模块定义： - ``lv_color1_t``
存储单色。为了兼容性，它还有 R，G，B 字段，但它们总是相同的（1 字节） -
``lv_color8_t`` 存储 8 位颜色（1 字节），R（3 位），G（3 位），B（2 位）
- ``lv_color16_t`` 存储 16 位颜色（2 字节），R（5 位），G（6 位），B（5
位） - ``lv_color24_t`` 存储 24 位颜色（4 字节），R（8 位），G（8
位），B（8 位） - ``lv_color_t`` 根据颜色深度设置为 ``color1/8/16/24_t``
- ``lv_color_int_t`` 根据颜色深度设置为 ``uint8_t``\ ，\ ``uint16_t`` 或
``uint32_t`` 类型。用于从普通数字构建颜色数组 - ``lv_opa_t`` 使用
``uint8_t`` 类型来描述不透明度

``lv_color_t``\ ，\ ``lv_color1_t``\ ，
``lv_color8_t``\ ，\ ``lv_color16_t`` 和 ``lv_color24_t``
类型有四个字段： - ``red``\ ：red channel - ``green``\ ： green channel
- ``blue``\ ： blue channel - ``red + green + blue``

可以通过设置定义在 ``lv_conf.h`` 中的 ``LV_COLOR_DEPTH`` 为
1（单色），8，16 或 24 来设置当前颜色深度。

为了描述不透明度，\ ``lv_opa_t`` 类型被创建为 ``uint8_t`` 的包装器： -
``LV_OPA_TRANSP`` 值为：0 表示完全透明 - ``LV_OPA_10``
值为：25，表示仅覆盖一点颜色 - ``LV_OPA_20~OPA_80`` 依次变化 -
``LV_OPA_90`` 值为：229，表示近乎完全覆颜色 - ``LV_OPA_COVER``
值为：255，表示完全覆盖颜色

字体
~~~~

在 LittlevGL
中，字体是位图和其他描述符（用于存储字母（字形）的图像和一些附加信息）。字体存储在
``lv_font_t`` 变量中，可以在样式的 ``text.font`` 字段中设置。

字体具有 bpp（Bit-Per-Pixel）特性，bpp
用于描述字体中像素的位数。像素值确定像素的不透明度。使用这种方式字母的图像（特别是在边缘上）可以是平滑和均匀的。bpp
值可以取 1，2，4 和 8（值越高意味着更好的效果）。bpp
还会影响存储字体所需的内存大小。例如：与 bpp=1 相比，bpp=4
使字体的内存大小增加 4 倍。

内置字体
^^^^^^^^

有几种内置字体可以通过定义在 ``lv_conf.h`` 中的 ``USE_LV_FONT _...``
使能。这些内置字体有不同大小： - 10 px - 20 px - 30 px - 40 px

可以使用 1，2，4 或 8 值来使能字体以设置其 bpp（例如
``USE_LV_FONT_DEJAVU_20 4``\ ）。内置字体在每种 bpp 中都有多个字符集： -
ASCII（Unicode 32..126） - Latin supplement（Unicode 160..255） -
Cyrillic（Unicode 1024..1279）

内置字体使用 ``Dejavu`` 字体。内置字体是全局变量，名称如下： -
``lv_font_dejavu_20``\ （20 px ASCII 字体） -
``lv_font_dejavu_20_latin_sup``\ （20 px Latin supplement 字体） -
``lv_font_dejavu_20_cyrillic``\ （20 px Cyrillic 字体）

支持 Unicode
^^^^^^^^^^^^

LittlevGL 支持 ``UTF-8`` 编码的 ``Unicode``
字符。需要配置编辑器以将代码/文本保存为 ``UTF-8`` 并在 ``lv_conf.h``
中使能 ``LV_TXT_UTF8``\ 。如果未使能 ``LV_TXT_UTF8``\ ，则只能使用
``ASCII`` 字体和符号（请参阅下面的符号）。

您可以指定更多字体来创建更大的字符集。为此，请选择基本字体（通常是 ASCII
字符）并向其添加扩展字体：\ ``lv_font_add(child, parent)``\ 。内置字体已添加到相同大小的
ASCII 字符中。例如：如果在 ``lv_conf.h`` 中使能了
``USE_LV_FONT_DEJAVU_20`` 和
``USE_LV_FONT_DEJAVU_20_LATIN_SUP``\ ，则在使用 ``lv_font_dejavu_20``
时可以呈现 “abcÁÖÜ” 文本。

符号字体
^^^^^^^^

符号字体是包含符号而不是字母的特殊字体。还有内置的符号字体，它们也被分配给具有相同大小的
``ASCII`` 字体。在文本中，可以引用符号，如
``SYMBOL_LEFT``\ ，\ ``SYMBOL_RIGHT``
等。您可以将这些符号名称与字符串混合使用：\ ``lv_label_set_text(label1，“Right”SYMBOL_RIGHT);``\ 。符号也可以在没有使能
``UTF-8`` 的情况下使用。

以下列表显示了 LittlevGL 现有的符号：

.. figure:: ../../_static/hmi_solution/littlevgl/symbols.jpg
    :align: center


    图 6. 符号字体



添加字体
^^^^^^^^

如果要向库中添加新字体，可以使用\ `在线字体转换器工具 <https://littlevgl.com/ttf-font-to-c-array>`__\ 。从
TTF 文件创建一个 C 数组，将其复制到项目中。可以指定高度、字符范围和
bpp。您可以枚举字符以仅将它们包含在最终字体中（可选）。要使用生成的字体，请使用
``LV_FONT_DECLAER(my_font_name)`` 函数声明它。

动画
~~~~

可以使用动画函数在开始值和结束值之间自动更改变量（动画），动画函数的原型是
``void func(void * var, int32_t value)``\ 。通过定期调用动画函数（使用相应的参数）来产生动画。要创建动画，您必须初始化
``lv_anim_t`` 变量（\ ``lv_anim.h`` 中有模板）。

您可以确定动画的路径。在大多数简单的情况下，它是线性变化的。目前有两条内置路径：
- ``lv_anim_path_linear`` 线性动画 - ``lv_anim_path_step``
在最后一步改变

默认情况下，您可以设置动画时间。使用
``lv_anim_speed_to_time(speed, start, end)``
函数可以计算以一定速度从起始点到达结束点所需的时间（以毫秒为单位）。例如：\ ``anim_speed_to_time(20, 0, 100)``
将返回 5000（毫秒）。

可以同时在同一个变量上应用多个不同的动画。但是只有一个动画可以与给定的变量和函数对一起存在。您可以通过
``lv_anim_del(var, func)`` 删除动画变量和动画函数。

输入设备
~~~~~~~~

要使用创建的对象进行交互，需要输入设备。例如：触摸板、鼠标、键盘或编码器。注册输入设备驱动程序时，LittlevGL
会添加一些额外信息，以更详细地描述输入设备的状态。当发生用户动作（例如：按下按钮）并且触发动作（回调）函数时，总是存在触发该动作的输入设备。您可以使用
``lv_indev_t * indev = lv_indev_get_act()`` 函数获取此输入设备。

对象组
~~~~~~

可以对对象进行分组，以便在没有触摸板或鼠标的情况下轻松控制它们。它允许你使用以下输入设备在对象之间移动：
- 键盘或辅助键盘 - 硬件按钮 - 编码器

首先，您必须使用 ``lv_groupt_t *group = lv_group_create()``
函数创建一个对象组，并使用 ``lv_group_add_obj(group, obj)``
函数向对象组中添加对象。在一个对象组中总是有一个处于选中状态的对象。所有按钮事件都将通知给当前处于选中状态的对象。

``LV_INDEV_TYPE_KEYPAD``
类型的输入设备才能在对象组中的对象之间移动（更改处于选中状态的对象）并与它们交互。可以在该类型的输入设备的读取函数中告诉
LittlevGL 库按下或释放哪个键。此外，您必须使用
``lv_indev_set_group(indev, group)``
函数将对象组与输入设备绑定。在读取函数中可以使用一些特殊的控制字符： -
``LV_GROUP_KEY_NEXT`` 移动到下一个对象 - ``LV_GROUP_KEY_PREV``
移动到上一个对象 - ``LV_GROUP_KEY_UP``
递增当前值、向上移动或单击选中的对象（例如：向上移动意味着选择上面的列表元素）
- ``LV_GROUP_KEY_DOWN``
递减当前值或向下移动选中的对象（例如：向下移动意味着选择下面的列表元素）
- ``LV_GROUP_KEY_RIGHT`` 递增选中对象的值或单击选中的对象 -
``LV_GROUP_KEY_LEFT`` 递减选中对象的值 - ``LV_GROUP_KEY_ENTER``
单击当前选中的对象或选中的元素（例如：list 元素） - ``LV_GROUP_KEY_ESC``
退出当前选中的对象（例如：下拉列表）

选中对象的样式由函数修改。默认情况下，它会使对象的颜色变为橙色，但也可以使用
``void lv_group_set_style_mod_cb(group, style_mod_cb)``
函数在每个对象组中指定自己的样式更新函数。\ ``style_mod_cb`` 需要一个
``lv_style_t *``
参数，该参数是选中对象样式的副本。在回调函数中，可以将一些颜色混合到当前颜色，并修改参数但不允许设置修改大小相关的属性（如
letter\_space，padding 等）。

绘图和渲染
~~~~~~~~~~

在 LittlevGL
中，您可以只关心图形对象，而不关心绘图是如何进行的。您可以设置对象的大小、位置或其他属性，LittlevGL
库将重新绘制。但是，您应该知道基本的绘制方法，以了解您应该选择哪一个。

Buffered 和 Unbuffered
^^^^^^^^^^^^^^^^^^^^^^

1. Unbuffered

   无缓存模式（Unbuffered）将像素直接发送到显示器（帧缓冲区）。因此，在绘制过程中可能会出现一些闪烁，因为首先必须绘制背景，然后绘制背景上的对象。因此，在使用滚动、拖动和动画时，这种模式不适用。另一方面，它内存占用的最少，因为不需要额外的图形缓冲区。通过在
   ``lv_conf.h`` 中设置 ``LV_VDB_SIZE`` 为 0 并注册 ``disp_map`` 和
   ``disp_fill`` 函数以使用无缓存模式。

2. Buffered

   缓存模式（Buffered）类似于双缓存。然而，LittlevGL 的 Buffered
   绘制算法仅使用一个帧缓冲区（显示器）和一个称为虚拟显示缓冲区（VDB）的小型图形缓冲区。对于
   VDB 大小，屏幕大小的 1/10 通常就足够了。例如：使用 16 位颜色的
   320×240 屏幕，仅需要额外的 15 KB RAM。

   使用缓存模式绘制时不会出现闪烁，因为图像首先在内存（VDB）中创建，因此可以使用滚动，拖动和动画。此外，它还可以使用其他图形效果，如抗锯齿、透明度（不透明度）和阴影。通过在
   ``lv_conf.h`` 中设置 ``LV_VDB_SIZE``>\ ``LV_HOR_RES`` 并注册
   ``disp_flush`` 函数使用缓存模式。

   在缓存模式下，您可以使用双 VDB 并行执行渲染到一个
   VDB，并将像素从其他位置复制到帧缓冲区。副本应该使用 DMA
   或其他硬件加速在后台工作，让 CPU 做其他事情。在 ``lv_conf.h``
   中，\ ``LV_VDB_DOUBLE`` 为 1 使能双 VDB 功能。

3. Buffered vs Unbuffered

   请记住，使用无缓存模式绘制的速度并不一定比使用缓存快。在渲染过程中，某个像素可能会被多次重绘（例如：背景，按钮，文本按序渲染）。在无缓存模式下，LittlevGL
   库需要多次访问外部存储器或显示控制器，这比写入/读取内部 RAM 慢。

   下表总结了两种绘制方法之间的差异：

+-----------------+----------------------+--------------------+
|                 | Unbuffered drawing   | Buffered drawing   |
+=================+======================+====================+
| Memory usage    | No extra             | >1/10 screen       |
+-----------------+----------------------+--------------------+
| Quality         | Flickering           | Flawless           |
+-----------------+----------------------+--------------------+
| Anti-aliasing   | Not supported        | Supported          |
+-----------------+----------------------+--------------------+
| Transparency    | Not supported        | Supported          |
+-----------------+----------------------+--------------------+
| Shadows         | Not supported        | Supported          |
+-----------------+----------------------+--------------------+


抗锯齿
^^^^^^

在 ``lv_conf.h`` 中使能 ``LV_ANTIALIAS`` 打开抗锯齿功能。但仅在缓存模式
``(LV_VDB_SIZE>LV_HOR_RES)``
中支持抗锯齿。抗锯齿算法通过填充一些半透明像素（具有不透明度的像素）使线条和曲线（包括具有半径的角落）更平滑且均匀。

如字体部分所述，可以使用更高 bpp (Bit-Per-Pixel)
的字体来抗锯齿。这样，字体的像素不仅可以是 0 或
1，而且可以是半透明的。支持的 bpp-s 分别为 1，2，4 和 8。但 bpp
较高的字体需要更多的 ROM。

Little 控件介绍
~~~~~~~~~~~~~~~

基础对象 (lv\_obj)
^^^^^^^^^^^^^^^^^^

基础对象包含对象的最基本属性： - 坐标 - 父对象 - 子对象 - 样式 -
点击使能 - 拖动使能等属性

可以通过函数设置坐标、对象大小、对齐方式、父对象等。对齐方式有：

.. figure:: ../../_static/hmi_solution/littlevgl/align.jpg
    :align: center


    图 7. 对齐方式

当您使用 ``lv_obj_create(NULL，NULL)``\ 函数创建 Screen 时，可以使用
``lv_scr_load(screen1)`` 加载它。使用
``lv_scr_act()``\ 函数将返回指向当前 Screen 的指针。

自动生成的两层图层： - 顶层 - 系统层

它们是独立于 Screen 对象的，因此创建一个对象时，将会在 Screen
上显示。顶层位于 Screen
上的每个对象上方，系统层也位于顶层之上。您可以在顶层自由添加任何弹出窗口。但是系统层限于系统级事物（例如鼠标光标将在这里移动）。使用
``lv_layer_top()`` 和 ``lv_layer_sys()``
函数将返回指向顶层或系统层的指针。

Label (lv\_label)
^^^^^^^^^^^^^^^^^

标签是用于显示文本的基本对象，文本大小没有限制。可以使用
``lv_label_set_text()``
函数修改的文本。标签对象的大小可以自动扩展到文本大小，或者可以选择以下方式：
- ``LV_LABEL_LONG_EXPAND``\ ： 将对象大小扩展为文本大小 -
``LV_LABEL_LONG_BREAK``\ ： 保持对象宽度，展开对象高度 -
``LV_LABEL_LONG_DOTS``\ ： 保持对象大小，截取文本并在最后一行写入点 -
``LV_LABEL_LONG_SCROLL``\ ：
展开对象大小并滚动父对象上的文本（移动标签对象） -
``LV_LABEL_LONG_ROLL``\ ： 保持大小并只滚动文本（不是对象）

Image (lv\_img)
^^^^^^^^^^^^^^^

图像是显示图像的基本对象。 为了提供最大的灵活性，图像的来源可以是： -
代码中的变量（带有像素的 C 数组） - 外部存储的文件（如在 SD 卡上） -
带符号的文字

要从 PNG，JPG 或 BMP
图像生成像素数组，请使用\ `在线图像转换器工具 <https://littlevgl.com/image-to-c-array>`__\ ，并使用其指针设置转换后的图像：
``lv_img_set_src(img1，＆converted_img_var);``\ 。

要使用外部文件，您还需要使用\ `在线转换器工具 <https://littlevgl.com/image-to-c-array>`__\ 转换图像文件，但现在应选择二进制输出格式。要了解如何处理
LittlevGL
的外部图像文件，请查看\ `教程 <https://github.com/littlevgl/lv_examples/tree/master/lv_tutorial/6_images>`__\ 。

您也可以使用定义在 ``lv_symbol_def.h``
中的符号。在这种情况下，图像将根据样式中指定的字体呈现为文本。它可以使用轻量级单色“字母”而不是真实图像。您可以像这样使用符号：
``lv_img_set_src(img1，SYMBOL_OK);``\ 。

Line (lv\_line)
^^^^^^^^^^^^^^^

线对象能够在一组点之间绘制直线。这些点必须存储在 ``lv_point_t``
数组中，并通过 ``lv_line_set_points(lines，point_array，point_num)``
函数传递给对象。

可以根据点自动设置线对象的大小。您可以使用
``lv_line_set_auto_size(line，true)``
函数使能自动设置对象大小。如果使能，那么当设置点时，对象的宽度和高度将根据最大值（\ ``max.x``
和 ``max.y``\ ）进行更改。默认情况下使能自动设置对象大小。

Container (lv\_cont)
^^^^^^^^^^^^^^^^^^^^

容器是类似矩形的对象，具有一些特殊功能。您可以在容器上应用布局以自动布局其子对象。布局间距来自
``style.body.padding.hor/ver/inner`` 属性。可选的布局： -
``LV_CONT_LAYOUT_OFF``\ ：不要让子对象自动布局 -
``LV_CONT_LAYOUT_CENTER``\ ：将子对象与列中的中心对齐，并且它们之间保持间距为
``pad.inner`` -
``LV_CONT_LAYOUT_COL_L``\ ：对齐左对齐列中的子对象。左侧间距为
``pad.hor``\ ，顶部间距为 ``pad.ver``\ ，子对象之间间距为 ``pad.inner``
- ``LV_CONT_LAYOUT_COL_M``\ ：对齐居中列中的子对象。顶部间距为
``pad.ver``\ ，子对象之间间距为 ``pad.inner`` -
``LV_CONT_LAYOUT_COL_R``\ ：对齐右对齐列中的子对象。右侧间距为
``pad.hor``\ ，顶部间距为 ``pad.ver``\ ，子对象之间间距为 ``pad.inner``
- ``LV_CONT_LAYOUT_ROW_T``\ ：对齐顶部对齐行中的子对象。左侧间距为
``pad.hor``\ ，顶部间距为 ``pad.ver``\ ，子对象之间间距为 ``pad.inner``
- ``LV_CONT_LAYOUT_ROW_M``\ ：对齐居中行中的子对象。左侧间距为
``pad.hor``\ ，子对象之间间距为 ``pad.inner`` -
``LV_CONT_LAYOUT_ROW_B``\ ：对齐底部对齐行中的子对象。左侧间距为
``pad.hor``\ ，底部间距为 ``pad.ver``\ ，子对象之间间距为 ``pad.inner``
-
``LV_CONT_LAYOUT_PRETTY``\ ：尽可能将对象放在一行中。在子对象之间平均划分每行。顶部间距为
``pad.ver``\ ，行间距为 ``pad.inner`` - ``LV_CONT_LAYOUT_GRID``\ ：与
``PRETTY LAYOUT`` 类似，但不是平均划分一行，而是子对象之间间距为
``pad.hor``

您可以使能自动调整功能，该功能会自动设置容器大小以包括所有子对象。在左侧和右侧间距保持为
``pad.hor``\ ，在顶部和底部间距保持为 ``pad.ver``\ 。可以使用
``lv_cont_set_fit(cont，true，true)``
函数使能水平、垂直或双向自动调整。第二个参数是水平方向，第三个参数是垂直方向。

Page (lv\_page)
^^^^^^^^^^^^^^^

页面由两个容器组成的：底部是背景，顶部是可滚动的。如果您在页面上创建子对象，它将自动移动到可滚动容器。如果可滚动容器变大，则可以通过拖动滚动背景。默认情况下，使能垂直方向可滚动的自动调整属性，因此其高度将增加以包括其所有子项。可滚动的宽度自动调整为背景宽度（减去背景的水平填充）。

有以下四种方式显示滚动条： - ``LV_SB_MODE_OFF``\ ：从不显示滚动条 -
``LV_SB_MODE_ON``\ ：始终显示滚动条 -
``LV_SB_MODE_DRAG``\ ：拖动页面时显示滚动条 -
``LV_SB_MODE_AUTO``\ ：当可滚动容器足够大时显示滚动条

您可以通过 ``lv_page_glue_obj(child, true)``
函数将子对象粘贴到页面上。在这种情况下，您可以通过拖动子对象来滚动页面。您可以使用以下命令聚焦到页面上的对象：\ ``lv_page_focus(page, child, anim_time)``\ 。它将移动可滚动容器以显示孩子。

可以使用 ``lv_page_set_rel_action(page, my_rel_action)`` 和
``lv_page_set_pr_action(page, my_pr_action)``
为页面设置释放和按下操作。该操作也可以从背景和可滚动对象触发。

Window (lv\_win)
^^^^^^^^^^^^^^^^

窗口是最复杂的容器类对象之一。它们由两个主要部分构成：顶部的标题容器和标题下面的内容页面。

在标题容器上有标题，可以通过以下方式修改：\ ``lv_win_set_title(win，“New title”)``\ 。标题始终继承标题容器的样式。

您可以使用以下命令在标题的右侧添加控制按钮：\ ``lv_win_add_btn(win，“U：/ close”，my_close_action)``\ 。第二个参数是图像文件路径，第三个参数是释放按钮时的回调函数。您可以将符号用作图像，如：\ ``lv_win_add_btn(win，SYMBOL_CLOSE，my_close_action)``\ 。

Tab view (lv\_tabview)
^^^^^^^^^^^^^^^^^^^^^^

选项卡对象可用于组织选项卡中的容器。您可以使用
``lv_tabview_add_tab(tabview, "Tab name")``
添加新选项卡。它将返回一个指向 Page
对象的指针，您可以在其中添加选项卡的内容。

要选择标签，您可以： - 在标题部分单击它 - 水平滑动 - 使用
``lv_tabview_set_tab_act(tabview，id，anim_en)`` 函数

使用 ``lv_tabview_set_sliding(tabview，false)``
函数禁用手动滑动，动画时间可以使用
``lv_tabview_set_anim_time(tabview，anim_time)`` 函数调整。使用
``lv_tabview_set_tab_load_action(tabview，action)``
函数给选项卡添加回调函数。

Bar (lv\_bar)
^^^^^^^^^^^^^

Bar
对象有两个主要部分：一个背景，它是对象本身，一个游标，其形状类似于背景，但其宽度/高度可以调整。根据宽度/高度比，Bar
的方向可以是垂直的或水平的。

可以通过以下方式设置值：\ ``lv_bar_set_value(bar，new_value)``\ 。该值在范围（最小值和最大值）中，可以使用以下值修改范围：\ ``lv_bar_set_range(bar，min，max)``\ 。默认范围是：1~100。使用
``lv_bar_set_value_anim(bar，new_value，anim_time)``
函数可以设置从当前值改变到设置的值的动画时间。

Line meter (lv\_lmeter)
^^^^^^^^^^^^^^^^^^^^^^^

Line Meter 对象包含一些绘制比例的径向线。使用
``lv_lmeter_set_value(lmeter，new_value)``
函数设置值时，刻度的比例部分将重新着色。

使用 ``lv_lmeter_set_range(lmeter，min，max)``
函数设置线路表的范围，使用
``lv_lmeter_set_scale(lmeter，angle，line_num)``
函数设置刻度的角度和线数量。默认角度为 240，默认线数量为 31。

.. figure:: ../../_static/hmi_solution/littlevgl/line-meter-lv_lmeter.jpg
    :align: center


    图 8. Line meter

Gauge (lv\_gauge)
^^^^^^^^^^^^^^^^^

仪表是带刻度标签和针头的对象。您可以使用
``lv_gauge_set_scale(gauge，angle，line_num，label_cnt)``
函数来调整角度以及刻度线和标签的数量。默认设置为：220 度角，6
个刻度标签和 21 条线。

仪表可以显示多个针头。使用
``lv_gauge_set_needle_count(gauge，needle_num，color_array)``
函数设置针数和每个针的颜色数组（数组必须是静态或全局变量）。

要设置临界值，请使用
``lv_gauge_set_critical_value(gauge，value)``\ 。在临界值之后，刻度颜色将变为
``line.color``\ 。（默认值：80）仪表的范围可以通过
``lv_gauge_set_range(gauge, min, max)`` 函数设置。

Chart (lv\_chart)
^^^^^^^^^^^^^^^^^

图表具有类似矩形的背景，具有水平和垂直分割线。您可以通过
``lv_chart_add_series(chart, color)``
函数向图表添加任意数量的数据源。数据源为 ``lv_chart_series_t``
结构，该结构包含所选颜色和数据数组。

您有几个选项来设置数据源： - 在数组中手动​​设置值，如
``ser1->points[3] = 7``\ ，并使用 ``lv_chart_refresh(chart)`` 刷新图表 -
使用 ``lv_chart_set_next(chart, ser, value)``
函数将所有数据移至左侧，并在最右侧位置设置新数据 -
使用以下命令将所有点初始化为给定值：\ ``lv_chart_init_points(chart, ser, value)``
-
使用以下命令设置数组中的所有点：\ ``lv_chart_set_points(chart, ser, value_array)``

有四种数据显示类型： -
``LV_CHART_TYPE_NONE``\ ：不显示点。如果您想添加自己的绘制方法，可以使用它
- ``LV_CHART_TYPE_LINE``\ ：在点之间绘制线条 -
``LV_CHART_TYPE_COL``\ ：绘制列 - ``LV_CHART_TYPE_POINT``\ ：绘制点

您可以使用 ``lv_chart_set_type(chart, TYPE)``
函数指定显示类型。\ ``LV_CHART_TYPE_LINE | LV_CHART_TYPE_POINT``
类型可用于绘制线和点。

Led (lv\_led)
^^^^^^^^^^^^^

LED 是矩形（或圆形）的对象。您可以使用
``lv_led_set_bright(led, bright)`` 设置亮度。亮度应介于 0（最暗）和
255（最亮）之间。

使用 ``lv_led_on(led)`` 和 ``lv_led_off(led)`` 函数将亮度设置为预定义的
ON 或 OFF 值。\ ``lv_led_toggle(led)`` 在 ON 和 OFF 状态之间切换。

Message box (lv\_mbox)
^^^^^^^^^^^^^^^^^^^^^^

消息框充当弹出窗口。它们是由背景，文本和按钮构成的。背景是一个容器对象，使能垂直方向自动调整以确保文本和按钮始终可见。

使用 ``lv_mbox_set_text(mbox, "My text")``
函数设置文本。要添加按钮，请使用
``lv_mbox_add_btns(mbox, btn_str, action)`` 函数。
在这里你可以指定按钮文本，并添加一个释放按钮时的回调函数。使用
``lv_mbox_start_auto_close(mbox, delay)`` 函数可以在延时 ``delay``
毫秒后自动关闭消息框。使用 ``lv_mbox_stop_auto_close(mbox)``
函数将禁用开始自动关闭。使用 ``lv_mbox_set_anim_time(mbox，anim_time)``
函数调整动画时间。

Text area (lv\_ta)
^^^^^^^^^^^^^^^^^^

文本区域是一个带有标签和光标的页面。您可以使用以下方法将文本或字符插入当前光标位置：
- ``lv_ta_add_char(ta，'c');`` -
``lv_ta_add_text(ta，“insert this text”);``

使用 ``lv_ta_set_text(ta, "New text")`` 函数更改整个文本。使用
``lv_ta_del()`` 函数删除当前光标位置左侧的字符。

可以使用 ``lv_ta_set_cursor_pos(ta, 10)``
函数直接修改光标位置，也可以单步执行： - ``lv_ta_cursor_right(ta)`` -
``lv_ta_cursor_left(ta)`` - ``lv_ta_cursor_up(ta)`` -
``lv_ta_cursor_down(ta)``

您可以使用 ``lv_ta_set_cursor_type(ta, LV_CURSOR_...)``
函数设置光标类型： - ``LV_CURSOR_NONE`` - ``LV_CURSOR_LINE`` -
``LV_CURSOR_BLOCK`` - ``LV_CURSOR_OUTLINE`` - ``LV_CURSOR_UNDERLINE``

你可以使用 ``LV_CURSOR_HIDDEN`` 隐藏光标。

使用　\ ``lv_ta_set_one_line(ta，true)`` 函数设置文本区域为一行。使用
``lv_ta_set_pwd_mode(ta，true)`` 函数使能密码模式。

Button (lv\_btn)
^^^^^^^^^^^^^^^^

按钮可以通过回调函数响应用户按下、释放或长按动作。您可以使用
``lv_btn_set_action(btn, ACTION_TYPE, callback_func)``
函数设置某个操作类型的回调函数： -
``LV_BTN_ACTION_CLICK``\ ：按下（点击）按钮后释放 -
``LV_BTN_ACTION_PR``\ ：按下按钮 - ``LV_BTN_ACTION_LONG_PR``\ ：长按按钮
- ``LV_BTN_ACTION_LONG_PR_REPEAT``\ ：长按按钮，定期触发此操作

按钮可以处于五种可能状态之一： - ``LV_BTN_STATE_REL``\ ：已释放状态 -
``LV_BTN_STATE_PR``\ ：已按下状态 -
``LV_BTN_STATE_TGL_REL``\ ：切换释放状态 -
``LV_BTN_STATE_TGL_PR``\ ：切换按下状态 -
``LV_BTN_STATE_INA``\ ：禁用状态

可以使用 ``lv_btn_set_toggle(btn, true)``
函数将按钮设置为触发按钮。在这种情况下，在释放时，按钮进入切换释放状态。可以使用
``lv_btn_set_state(btn，LV_BTN_STATE_TGL_REL)`` 函数手动设置按钮的状态。

按钮只能通过 ``lv_btn_set_state()``
函数手动进入禁用状态。在禁用状态下，不会调用任何操作。

与容器类似，按钮也有布局和自动调整： -
``lv_btn_set_layout(btn，LV_LAYOUT _...)`` 设置布局。默认为
``LV_LAYOUT_CENTER``\ 。因此，如果添加标签，它将自动与中间对齐。 -
``lv_btn_set_fit(btn，hor_en，ver_en)``
可以根据子对象自动设置按钮宽度、高度。

Button matrix (lv\_btnm)
^^^^^^^^^^^^^^^^^^^^^^^^

Button Matrix 对象可以根据描述符字符串数组显示多个按钮，称为
map。您可以使用 ``lv_btnm_set_map(btnm，my_map)`` 指定 map。

map 的声明看起来像
``const char * map [] = {“btn1”，“btn2”，“btn3”，“”}``\ 。请注意，最后一个元素必须是空字符串！

字符串的第一个字符可以是控制字符，用于指定一些属性： - bit 7..6 始终为
0b10，以区分控制字节和文本字符 - bit 5 禁用按钮 - bit 4 隐藏按钮 - bit 3
没有长按功能的按钮 - bit 2..0 相对宽度：与同一行中的按钮相比。 [1..7]

在 map
中使用“”进行换行：\ ``{“btn1”，“btn2”，“\ n”，“btn3”，“”}``\ 。每行重新计算按钮的宽度。

使用 ``lv_btnm_set_action(btnm，btnm_action)``
函数指定释放按钮时的回调函数。

Keyboard (lv\_kb)
^^^^^^^^^^^^^^^^^

正如它的名字所示，键盘对象提供了一个键盘来写文本。您可以为键盘指定文本区域以将单击的字符放在那里。调用
``lv_kb_set_ta(kb, ta)`` 函数指定文本区域。

键盘包含“Ok”和“Hide”按钮。可以调用 ``lv_kb_set_ok_action(kb, action)``
和 ``lv_kb_set_hide_action(kb, action)`` 函数指定 ``ok`` 和 ``hide``
按钮的回调函数。

指定的文本区域的光标可以由键盘管理：当键盘被指定时，前一个文本区域的光标将被隐藏，将显示新的光标。单击“OK”或“Hide”也将隐藏光标。光标管理器功能由
``lv_kb_set_cursor_manage(kb, true)`` 使能。默认值不使用键盘管理。

键盘有两种模式： - LV\_KB\_MODE\_TEXT：显示字母，数字和特殊字符 -
LV\_KB\_MODE\_NUM：显示数字，+/- 符号和点

调用 ``lv_kb_set_mode(kb, mode)`` 函数设置模式。默认值为
``LV_KB_MODE_TEXT``\ 。

可以调用 ``lv_kb_set_map(kb，map)``
为键盘指定新的映射（布局）。它的工作方式类似于按钮矩阵，因此控件字符可以添加到布局中设置按钮宽度和其他属性。请记住，使用以下关键字将与原始映射具有相同的效果：\ ``SYMBOL_OK``\ ，\ ``SYMBOL_CLOSE``\ ，\ ``SYMBOL_LEFT``\ ，\ ``SYMBOL_RIGHT``\ ，\ ``ABC``\ ，\ ``abc``\ ，\ ``Enter``\ ，\ ``Del``\ ，\ ``＃1``\ ，\ ``+/-``\ 。

List (lv\_list)
^^^^^^^^^^^^^^^

列表是由背景页面和按钮组成的。按钮包含可选的图标式图像（也可以是符号）和标签。当列表变得足够长时，它可以滚动。根据对象宽度将按钮的宽度设置为最大。按钮的高度根据内容自动调整。

可以使用 ``lv_list_add(list, "U:/img", "Text", rel_action)``
函数添加新的列表元素或使用
``lv_list_add(list, SYMBOL_EDIT, "Edit text")``
函数添加带符号图标的列表元素。该函数返回一个指向已创建的按钮的指针，以允许进一步配置。

使用 ``lv_list_get_btn_label(list_btn)`` 函数和
``lv_list_get_btn_img(list_btn)`` 函数来获取标签和列表按钮的图像。

在按钮的释放操作中，您可以调用 ``lv_list_get_btn_label(list_btn)``
函数获取按钮的文本。要删除列表元素，只需在 ``lv_list_add()``
的返回值上调用 ``lv_obj_del()`` 函数。可以调用 ``lv_list_up(list)`` 和
``lv_list_down(list)`` 函数在列表中手动移动。

可以使用 ``lv_list_focus(btn, anim_en)``
直接选中按钮。上/下/焦点移动的动画时间可以通过以下方式设置：\ ``lv_list_set_anim_time(list，anim_time)``\ 。

Drop down list (lv\_ddlist)
^^^^^^^^^^^^^^^^^^^^^^^^^^^

下拉列表允许您从选项列表中选择一个选项。下拉列表默认关闭，显示当前选定的文本。如果单击它，将打开此列表并显示所有选项。

将选项作为字符串使用 ``lv_ddlist_set_options(ddlist，options)``
函数传递给下拉列表。选项应以 ``\n`` 分隔。例如：“First”。

使用 ``lv_ddlist_set_selected(ddlist，id)`` 函数手动选择一个选项，其中
id 是选项的索引。使用 ``lv_ddlist_set_action(ddlist，my_action)``
函数设置回调函数。

默认情况下，列表的高度会自动调整以显示所有选项。使用
``lv_ddlist_set_fix_height(ddlist，h)`` 函数设置固定高度。

宽度也会自动调整。使用 ``lv_ddlist_set_hor_fit(ddlist，false)``
函数，并使用 ``lv_obj_set_width(ddlist，width)`` 函数手动设置宽度。

与具有固定高度的页面类似，下拉列表支持各种滚动条显示模式。可以使用\ ``lv_ddlist_set_sb_mode(ddlist，LV_SB_MODE _...)``
函数设置。

Drop Dawn List 打开/关闭动画时间由
``lv_ddlist_set_anim_time(ddlist，anim_time)`` 函数设置。

Roller (lv\_roller)
^^^^^^^^^^^^^^^^^^^

Roller
允许您通过简单地滚动从选项列表中选择一个选项。其功能类似于下拉列表。

使用 ``lv_roller_set_options(roller, options)``
函数设置选项列表。其中第二个参数为字符串，以 ``\n``
分隔。例如：“First”。使用 ``lv_roller_set_selected(roller，id)``
函数手动选择一个选项，其中 id 是选项的索引。使用
``lv_roller_set_action(roller，my_action)`` 函数设置回调函数。使用
``lv_roller_set_visible_row_count(roller，row_cnt)``
函数调整滚轴的高度，以设置可见选项的数量。

Roller 的宽度自动调整。可以使用 ``lv_roller_set_hor_fit(roller，false)``
函数禁止自动调整，并使用 ``lv_obj_set_width(roller, width)``
函数手动设置宽度。Roller 的打开/关闭动画时间由
``lv_roller_set_anim_time(roller，anim_time)`` 函数调整。

.. figure:: ../../_static/hmi_solution/littlevgl/roller-lv_roller.jpg
    :align: center


    图 9. Roller

Check box (lv\_cb)
^^^^^^^^^^^^^^^^^^

Check Box
对象是基于按钮的，其中包含一个按钮和一个标签，用于实现经典复选框。

使用 ``lv_cb_set_text(cb，“New text”)`` 函数修改文本。使用
``lv_cb_set_action(cb, action)`` 函数设置回调函数。可以使用
``lv_cb_set_checked(cb, state)`` 函数手动选中或取消选中。

Slider (lv\_slider)
^^^^^^^^^^^^^^^^^^^

滑条对象看起来像增加了一个旋钮的
Bar。可以拖动旋钮来设置值。滑块也可以是垂直的或水平的。

使用 ``lv_slider_set_value(slider，new_value)`` 函数设置初始值或使用
``lv_slider_set_value_anim(slider，new_value，anim_time)``
函数设置动画时间。

可以使用 ``lv_slider_set_range(slider，min，max)``
函数指定范围（最小值，最大值）。

当用户设置新的值时，可以通过 ``lv_slider_set_action(slider，my_action)``
函数设置回调函数。

旋钮有两种方式放置： - 在最小/最大值的背景内 - 在最小/最大值的边缘上

使用 ``lv_slider_set_knob_in(slider，true / false)``
函数在这两种之间进行选择。（默认值是 ``knob_in == false``\ ）

Switch (lv\_sw)
^^^^^^^^^^^^^^^

开关可用于打开/关闭某些东西。可以通过以下方式更改开关的状态： - 点击 -
滑动 - ``lv_sw_on(sw)``\ 和 ``lv_sw_off(sw)`` 函数

当用户使用开关时，可以使用 ``lv_sw_set_action(sw，my_action``
函数设置回调函数。

LittlevGL 使用
--------------

iot-solution 中已经做了一些驱动适配，驱动路径：
``components/hmi/gdrivers``\ 。

在基于 iot-solution 的工程中使用 LittlevGL 的步骤：

1. 搭建 iot-solution
   环境：\ `Preparation <https:404#preparation>`__
2. 在工程源代码中添加头文件 ``#include "iot_lvgl.h"``
3. 在 ``menuconfig`` 中使能 LittlevGL GUI
   （\ ``IoT Solution settings > IoT Components Management > HMI components > GUI Library Select > LittlevGL GUI Enable``\ ）
4. 在 ``menuconfig`` 中进行 LittlevGL GUI `相关配置 <#littlevgl-配置>`__
   （\ ``IoT Solution settings > IoT Components Management > HMI components > LittlevGL Settings``\ ）
5. 根据示例工程 ``lvgl_example`` 所示完成 LittlevGL 的初始化
6. 根据实际工程进行 GUI 的开发

LittlevGL 配置
~~~~~~~~~~~~~~

在 iot-solution 中进行 LittlevGL 配置主要有两种方式：

1. 在 ``menuconfig`` 中进行 LittlevGL 配置

   对于部分使用频率较高的配置选项，将其添加到 ``menuconfig``
   中以便于配置。例如：驱动配置、触摸屏使能、屏幕分辨率、旋转方向等。LittlevGL
   配置菜单位于
   ``IoT Solution settings > IoT Components Management > HMI components > LittlevGL Settings``\ 。

2. 修改 ``lv_conf.h`` 文件进行 LittlevGL 配置

   LittlevGL 所有项目的特定选项都在文件 ``lv_conf.h`` 中定义，该文件在
   ``esp-iot-solution/components/hmi/lvgl_gui/lv_conf.h``\ ，用户可自行修改。

``menuconfig`` 中 LittlevGL 的配置选项，如下图所示：

.. figure:: ../../_static/hmi_solution/littlevgl/lvgl_menuconfig.jpg
    :align: center


    图 10. LittlevGL menuconfig

1. 驱动配置

   在 LittlevGL Settings
   菜单中可以选择显示屏和触摸屏的驱动以及相关硬件接口配置，路径：\ ``Config Driver->Choose Touch Screen Driver``
   和 ``Config Driver->Choose Screen Driver``\ 。

2. 屏幕刷新配置

   在 LittlevGL Settings 菜单中可以选择屏幕刷新方式（使用 Buffered
   或不使用 Unbuffered
   两种方式），路径：\ ``Display Driver Mode``\ 。在菜单中也可以设置自动刷新间隔，路径：\ ``LittlevGL Driver Auto Flush Interval(ms)``\ 。

3. 触摸屏使能

   在 LittlevGL Settings
   菜单中可以选择使能或禁止触摸屏，路径：\ ``LittlevGL Touch Screen Enable``\ 。

4. 屏幕分辨率

   在 LittlevGL Settings
   菜单中可以选择显示屏的屏幕分辨率，路径：\ ``Config Driver->LittlevGL Screen Width (pixels)``
   和 ``Config Driver->LittlevGL Screen Height (pixels)``\ 。

5. 旋转方向

   在 LittlevGL Settings
   菜单中可以选择显示屏旋转的方向，路径：\ ``Choose Screen Rotate``\ 。

6. 自定义驱动

   在 LittlevGL Settings 菜单中可以将自定义的驱动组件添加到 LittlevGL
   的编译路径中（此时只编译自定义的驱动，不编译 iot-solution
   提供的驱动组件），路径：\ ``Use Custom Driver Defined By Users``\ 。

FAQs
----

1. ``.dram0.bss`` will not fit in :literal:`region dram0\_0\_seg` or :literal:`region dram0_0_seg` overflowed by 10072 bytes

   由于 LittlevGL 更新，增加了 ``.bss``
   代码量，如果编译时出现这个问题，可以在 ``lv_conf.h``
   文件中，将没有使用的主题(theme)、字体(font)、对象(objects)关掉，例如：程序中只使用默认主题，那么我们可以将其他的主题都关掉：

   .. code:: c

       /*================
       *  THEME USAGE
       *================*/
       #define LV_THEME_LIVE_UPDATE    0       /*1: Allow theme switching at run time. Uses 8..10 kB of RAM*/

       #define USE_LV_THEME_TEMPL      0       /*Just for test*/
       #define USE_LV_THEME_DEFAULT    1       /*Built mainly from the built-in styles. Consumes very few RAM*/
       #define USE_LV_THEME_ALIEN      0       /*Dark futuristic theme*/
       #define USE_LV_THEME_NIGHT      0       /*Dark elegant theme*/
       #define USE_LV_THEME_MONO       0       /*Mono color theme for monochrome displays*/
       #define USE_LV_THEME_MATERIAL   0       /*Flat theme with bold colors and light shadows*/
       #define USE_LV_THEME_ZEN        0       /*Peaceful, mainly light theme */
       #define USE_LV_THEME_NEMO       0       /*Water-like theme based on the movie "Finding Nemo"*/

   类似，我们可以将其他不使用功能关掉。


