**Protocomm 扩展方案**
======================

:link_to_translation:`en:[English]`

``protocomm_ext`` 是 ESP-IDF
`protocomm <https://docs.espressif.com/projects/esp-idf/en/latest/api-reference/provisioning/protocomm.html>`_
的 **客户端扩展**。

.. list-table::
   :header-rows: 1
   :widths: 30 70

   * - 组件
     - 角色
   * - **protocomm** (IDF)
     - 设备端 **Server**：承接配网 / 本地控制会话
   * - **protocomm_ext**
     - 控制器端 **Client**：主动建连、完成安全握手、收发载荷

本组件只做 **协议管道**，\ **不**\ 编解码 Wi-Fi 配网或 ``esp_local_ctrl`` 业务 protobuf。
上层应将已编好的端点载荷交给 ``protocomm_ext_send_data()``。

功能
----

- **Security 0**：明文会话
- **Security 1**：Curve25519 + AES-256-CTR（可选 PoP）
- **Security 2**：SRP-6a（NG_3072 / SHA512）+ AES-256-GCM（用户名 / 密码）
- 传输：HTTP / HTTPS、BLE（NimBLE Central）、UART Console
- 可通过 Kconfig 裁剪未使用的安全版本

**支持目标：** ESP32 / ESP32-S3 / ESP32-C2 / ESP32-C3 / ESP32-C5 / ESP32-C6 / ESP32-C61 / ESP32-P4

典型流程
--------

.. code:: c

    #include "protocomm_ext.h"
    #include "protocomm_ext_http.h"
    #include "protocomm_ext_security.h"

    esp_http_client_config_t http_cfg = {
        .url = "http://192.168.4.1:80",
        .timeout_ms = 5000,
    };

    protocomm_ext_security1_params_t pop = {
        .data = (const uint8_t *)"abcd1234",
        .len = 8,
    };

    protocomm_ext_config_data_t cfg = {
        .transport_method = PROTOCOMM_EXT_TRANSPORT_METHOD_HTTP,
        .security_method = PROTOCOMM_EXT_SECURITY_METHOD_SECURITY_1,
        .transport_data = &http_cfg,
        .security_data = &pop,
    };

    protocomm_ext_t *pc = protocomm_ext_new(&cfg);
    protocomm_ext_open_session(pc, NULL);
    protocomm_ext_establish_security(pc, "prov-session");

    uint8_t *resp = NULL;
    size_t resp_len = 0;
    protocomm_ext_send_data(pc, "prov-config", req, req_len, &resp, &resp_len);
    free(resp);

    protocomm_ext_close_session(pc);
    protocomm_ext_delete(pc);

**会话：** 一个实例对应一台设备。HTTP 会保住该实例自己的连接、TCP keepalive 和 Cookie。每台设备各建一个实例。BLE 仍是单连接，并且需要 NimBLE 的 central、observer 与 GATT client 角色。

添加依赖
--------

.. code:: yaml

    dependencies:
      espressif/protocomm_ext: "^1.1.0"

API 参考
--------

.. include-build-file:: inc/protocomm_ext.inc

.. include-build-file:: inc/protocomm_ext_security.inc

.. include-build-file:: inc/protocomm_ext_http.inc

.. include-build-file:: inc/protocomm_ext_nimble.inc

.. include-build-file:: inc/protocomm_ext_console.inc
