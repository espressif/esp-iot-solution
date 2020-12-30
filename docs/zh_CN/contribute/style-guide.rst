esp-iot-solution 编码规范
=========================

总体原则
--------

-  简洁明了，结构清晰
-  统一风格，易于维护
-  充分注释，易于理解
-  `继承 ESP-IDF 已有规范 <https://docs.espressif.com/projects/esp-idf/en/latest/esp32/contribute/style-guide.html>`_
-  继承第三方代码已有规范

目录结构
--------

-  components：总体上按照功能分类，大类下如果存在多级目录，应包含一个\ ``README``
   做综述和索引；
-  docs：\ ``rst`` 格式文档，包括各个组件的使用指南，API 说明；
-  examples：总体上按照与组件对应的功能分类；
-  tools：CI 脚本、调试工具。

头文件
------

-  尽量每一 ``.c`` 对应一个同名的 ``.h`` 文件；

-  单个组件存在多个 ``.h`` ，主要对外 ``.h``\ 命名尽量与组件名保持一致；

-  头文件中主要放函数声明，不放函数实现；

-  尽量不在头文件定义任何形式的变量；

-  头文件应按照注释规范，对函数接口进行充分注释;

-  添加宏定义避免重复引用，宏定义名为大写的头文件名加下划线填充：

.. code:: c

    #ifndef _IOT_I2C_BUS_H_
    #define _IOT_I2C_BUS_H_

    #endif


-  函数声明添加 extern "C" 修饰，支持 C/C++ 混合编程：

.. code:: c

    #ifdef __cplusplus
    extern "C"
    {
    #endif

    //c code

    #ifdef __cplusplus
    }
    #endif



注释
----

-  安装 VSCODE 插件 `Doxygen Documentation
   Generator <https://marketplace.visualstudio.com/items?itemName=cschlosser.doxdocgen>`__
   可自动生成注释框架；

-  注释中避免使用单词缩写；

-  函数声明处注释需要描述函数功能、性能或用法，输入和输出参数、函数返回值说明。

自动生成的注释框架：

.. code:: c

    /**
    * @brief 
    * 
    * @param port 
    * @param conf 
    * @return i2c_bus_handle_t 
    */
    i2c_bus_handle_t iot_i2c_bus_create(i2c_port_t port, i2c_config_t* conf);


补充信息和参数方向：

.. code:: c

    /**
    * @brief Create an I2C bus instance then return a handle if created successfully. 
    * @note Each I2C bus works in a singleton mode, which means for an i2c port only one group parameter works. When
    * iot_i2c_bus_create is called more than one time for the same i2c port, following parameter will override the previous one.
    * 
    * @param[in] port I2C port number
    * @param[in] conf Pointer to I2C parameters
    * @return i2c_bus_handle_t Return the I2C bus handle if created successfully, return NULL if failed. 
    */
    i2c_bus_handle_t iot_i2c_bus_create(i2c_port_t port, i2c_config_t* conf);
  

-  版权声明注释（第三方代码，请保留版权声明信息）

.. code:: c

    // Copyright 2019-2020 Espressif Systems (Shanghai) PTE LTD
    //
    // Licensed under the Apache License, Version 2.0 (the "License");
    // you may not use this file except in compliance with the License.
    // You may obtain a copy of the License at

    //     http://www.apache.org/licenses/LICENSE-2.0
    //
    // Unless required by applicable law or agreed to in writing, software
    // distributed under the License is distributed on an "AS IS" BASIS,
    // WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    // See the License for the specific language governing permissions and
    // limitations under the License.

函数规范
--------

-  多处重复使用的代码尽量设计为函数；
-  作用域仅限于当前文件的函数必须声明为静态 ``static``\ ；
-  设计使用静态全局变量、静态局部变量的函数时，需要考虑重入问题；
-  尽量在一个固定函数中操作静态全局变量；
-  如果函数存在重入或线程安全问题，需在注释中说明；
-  同一组件内的公有函数名，应保持同一前缀；
-  函数名统一使用\ ``snake_case``\ 格式，只使用小写字母，单词之间加
   ``_`` ;
