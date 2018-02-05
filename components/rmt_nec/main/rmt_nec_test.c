/* NEC remote infrared RMT example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_err.h"
#include "esp_log.h"
#include "driver/periph_ctrl.h"
#include "driver/rmt_nec.h"
//#include "soc/rmt_reg.h"

static const char* NEC_TAG = "NEC";

//CHOOSE SELF TEST OR NORMAL TEST
#define RMT_RX_SELF_TEST   1

/******************************************************/
/*****                SELF TEST:                  *****/
/*Connect RMT_TX_GPIO_NUM with RMT_RX_GPIO_NUM        */
/*TX task will send NEC data with carrier disabled    */
/*RX task will print NEC data it receives.            */
/******************************************************/
#if RMT_RX_SELF_TEST
#define RMT_RX_ACTIVE_LEVEL  1   /*!< Data bit is active high for self test mode */
#define RMT_TX_CARRIER_EN    0   /*!< Disable carrier for self test mode  */
#else
//Test with infrared LED, we have to enable carrier for transmitter
//When testing via IR led, the receiver waveform is usually active-low.
#define RMT_RX_ACTIVE_LEVEL  0   /*!< If we connect with a IR receiver, the data is active low */
#define RMT_TX_CARRIER_EN    1   /*!< Enable carrier for IR transmitter test with IR led */
#endif

#define RMT_TX_CHANNEL    1     /*!< RMT channel for transmitter */
#define RMT_TX_GPIO_NUM  18     /*!< GPIO number for transmitter signal */
#define RMT_RX_CHANNEL    0     /*!< RMT channel for receiver */
#define RMT_RX_GPIO_NUM  19     /*!< GPIO number for receiver */
#define RMT_CLK_DIV      100    /*!< RMT counter clock divider */
#define RMT_TICK_10_US    (80000000/RMT_CLK_DIV/100000)   /*!< RMT counter value for 10 us.(Source clock is APB clock) */

//#define NEC_HEADER_HIGH_US    9000                         /*!< NEC protocol header: positive 9ms */
//#define NEC_HEADER_LOW_US     4500                         /*!< NEC protocol header: negative 4.5ms*/
//#define NEC_BIT_ONE_HIGH_US    560                         /*!< NEC protocol data bit 1: positive 0.56ms */
//#define NEC_BIT_ONE_LOW_US    (2250-NEC_BIT_ONE_HIGH_US)   /*!< NEC protocol data bit 1: negative 1.69ms */
//#define NEC_BIT_ZERO_HIGH_US   560                         /*!< NEC protocol data bit 0: positive 0.56ms */
//#define NEC_BIT_ZERO_LOW_US   (1120-NEC_BIT_ZERO_HIGH_US)  /*!< NEC protocol data bit 0: negative 0.56ms */
//#define NEC_BIT_END            560                         /*!< NEC protocol end: positive 0.56ms */
//#define NEC_BIT_MARGIN         20                          /*!< NEC parse margin time */

//#define NEC_ITEM_DURATION(d)  ((d & 0x7fff)*10/RMT_TICK_10_US)  /*!< Parse duration time from memory register value */
#define NEC_DATA_ITEM_NUM   34  /*!< NEC code item number: header + 32bit data + end */
#define RMT_TX_DATA_NUM  100    /*!< NEC tx test data number */
#define rmt_item32_tIMEOUT_US  9500   /*!< RMT receiver timeout value(us) */


/**
 * @brief RMT receiver demo, this task will print each received NEC data.
 *
 */
static void rmt_example_nec_rx_task()
{
    int channel = RMT_RX_CHANNEL;
    uint16_t *recv_data;

    rmt_config_t rmt_rx;
    rmt_rx.channel = RMT_RX_CHANNEL;
    rmt_rx.gpio_num = RMT_RX_GPIO_NUM;
    rmt_rx.clk_div = RMT_CLK_DIV;
    rmt_rx.mem_block_num = 1;
    rmt_rx.rmt_mode = RMT_MODE_RX;
    rmt_rx.rx_config.filter_en = true;
    rmt_rx.rx_config.filter_ticks_thresh = 100;
    rmt_rx.rx_config.idle_threshold = rmt_item32_tIMEOUT_US / 10 * (RMT_TICK_10_US);

    ir_nec_init(rmt_rx);
    while(1){
    	recv_data = ir_nec_recv(channel);
    	ESP_LOGI(NEC_TAG, "receive");
    	vTaskDelay(2000 / portTICK_PERIOD_MS);
    }


    vTaskDelete(NULL);
}

/**
 * @brief RMT transmitter demo, this task will periodically send NEC data. (100 * 32 bits each time.)
 *
 */
static void rmt_example_nec_tx_task()
{
    vTaskDelay(10);
    int ret=0;
    rmt_config_t rmt_tx;
    rmt_tx.channel = RMT_TX_CHANNEL;
    rmt_tx.gpio_num = RMT_TX_GPIO_NUM;
    rmt_tx.mem_block_num = 1;
    rmt_tx.clk_div = RMT_CLK_DIV;
    rmt_tx.tx_config.loop_en = false;
    rmt_tx.tx_config.carrier_duty_percent = 50;
    rmt_tx.tx_config.carrier_freq_hz = 38000;
    rmt_tx.tx_config.carrier_level = 1;
    rmt_tx.tx_config.carrier_en = RMT_TX_CARRIER_EN;
    rmt_tx.tx_config.idle_level = 0;
    rmt_tx.tx_config.idle_output_en = true;
    rmt_tx.rmt_mode = 0;

    ir_nec_init(rmt_tx);

    esp_log_level_set(NEC_TAG, ESP_LOG_INFO);
    int channel = RMT_TX_CHANNEL;
    uint16_t cmd = 0x0;
    uint16_t addr = 0x11;
    for(;;) {
//        ESP_LOGI(NEC_TAG, "RMT TX DATA");
        ret = ir_nec_send(channel, cmd, addr);
        cmd++;
		addr++;
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}


void rmt_test()
{
    xTaskCreate(rmt_example_nec_rx_task, "rmt_nec_rx_task", 2048, NULL, 10, NULL);
    xTaskCreate(rmt_example_nec_tx_task, "rmt_nec_tx_task", 2048, NULL, 10, NULL);
}


TEST_CASE("Rmt_nec test", "[rmt_nec][iot]")
{
	rmt_test();
}
