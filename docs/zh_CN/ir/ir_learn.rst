红外学习
=============
:link_to_translation:`en:[English]`

这是基于 RMT 模块的红外学习组件，可以接收学习 38KHz 载波的红外信号。接收的信号以 raw 的形式保存和转发，不支持对红外协议的具体解析。

应用示例
-----------

创建红外学习任务
^^^^^^^^^^^^^^^^
.. code:: c

    const ir_learn_cfg_t config = {
        .learn_count = 4,              /*!< 需要的红外学习次数 */
        .learn_gpio = GPIO_NUM_38,     /*!< 红外学习 IO */
        .clk_src = RMT_CLK_SRC_DEFAULT,/*!< RMT 时钟源 */
        .resolution = 1000000,         /*!< RMT 分辨率，1M，1 个时钟周期 = 1 微秒 */

        .task_stack = 4096,   /*!< 红外学习任务堆栈大小 */
        .task_priority = 5,   /*!< 红外学习任务优先级 */
        .task_affinity = 1,   /*!< 红外学习任务固定到核（-1 表示无固定） */
        .callback = cb,       /*!< 红外学习结果回调函数 */
    };

    ESP_ERROR_CHECK(ir_learn_new(&config, &handle));

回调函数处理
^^^^^^^^^^^^^^

.. code:: c

    void ir_learn_auto_learn_cb(ir_learn_state_t state, uint8_t sub_step, struct ir_learn_sub_list_head *data)
    {
      switch (state) {
      /**< 红外学习准备就绪，在成功初始化后 */
      case IR_LEARN_STATE_READY:
         ESP_LOGI(TAG, "IR Learn ready");
         break;
      /**< 红外学习退出 */
      case IR_LEARN_STATE_EXIT:
         ESP_LOGI(TAG, "IR Learn exit");
         break;
      /**< 红外学习成功 */
      case IR_LEARN_STATE_END:
         ESP_LOGI(TAG, "IR Learn end");
         ir_learn_save_result(&ir_test_result, data);
         ir_learn_print_raw(data);
         ir_learn_stop(&handle);
         break;
      /**< 红外学习失败 */
      case IR_LEARN_STATE_FAIL:
         ESP_LOGI(TAG, "IR Learn failed, retry");
         break;
      /**< 红外学习步骤，从1开始 */
      case IR_LEARN_STATE_STEP:
      default:
         ESP_LOGI(TAG, "IR Learn step:[%d][%d]", state, sub_step);
         break;
      }
      return;
   }

红外学习支持单包和多包学习，如果学习成功，通知 IR_LEARN_STATE_END 事件。
用户可自行处理学习结果，返回学习的数据包格式见 :cpp:struct:`ir_learn_sub_list_t` 。

学习过程会自动校验数据，如果校验失败，通知 IR_LEARN_STATE_FAIL 事件，校验逻辑如下：
   - 每包 symbol 数量，数量不一致，则判定为学习失败。

   - 每个电平的时间差值，如果超过阈值，则判定为学习失败。（menuconfig 的 RMT_DECODE_MARGIN_US 可调整阈值）


学习包发送
^^^^^^^^^^^^^^

.. code:: c

   void ir_learn_test_tx_raw(struct ir_learn_sub_list_head *rmt_out)
   {
      /**< 创建 RMT 发送通道 */
      rmt_tx_channel_config_t tx_channel_cfg = {
         .clk_src = RMT_CLK_SRC_DEFAULT,
         .resolution_hz = IR_RESOLUTION_HZ,
         .mem_block_symbols = 128,  // 通道可以同时存储的 RMT 符号数量
         .trans_queue_depth = 4,    // 允许在后台挂起的传输数量
         .gpio_num = IR_TX_GPIO_NUM,
      };
      rmt_channel_handle_t tx_channel = NULL;
      ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_channel_cfg, &tx_channel));
      
      /**< 将载波调制到发送通道 */
      rmt_carrier_config_t carrier_cfg = {
         .duty_cycle = 0.33,
         .frequency_hz = 38000, // 38KHz
      };
      ESP_ERROR_CHECK(rmt_apply_carrier(tx_channel, &carrier_cfg));

      rmt_transmit_config_t transmit_cfg = {
         .loop_count = 0, // 禁止循环
      };
      
      /**< 注册红外编码器，编码格式为原始格式 */
      ir_encoder_config_t raw_encoder_cfg = {
         .resolution = IR_RESOLUTION_HZ,
      };
      rmt_encoder_handle_t raw_encoder = NULL;
      ESP_ERROR_CHECK(ir_encoder_new(&raw_encoder_cfg, &raw_encoder));
      
      ESP_ERROR_CHECK(rmt_enable(tx_channel));  // 启用 RMT 发送通道

      /**< 遍历并发送命令 */
      struct ir_learn_sub_list_t *sub_it;
      SLIST_FOREACH(sub_it, rmt_out, next) {
         vTaskDelay(pdMS_TO_TICKS(sub_it->timediff / 1000));

         rmt_symbol_word_t *rmt_symbols = sub_it->symbols.received_symbols;
         size_t symbol_num = sub_it->symbols.num_symbols;

         ESP_ERROR_CHECK(rmt_transmit(tx_channel, raw_encoder, rmt_symbols, symbol_num, &transmit_cfg));
         rmt_tx_wait_all_done(tx_channel, -1);  // 等待发送完成
      }
      
      /**< 停止并删除 RMT 发送通道 */
      rmt_disable(tx_channel);
      rmt_del_channel(tx_channel);
      raw_encoder->del(raw_encoder);
   }

API Reference
-----------------

.. include-build-file:: inc/ir_learn.inc