-  函数命名指引（应保持与已有代码风格一致，不严格约束）：

+----------------------------+--------------------------------------------------------------+---------------------------------------------------------------------------+
| 函数名格式                 | 函数示例                                                     | 说明                                                                      |
+============================+==============================================================+===========================================================================+
| iot\_type\_xxx             | iot\_sensor\_xxx; iot\_board\_xxx; iot\_storage\_...         | 高度抽象的 iot 组件                                                       |
+----------------------------+--------------------------------------------------------------+---------------------------------------------------------------------------+
| type\_xxx                  | imu\_xxx; light\_xxx; eeprom\_xxx                            | 对一类外设的抽象                                                          |
+----------------------------+--------------------------------------------------------------+---------------------------------------------------------------------------+
| name\_xxx                  | mpu6050\_xxx;                                                | 底层 driver，由于可能来自第三方，不约束函数名                             |
+----------------------------+--------------------------------------------------------------+---------------------------------------------------------------------------+
| xxx\_creat / xxx\_delete   |                                                              | 创建和销毁                                                                |
+----------------------------+--------------------------------------------------------------+---------------------------------------------------------------------------+
| xxx\_read / xxx\_write     |                                                              | 数据操作                                                                  |
+----------------------------+--------------------------------------------------------------+---------------------------------------------------------------------------+
+----------------------------+--------------------------------------------------------------+---------------------------------------------------------------------------+

变量规范
--------

-  避免使用全局变量，可声明为静态全局变量，使用 ``get_`` ``set_``
   等接口进行变量操作；
-  作用域仅限于当前文件的变量必须声明为静态变量 ``static``\ ；
-  静态全局变量请添加 ``g_`` 前缀，静态局部变量请添加 ``s_`` 前缀；
-  局部变量设计大小时，应考虑栈溢出的问题；
-  任何变量定义时，必须赋初值；
-  变量功能要明确，避免将单一变量做多个用途；
-  句柄类型变量，在对象销毁后，应重新赋值为 NULL;
-  变量统一使用\ ``snake_case``\ 格式，只使用小写字母，单词之间加 ``_``
   ;
-  避免不必要的缩写，例如 ``data`` 不必缩写为 ``dat``\ ；
-  变量应尽量使用有意义的词语，或者已经达成共识的符号或\ `词语缩写 <https://github.com/kisvegabor/abbreviations-in-code>`__\ ；
-  变量命名指引：

+----------------+-----------------------------------------------------------------------------------+-----------------------------------------+
| 类型           | 规范                                                                              | 示例                                    |
+================+===================================================================================+=========================================+
| 全局变量       | 避免使用                                                                          | x                                       |
+----------------+-----------------------------------------------------------------------------------+-----------------------------------------+
| 静态全局变量   | static 标识 ， g\_ 前缀，赋初值                                                   | static uint32\_t g\_connect\_num = 0;   |
+----------------+-----------------------------------------------------------------------------------+-----------------------------------------+
| 静态局部变量   | static 标识 ， s\_ 前缀，赋初值                                                   | static uint32\_t s\_connect\_num= 0;    |
+----------------+-----------------------------------------------------------------------------------+-----------------------------------------+
| 迭代计数变量   | 使用通用的 ``i`` ``j`` ``k``                                                      |                                         |
+----------------+-----------------------------------------------------------------------------------+-----------------------------------------+
| 常用缩写       | `abbreviations-in-code <https://github.com/kisvegabor/abbreviations-in-code>`__   | addr,buf ,cfg , cmd, , ctrl,            |
+----------------+-----------------------------------------------------------------------------------+-----------------------------------------+
+----------------+-----------------------------------------------------------------------------------+-----------------------------------------+

-  常用缩写列表

