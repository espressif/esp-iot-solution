数字可寻址照明接口（DALI）总线驱动
=====================================

:link_to_translation:`en:[English]`

DALI 组件提供了基于 ESP-IDF 的 DALI（IEC 62386）主站驱动。
该驱动使用 ESP 的 RMT 外设实现 :term:`前向帧 <前向帧(FF，Forward Frame)>` 发送与 :term:`后向帧 <后向帧(BF，Backward Frame)>` 解码，便于应用直接控制和查询 DALI 控制设备。

功能特性
--------

* 物理层 — 基于 RMT 的曼彻斯特编码收发（Te = 416.67 µs，±10 % 容差）。
* 寻址模式 — :term:`短地址 <短地址(Short Address)>` （0–63）、 :term:`组地址 <组地址(Group Address)>` （0–15）、广播及特殊命令。
* Part 102（控制装置） — DAPC 调光、间接/配置命令及灯具查询。
* Part 103（控制设备） — 事件上报及设备/实例级命令。
* Part 303/304（传感器） — 支持人体感应（303）和光照（304）传感器。
* Part 209（DT8 调色） — 支持 RGB、CCT（Tc）、XY 色度控制。
* Commissioning — 自动短地址分配，支持 Part 102 灯具和 Part 103 输入设备。
* Send-twice 支持 — 内置 send-twice 机制，满足需要 100 ms 内双发的命令要求。

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

5. Commissioning（自动分配短地址）：

   .. code-block:: c

      /* Part 102 — 控制装置（灯具） */
      uint8_t count102 = 0;
      esp_err_t err = dali_commission(dali, DALI_COMMISSION_ALL, 0, 64, &count102,
                                      DALI_TX_TIMEOUT_MS);
      if (err == ESP_OK) {
          ESP_LOGI("dali", "Part 102 commissioning 完成: %u 个设备", count102);
      }

      /* Part 103 — 控制设备（传感器） */
      uint8_t count103 = 0;
      err = dali_103_commission(dali, DALI_COMMISSION_ALL, 10, 64, &count103,
                                DALI_TX_TIMEOUT_MS);
      if (err == ESP_OK) {
          ESP_LOGI("dali", "Part 103 commissioning 完成: %u 个设备", count103);
      }

6. 设置 DT8 颜色（Part 209）：

   .. code-block:: c

      /* RGB 模式 — 将地址 4 设为红色 */
      dali_color_val_t color = { .rgb = { .r = 254, .g = 0, .b = 0 } };
      ESP_ERROR_CHECK(dali_master_set_color(dali, DALI_ADDR_SHORT, 4,
                                             DALI_COLOR_RGB, color,
                                             DALI_TX_TIMEOUT_MS));

      /* CCT 模式 — 将地址 4 设为 2700K（370 Mirek） */
      dali_color_val_t cct = { .cct = { .mirek = 370 } };
      ESP_ERROR_CHECK(dali_master_set_color(dali, DALI_ADDR_SHORT, 4,
                                             DALI_COLOR_CCT, cct,
                                             DALI_TX_TIMEOUT_MS));

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

* DAPC 直控： ``config.is_cmd = false`` ， ``config.command`` 为亮度值。
* 普通命令/查询： ``config.is_cmd = true`` ， ``config.command`` 来自 ``dali_command.h`` 。
* 需要双发的命令：设置 ``config.send_twice = true`` 。
* 查询命令：传入 ``result != NULL`` ，并用 ``DALI_RESULT_IS_VALID(*result)`` 判断是否收到有效回复。
  ``dali_master_do_transaction()`` 每次调用后自动插入所需帧间隔 (> 22 :term:`Te` )，无需在连续调用间手动延时。

时序说明
--------

* DALI 对帧间隔与后向帧响应窗口有严格约束。
* 驱动在每次 ``dali_master_do_transaction()`` 调用后自动插入最小帧间隔 (> 22 Te)，满足 IEC 62386 时序要求。
* ``dali_master_do_transaction()`` 为阻塞调用 — 请勿在 ISR 或对实时性要求极高的任务中调用。

示例与测试
----------

* 示例工程：:example:`lighting/dali_basic`
* 组件测试：``components/dali/test_apps/main/dali_test.c``

示例覆盖内容包括：

* Commissioning — Part 102（灯具）和 Part 103（传感器）自动短地址分配
* 动态设备识别 — 扫描并按设备类型区分 DT6（调光）与 DT8（调色）灯具
* DAPC 调光 — 对所有发现的灯具执行亮度控制序列
* Part 103 传感器 — 占用检测及事件触发动作
* 同时闪烁 — 检测到有人时：DT6 灯具同时亮灭闪烁，DT8 灯具红蓝交替闪烁
* 查询命令 — 状态、实际亮度及设备类型查询

API 参考
--------

.. include-build-file:: inc/dali.inc

术语表
------

.. glossary::

   Te
      DALI 半周期单位，标称值为 416.67 µs（IEC 62386 允许 ±10% 容差）。
      所有 DALI 时序均以 Te 的整数倍表示。

   前向帧(FF，Forward Frame)
      由 DALI 主站发送给控制设备的 16 位帧，由 1 个起始位 + 16 个数据位 +
      2 个停止位组成，共 38 Te。第一字节为地址字节，第二字节为命令或亮度值。

   后向帧(BF，Backward Frame)
      由 DALI 从设备响应查询命令时发送的 8 位回复帧，由 1 个起始位 + 8 个数据位 +
      2 个停止位组成，共 22 Te。从设备须在前向帧结束后 7 Te～22 Te 内回复。

   短地址(Short Address)
      分配给单个 DALI 控制设备的唯一地址，范围 0–63。
      在前向帧中编码为 ``0AAAAAAS`` （A 为地址位，S 为选择位）。
      102和103设备的短地址是分别独立的，在commissioning分配的时候不会有地址冲突的问题。

   组地址(Group Address)
      最多 16 个控制设备共享的地址，范围 0–15。
      编码为 ``100AAAAS`` ，可同时控制多个灯具而无需逐一寻址。