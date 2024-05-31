IR learn
=============
:link_to_translation:`zh_CN:[中文]`

This is an IR learning component based on the RMT module, capable of receiving infrared signals with a carrier frequency of 38KHz. The received signals are stored and forwarded in raw format, without support for the specific parsing of IR protocols.

Application Examples
---------------------

Create IR_learn
^^^^^^^^^^^^^^^^
.. code:: c

    const ir_learn_cfg_t config = {
        .learn_count = 4,              /*!< IR learn count needed */
        .learn_gpio = GPIO_NUM_38,     /*!< IR learn io that consumed by the sensor */
        .clk_src = RMT_CLK_SRC_DEFAULT,/*!< RMT clock source */
        .resolution = 1000000,         /*!< RMT resolution, 1M, 1 tick = 1us*/

        .task_stack = 4096,   /*!< IR learn task stack size */
        .task_priority = 5,   /*!< IR learn task priority */
        .task_affinity = 1,   /*!< IR learn task pinned to core (-1 is no affinity) */
        .callback = cb,       /*!< IR learn result callback for user */
    };

    ESP_ERROR_CHECK(ir_learn_new(&config, &handle));

Callback function
^^^^^^^^^^^^^^^^^^^^^

.. code:: c

    void ir_learn_auto_learn_cb(ir_learn_state_t state, uint8_t sub_step, struct ir_learn_sub_list_head *data)
    {
      switch (state) {
      /**< IR learn ready, after successful initialization */
      case IR_LEARN_STATE_READY:
         ESP_LOGI(TAG, "IR Learn ready");
         break;
      /**< IR learn exit */
      case IR_LEARN_STATE_EXIT:
         ESP_LOGI(TAG, "IR Learn exit");
         break;
      /**< IR learn successfully */
      case IR_LEARN_STATE_END:
         ESP_LOGI(TAG, "IR Learn end");
         ir_learn_save_result(&ir_test_result, data);
         ir_learn_print_raw(data);
         ir_learn_stop(&handle);
         break;
      /**< IR learn failure */
      case IR_LEARN_STATE_FAIL:
         ESP_LOGI(TAG, "IR Learn failed, retry");
         break;
      /**< IR learn step, start from 1 */
      case IR_LEARN_STATE_STEP:
      default:
         ESP_LOGI(TAG, "IR Learn step:[%d][%d]", state, sub_step);
         break;
      }
      return;
   }

IR_learn supports single and multi-packet learning. Upon successful learning, it notifies the IR_LEARN_STATE_END event. Users can handle the learning result independently, and the format of the learned data packet is described in struct :cpp:struct:`ir_learn_sub_list_t`.

The learning process automatically verifies the data. If the verification fails, it notifies the IR_LEARN_STATE_FAIL event. The verification logic is as follows:
   - For each packet, if the quantity of symbols is inconsistent, it is considered a learning failure.
   - For each level's time difference, if it exceeds the threshold, it is considered a learning failure. (Threshold can be adjusted using the RMT_DECODE_MARGIN_US in menuconfig.)


Send out
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code:: c

   void ir_learn_test_tx_raw(struct ir_learn_sub_list_head *rmt_out)
   {
      /**< Create RMT TX channel */
      rmt_tx_channel_config_t tx_channel_cfg = {
         .clk_src = RMT_CLK_SRC_DEFAULT,
         .resolution_hz = IR_RESOLUTION_HZ,
         .mem_block_symbols = 128,  // amount of RMT symbols that the channel can store at a time
         .trans_queue_depth = 4,    // number of transactions that allowed to pending in the background
         .gpio_num = IR_TX_GPIO_NUM,
      };
      rmt_channel_handle_t tx_channel = NULL;
      ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_channel_cfg, &tx_channel));
      
      /**< Modulate carrier to TX channel */
      rmt_carrier_config_t carrier_cfg = {
         .duty_cycle = 0.33,
         .frequency_hz = 38000, // 38KHz
      };
      ESP_ERROR_CHECK(rmt_apply_carrier(tx_channel, &carrier_cfg));

      rmt_transmit_config_t transmit_cfg = {
         .loop_count = 0, // no loop
      };
      
      /**< Install IR encoder, here's raw format */
      ir_encoder_config_t raw_encoder_cfg = {
         .resolution = IR_RESOLUTION_HZ,
      };
      rmt_encoder_handle_t raw_encoder = NULL;
      ESP_ERROR_CHECK(ir_encoder_new(&raw_encoder_cfg, &raw_encoder));
      
      ESP_ERROR_CHECK(rmt_enable(tx_channel));  // Enable RMT TX channels

      /**< Traverse and send commands */
      struct ir_learn_sub_list_t *sub_it;
      SLIST_FOREACH(sub_it, rmt_out, next) {
         vTaskDelay(pdMS_TO_TICKS(sub_it->timediff / 1000));

         rmt_symbol_word_t *rmt_symbols = sub_it->symbols.received_symbols;
         size_t symbol_num = sub_it->symbols.num_symbols;

         ESP_ERROR_CHECK(rmt_transmit(tx_channel, raw_encoder, rmt_symbols, symbol_num, &transmit_cfg));
         rmt_tx_wait_all_done(tx_channel, -1);  // wait all transactions finished
      }
      
      /**< Disable and delete RMT TX channels */
      rmt_disable(tx_channel);
      rmt_del_channel(tx_channel);
      raw_encoder->del(raw_encoder);
   }

API Reference
-----------------

.. include-build-file:: inc/ir_learn.inc