+--------+-----------+--------+---------------+---------+-------------+--------+--------------------------+
| 缩写   | 全称      | 缩写   | 全称          | 缩写    | 全称        | 缩写   | 全称                     |
+========+===========+========+===============+=========+=============+========+==========================+
| addr   | address   | id     | identifier    | len     | length      | ptr    | pointer                  |
+--------+-----------+--------+---------------+---------+-------------+--------+--------------------------+
| buf    | buffer    | info   | information   | obj     | object      | ret    | return                   |
+--------+-----------+--------+---------------+---------+-------------+--------+--------------------------+
| cfg    | command   | hdr    | header        | param   | parameter   | temp   | temporary、temperature   |
+--------+-----------+--------+---------------+---------+-------------+--------+--------------------------+
| cmd    | command   | init   | initialize    | pos     | position    | ts     | timestamp                |
+--------+-----------+--------+---------------+---------+-------------+--------+--------------------------+

类型定义
--------

-  使用加\ ``snake_case``\ 格式加 ``_t`` 后缀

.. code:: c

    typedef int signed_32_bit_t;

-  枚举应通过 typedef 通过以下方式定义

.. code:: c

    typedef enum { 
        MODULE_FOO_ONE,
        MODULE_FOO_TWO,
        MODULE_FOO_THREE 
    } module_foo_t;

格式和排版规范
--------------

该部分继承 `ESP-IDF
规范 <https://docs.espressif.com/projects/esp-idf/en/latest/esp32/contribute/style-guide.html>`_

1. 缩进
~~~~~~~

每个缩进层使用 **4
个空格**\ ，不要使用制表符进行缩进，将编辑器配置为每次按 tab 键时发出 4
个空格。

2. 垂直间隔
~~~~~~~~~~~

在函数之间放置一个空行，不要以空行开始或结束函数。

.. code:: c

    void function1()
    {
        do_one_thing();
        do_another_thing();
                                    // INCORRECT, don't place empty line here
    }
                                    // place empty line here
    void function2()
    {
                                    // INCORRECT, don't use an empty line here
        int var = 0;
        while (var < SOME_CONSTANT) {
            do_stuff(&var);
        }
    }

只要不严重影响可读性，最大行长度为 120 个字符。

3. 水平间隔
~~~~~~~~~~~

总是在条件和循环关键字之后添加单个空格

.. code:: c

    if (condition) {    // correct
        // ...
    }

    switch (n) {        // correct
        case 0:
            // ...
    }

    for(int i = 0; i < CONST; ++i) {    // INCORRECT
        // ...
    }

在二元操作符两端添加单个空格，一元运算符不需要空格，可以在乘法运算符和除法运算符之间省略空格。

.. code:: c

    const int y = y0 + (x - x0) * (y1 - y0) / (x1 - x0);    // correct

    const int y = y0 + (x - x0)*(y1 - y0)/(x1 - x0);        // also okay

    int y_cur = -y;                                         // correct
    ++y_cur;

    const int y = y0+(x-x0)*(y1-y0)/(x1-x0);                // INCORRECT

``.`` 和 ``->`` 操作符的周围不需要任何空格。

有时，在一行中添加水平间隔有助于提高代码的可读性。如下，可以添加空格来对齐函数参数:

.. code:: c

    gpio_matrix_in(PIN_CAM_D6,   I2S0I_DATA_IN14_IDX, false);
    gpio_matrix_in(PIN_CAM_D7,   I2S0I_DATA_IN15_IDX, false);
    gpio_matrix_in(PIN_CAM_HREF, I2S0I_H_ENABLE_IDX,  false);
    gpio_matrix_in(PIN_CAM_PCLK, I2S0I_DATA_IN15_IDX, false);

-  但是请注意，如果有人添加了一个新行，第一个参数是一个更长的标识符(例如PIN\_CAM\_VSYNC)，它将不适合。因为必须重新对齐其他行，这添加了无意义的更改。因此，尽量少使用这种对齐，特别是如果您希望稍后将新行添加到这列中。
-  不要使用制表符进行水平对齐，不要在行尾添加尾随空格。

4. 括号
~~~~~~~

函数定义的大括号应该在单独的行上

.. code:: c

    // This is correct:
    void function(int arg)
    {

    }

    // NOT like this:
    void function(int arg) {

    }

