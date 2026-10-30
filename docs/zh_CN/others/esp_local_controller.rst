**ESP Local Controller 本地控制客户端**
========================================

:link_to_translation:`en:[English]`

``esp_local_controller`` 是面向 ESP-IDF
`esp_local_ctrl <https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/api-reference/protocols/esp_local_ctrl.html>`_
的 **控制器侧** 客户端。

.. list-table::
   :header-rows: 1
   :widths: 30 70

   * - 组件
     - 角色
   * - **esp_local_ctrl**
     - 设备端 **Server** ：暴露属性
   * - **protocomm_ext**
     - 控制器 **管道** （传输 + Security 0 / 1 / 2）
   * - **esp_local_controller**
     - 控制器 **业务** （``esp_local_ctrl/control`` protobuf）

本组件负责编解码本地控制 protobuf。会话传输与安全握手仍由 ``protocomm_ext`` 完成。

功能
----

- HTTP、BLE（NimBLE Central）、UART Console
- 解析 version JSON（``local_ctrl.sec_ver`` / ``sec_patch_ver``）
- 按索引查询属性数量、读取 / 写入属性
- 通过 ``protocomm_ext`` 支持 Security 0 / 1 / 2

**支持目标：** ESP32 / ESP32-S3 / ESP32-C2 / ESP32-C3 / ESP32-C5 / ESP32-C6 / ESP32-C61 / ESP32-H2 / ESP32-P4

典型流程
--------

.. code:: c

    #include "esp_local_controller.h"
    #include "protocomm_ext.h"
    #include "protocomm_ext_security.h"

    protocomm_ext_t *pc = protocomm_ext_new(&pe_cfg); /* 传输 + Sec0 占位 */
    esp_local_controller_t *ctrl = esp_local_controller_create(pc);

    esp_local_controller_fetch_version(ctrl, /* BLE: &ble_addr，否则 NULL */ NULL);
    esp_local_controller_version_t ver = {0};
    esp_local_controller_get_version(ctrl, &ver);
    /* 按 ver.sec_ver 调用 protocomm_ext_set_security() 配置 PoP / Sec2 */
    esp_local_controller_version_free(&ver);

    esp_local_controller_establish_security(ctrl);

    uint32_t count = 0;
    esp_local_controller_get_property_count(ctrl, &count);

    esp_local_controller_stop_session(ctrl);
    esp_local_controller_delete(ctrl);
    protocomm_ext_delete(pc);

若安全方案已知，可先 ``protocomm_ext_set_security()``，再调用
``esp_local_controller_start_session()``。

**会话：** 一个实例对应一台设备。每台设备各建一个实例，即可同时保持多条 HTTP 会话。同一台设备仍然只接受一个 HTTP 客户端。

**线程安全：** 同一实例的 API 非线程安全，请在单任务中串行调用。不同实例可以在这个任务里轮流使用。

添加依赖
--------

.. code:: yaml

    dependencies:
      espressif/esp_local_controller: "^1.1.0"

示例
----

见 ``examples/esp_local_controller``。设备侧请使用 IDF 官方示例
``examples/protocols/esp_local_ctrl``。

API 参考
--------

.. include-build-file:: inc/esp_local_controller.inc
