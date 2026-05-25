数字可寻址照明接口（DALI）总线驱动
=====================================

:link_to_translation:`en:[English]`

DALI 组件提供了基于 ESP-IDF 的 DALI（IEC 62386）主站驱动。
该驱动使用 ESP 的 RMT 外设实现 :term:`前向帧 <前向帧（FF，Forward Frame）>` 发送与 :term:`后向帧 <后向帧（BF，Backward Frame）>` 解码，便于应用直接控制和查询 DALI 控制设备。

功能特性
--------

* 基于 RMT 的 DALI 物理层曼彻斯特编码收发实现。
* 支持 :term:`短地址 <短地址（Short Address）>`、:term:`组地址 <组地址（Group Address）>`、广播和特殊命令地址模式。
* 支持 DAPC 直控、控制/配置命令和查询命令。
* 内置 send-twice 机制，满足需要 100 ms 内双发的命令要求。
* 支持 TX/RX 极性可配置，适配不同硬件接口反相链路。
* 提供完整测试工程与示例工程。

支持目标
--------

组件元数据当前支持以下 ESP32 系列芯片：

* ESP32
* ESP32-S2
* ESP32-S3
* ESP32-C3
* ESP32-C6
* ESP32-P4
* ESP32-H2

快速开始
--------

1. 包含头文件：

   .. code-block:: c

      #include "dali.h"
      #include "dali_command.h"

2. 初始化驱动：

   .. code-block:: c

      dali_master_handle_t dali;
      dali_master_config_t cfg = {
          .rx_gpio = GPIO_NUM_4,
          .tx_gpio = GPIO_NUM_5,
          .invert_tx = false,
          .invert_rx = false,
      };
      dali_master_rmt_config_t rmt_cfg = {
          .mem_block_symbols = 64,
      };
      ESP_ERROR_CHECK(dali_new_master_rmt(&cfg, &rmt_cfg, &dali));

3. 发送命令（不期望后向帧）：

   .. code-block:: c

      /* 驱动在每次事务后自动插入最小帧间隔 (> 22 Te)，无需手动延时 */
      dali_master_transaction_config_t tx_cfg = {
          .addr_type = DALI_ADDR_SHORT,
          .addr = 0,
          .is_cmd = true,
          .command = DALI_CMD_RECALL_MAX_LEVEL,
          .send_twice = false,
          .tx_timeout_ms = DALI_TX_TIMEOUT_MS,
      };
      ESP_ERROR_CHECK(dali_master_do_transaction(dali, &tx_cfg, NULL));

4. 发送查询（期望后向帧）：

   .. code-block:: c

      int reply = DALI_RESULT_NO_REPLY;
      dali_master_transaction_config_t tx_cfg = {
          .addr_type = DALI_ADDR_SHORT,
          .addr = 0,
          .is_cmd = true,
          .command = DALI_CMD_QUERY_STATUS,
          .send_twice = false,
          .tx_timeout_ms = DALI_TX_TIMEOUT_MS,
      };
      ESP_ERROR_CHECK(dali_master_do_transaction(dali, &tx_cfg, &reply));
      if (DALI_RESULT_IS_VALID(reply)) {
          ESP_LOGI("dali", "QUERY_STATUS = 0x%02X", (unsigned)reply);
      }

配置项
------

DALI 驱动的全部配置均通过两个结构体在运行时完成，无需 Kconfig 选项：

* :cpp:type:`dali_master_config_t` — GPIO 引脚分配（``rx_gpio``、``tx_gpio``）和信号极性反相（``invert_tx``、``invert_rx``）。
* :cpp:type:`dali_master_rmt_config_t` — RMT 后端专用设置，如内存块大小。

.. code-block:: c

   dali_master_config_t cfg = {
       .rx_gpio = GPIO_NUM_4,
       .tx_gpio = GPIO_NUM_5,
       .invert_tx = false,      /* 默认不启用 TX 硬件链路奇数次反相 */
       .invert_rx = false,      /* 默认不启用 RX 硬件链路奇数次反相 */
   };
   dali_master_rmt_config_t rmt_cfg = {
       .mem_block_symbols = 0, /* 0 = 根据 SOC 能力自动检测 */
   };
   ESP_ERROR_CHECK(dali_new_master_rmt(&cfg, &rmt_cfg, &dali));

命令模型
--------

统一通过 ``dali_master_do_transaction()`` 完成不同类型事务。
通过 :cpp:type:`dali_master_transaction_config_t` 描述事务参数：

* DAPC 直控：``config.is_cmd = false``，``config.command`` 为亮度值。
* 普通命令/查询：``config.is_cmd = true``，``config.command`` 来自 ``dali_command.h``。
* 需要双发的命令：设置 ``config.send_twice = true``。
* 查询命令：传入 ``result != NULL``，并用 ``DALI_RESULT_IS_VALID(*result)`` 判断是否收到有效回复。
  ``dali_master_do_transaction()`` 每次调用后自动插入所需帧间隔 (> 22 :term:`Te`)，
  无需在连续调用间手动延时。

时序说明
--------

* DALI 对帧间隔与后向帧响应窗口有严格约束。
* 驱动在每次 ``dali_master_do_transaction()`` 调用后自动插入最小帧间隔 (> 22 Te)，
  满足 IEC 62386 时序要求。
* ``dali_master_do_transaction()`` 为阻塞调用 — 请勿在 ISR 或对实时性要求极高的任务中调用。

示例与测试
----------

* 示例工程：:example:`lighting/dali_basic`
* 组件测试：``components/dali/test_apps/main/dali_test.c``

示例覆盖内容包括：

* DAPC 调光序列
* 普通命令发送
* 查询命令与回复解析
* send-twice 配置命令流程

API 参考
--------

.. include-build-file:: inc/dali.inc

术语表
------

.. glossary::

   Te
      DALI 半周期单位，标称值为 416.67 µs（IEC 62386 允许 ±10% 容差）。
      所有 DALI 时序均以 Te 的整数倍表示。

   前向帧（FF，Forward Frame）
      由 DALI 主站发送给控制设备的 16 位帧，由 1 个起始位 + 16 个数据位 +
      2 个停止位组成，共 38 Te。第一字节为地址字节，第二字节为命令或亮度值。

   后向帧（BF，Backward Frame）
      DALI 从设备响应查询命令时发送的 8 位回复帧，由 1 个起始位 + 8 个数据位 +
      2 个停止位组成，共 22 Te。从设备须在前向帧结束后 7 Te～22 Te 内回复。

   短地址（Short Address）
      分配给单个 DALI 控制设备的唯一地址，范围 0–63。
      在前向帧中编码为 ``0AAAAAAS``（A 为地址位，S 为选择位）。

   组地址（Group Address）
      最多 16 个控制设备共享的地址，范围 0–15。
      编码为 ``100AAAAS``，可同时控制多个灯具而无需逐一寻址。