在函数中，将左大括号与条件语句和循环语句放在同一行

.. code:: c

    if (condition) {
        do_one();
    } else if (other_condition) {
        do_two();
    }

5. 注释
~~~~~~~

``//`` 用于单行注释。对于多行注释，可以在每行上使用 ``//``\ 或
``/ * * /`` 块注释。

虽然与格式没有直接关系，但下面是一些关于有效使用注释的注意事项。

-  不要使用一个注释来禁用某些功能

.. code:: c

    void init_something()
    {
        setup_dma();
        // load_resources();                // WHY is this thing commented, asks the reader?
        start_timer();
    }

-  如果不再需要某些代码，则将其完全删除。如果你需要，你可以随时在 git
   历史中查找这个文件。如果您因为临时原因而禁用某些调用，并打算在将来恢复它，则在相邻行上添加解释

.. code:: c

    void init_something()
    {
        setup_dma();
        // TODO: we should load resources here, but loader is not fully integrated yet.
        // load_resources();
        start_timer();
    }

-  ``#if 0 ... #endif``
   块也是如此。如果不使用，请完全删除代码块。否则，添加注释以解释为什么禁用该块。不要使用
   ``#if 0 ... #endif`` 或注释来存储将来可能需要的代码段。

-  不要添加有关作者和更改日期的琐碎注释。您总是可以查找谁使用 git
   修改了任何给定的行。例如，此注释在不添加任何有用信息的情况下，使代码混乱不堪：

.. code:: c

    void init_something()
    {
        setup_dma();
        // XXX add 2016-09-01
        init_dma_list();
        fill_dma_item(0);
        // end XXX add
        start_timer();
    }

6. 代码行的结束
~~~~~~~~~~~~~~~

commit 中只能包含以 LF（Unix风格）结尾的文件。

Windows 用户可以将 git 配置为在本地 checkout 是 CRLF（Windows
风格）结尾，通过设置 core.autocrlf 设置来 commit 时以 LF 结尾。 Github
有一个关于设置此选项的文档 。但是，由于 MSYS2 使用 Unix
样式的行尾，因此在编辑 ESP-IDF
源文件时，通常更容易将文本编辑器配置为使用 LF（Unix 样式）结尾。

如果您在分支中意外地 commit 了 LF 结尾，则可以通过在 MSYS2 或 Unix
终端中运行此命令将它们转换为 Unix（将目录更改为 IDF
工作目录，并预先检查当前是否已 checkout 正确的分支）：

.. code:: shell

    git rebase --exec 'git diff-tree --no-commit-id --name-only -r HEAD | xargs dos2unix && git commit -a --amend --no-edit --allow-empty' master

(请注意，这行代码将在 master
上重新建立基，并在最后更改分支名称以在另一个分支上建立基。)

要更新单个提交，可以运行

.. code:: shell

    dos2unix FILENAME

然后运行

.. code:: shell

    git commit --amend

7. 格式化代码
~~~~~~~~~~~~~

您可以使用 astyle 程序根据上述建议对代码进行格式化。

如果您正在从头开始编写一个文件，或者正在进行完全重写，请随意重新格式化整个文件。如果您正在更改文件的一小部分，不要重新格式化您没有更改的代码。这将帮助其他人检查您的更改。

要重新格式化文件，请运行

.. code:: shell

    tools/format.sh components/my_component/file.c

--------------

CMake 代码风格
--------------

-  缩进是 4 个空格
-  最大行长为 120 个字符。
   分割行时，请尝试尽可能集中于可读性（例如，通过在单独的行上配对关键字/参数对）。
-  不要在 endforeach()、endif() 等后面的可选括号中放入任何内容。
-  对命令、函数和宏名使用小写( with\_underscores )。
-  对于局部作用域的变量，使用小写字母( with\_underscores )。
-  对于全局作用域的变量，使用大写( WITH\_UNDERSCORES )。
-  其他，请遵循 `cmake-lint <https://github.com/richq/cmake-lint>`__
   项目的默认设置。

