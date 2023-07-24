.. _boards_component:

板级支持组件 (Boards)
======================

:link_to_translation:`en:[English]`

本文档主要介绍板级支持组件 (Boards) 的使用方法，该组件作为示例程序的公共组件，可向应用程序提供统一的引脚宏定义和与硬件无关的初始化操作，基于板级支持开发的应用程序可以同时兼容不同的开发板，具体功能如下：

1. 提供统一的引脚资源宏定义
2. 提供默认的外设配置参数
3. 提供统一的板级初始化接口
4. 提供开发板的硬件控制接口

Boards 组件的结构如下：

.. figure:: ../../_static/boards_diagram.png
    :align: center
    :width: 70%

    Boards 组件结构框图

* Boards 组件中包含以下内容：

    * ``board_common.h``，包含了公有 API 的函数声明
    * ``board_common.c``，包含了对公有 API 的函数实现（虚函数）
    * ``Kconfig.projbuild``，包含了公有的配置项

* 以开发板名称命名的子文件夹包含以下内容：

    * ``iot_board.h`` 提供了开发板的引脚资源定义，该开发板特有的自定义 API 函数声明
    * ``board.c`` 提供了公有 API 的用户实现（默认虚函数），自定义 API 函数实现
    * ``kconfig.in``，提供了该开发板特有的自定义配置项。

.. note::

    Boards 在 ``examples/common_components/boards`` 中提供。

使用方法
---------

1. 初始化开发板：在 ``app_main`` 使用 ``iot_board_init`` 初始化开发板，用户亦可在 ``menuconfig`` 中使用 :ref:`board_swith_and_config` 进行初始化配置；
2. 获取外设句柄：使用 ``iot_board_get_handle`` 和 ``board_res_id_t`` 获取外设资源的句柄，如果该外设未被初始化将返回 ``NULL``;
3. 使用句柄进行外设操作。

示例：

.. code:: c

    void app_main(void)
    {
        /*initialize board with default parameters,
        you can use menuconfig to choose a target board*/
        esp_err_t err = iot_board_init();
        if (err != ESP_OK) {
            goto error;
        }

        /*get the i2c0 bus handle with a board_res_id,
        BOARD_I2C0_ID is declared in board_res_id_t in each iot_board.h*/
        bus_handle_t i2c0_bus_handle = (bus_handle_t)iot_board_get_handle(BOARD_I2C0_ID);
        if (i2c0_bus_handle == NULL) {
            goto error;
        }

        /*
        * use initialized peripheral with handles directly,
        * no configurations required anymore.
        */
    }

.. _board_swith_and_config:

开发板切换和配置
----------------

基于 ``Boards`` 开发的应用程序，可以使用以下方法切换和配置开发板：

1. 选择目标开发板：在 ``menuconfig->Board Options->Choose Target Board`` 中选择一个开发板；
2. 配置开发板参数：``Board Common Options`` 中包含公有的开发板配置项，例如配置是否在开发板初始化期间初始化 ``i2c_bus``; ``XXX Board Options`` 中包含了该开发板特有的配置项，例如切换开发板供电状态等。
3. 使用 ``idf.py build flash monitor`` 重新编译并下载代码。

.. note::

    编译系统编译目标默认为 ``ESP32``，如使用 ``ESP32-S2``，请在编译之前使用 ``idf.py set-target esp32s2`` 配置目标。

已支持的开发板
----------------

============================   ===========================
       ESP32 开发板
----------------------------------------------------------
 |esp32-devkitc|_                |esp32-meshkit-sense|_
----------------------------   ---------------------------
 `esp32-devkitc`_                `esp32-meshkit-sense`_
----------------------------   ---------------------------
 |esp32-lcdkit|_                        
----------------------------   ---------------------------
 `esp32-lcdkit`_       
----------------------------   ---------------------------
       ESP32-S2 开发板    
----------------------------------------------------------
 |esp32s2-saola|_          
----------------------------   ---------------------------
 `esp32s2-saola`_          
----------------------------   ---------------------------
       ESP32-S3 开发板    
----------------------------------------------------------
 |esp32s3-devkitc-v1|_           |esp32s3_usb_otg_ev|_ 
----------------------------   ---------------------------
 `esp32s3-devkitc-v1`_           |esp32s3_usb_otg_ev|_ 
============================   ===========================

.. |esp32-devkitc| image:: ../../_static/esp32-devkitc-v4-front.png
.. _esp32-devkitc: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/hw-reference/modules-and-boards.html#esp32-devkitc-v4

.. |esp32-meshkit-sense| image:: ../../_static/esp32-meshkit-sense.png
.. _esp32-meshkit-sense: ../hw-reference/ESP32-MeshKit-Sense_guide.html

.. |esp32-lcdkit| image:: ../../_static/esp32-lcdkit.png
.. _esp32-lcdkit: ../hw-reference/ESP32-MeshKit-Sense_guide.html

.. |esp32s2-saola| image:: ../../_static/esp32s2-saola.png
.. _esp32s2-saola: https://docs.espressif.com/projects/esp-idf/en/latest/esp32s2/hw-reference/esp32s2/user-guide-saola-1-v1.2.html

.. |esp32s3-devkitc-v1| image:: ../../_static/esp32-s3-devkitc-1-v1-isometric.png
.. _esp32s3-devkitc-v1: https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/hw-reference/esp32s3/user-guide-devkitc-1.html

.. |esp32s3_usb_otg_ev| image:: ../../_static/pic_product_esp32_s3_otg.png
.. _esp32s3_usb_otg_ev: https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s3/esp32-s3-usb-otg/index.html

添加新的开发板
----------------

通过添加新的开发板，可以快速适配基于 ``Boards`` 组件开发的应用程序。

添加开发板过程：

1. 按照 :ref:`组件文件结构 <boards_component>` 准备必要的 ``iot_board.h``；
2. 按照需求在 ``board_xxx.c`` 添加该开发板特有的函数实现，或对公有的弱函数进行覆盖；
3. 按照需求在 ``kconfig.in`` 添加该开发板特有的配置项；
4. 将开发板信息添加到 ``Kconfig.projbuild``，供用户选择；
5. 将开发板目录添加到 ``CMakeLists.txt``，使其能被编译系统索引。如果需要支持老的 ``make`` 编译系统，请同时修改 ``component.mk``。

.. note::

    可复制 ``Boards`` 中已添加的开发板文件夹，通过简单修改完成开发板的添加。

组件依赖
---------

- 公共依赖项：bus, button, led_strip

已适配 IDF 版本
---------------

- ESP-IDF v4.4 及以上版本。

已适配芯片
----------

-  ESP32
-  ESP32-S2
-  ESP32-S3