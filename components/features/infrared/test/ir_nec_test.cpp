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
#include "esp_log.h"
#include "ir_nec.h"
#include "unity.h"

TEST_CASE("IR nec test", "[rmt_nec][iot]")
{
	printf("ir test...\n");
	CIrNecSender tx_test(RMT_CHANNEL_0, GPIO_NUM_18, false);
    CIrNecRecv rx_test(RMT_CHANNEL_1, GPIO_NUM_19, 1);

	tx_test.send(0x1, 0x1);
	tx_test.send(0x1, 0x2);
	tx_test.send(0x1, 0x3);
	tx_test.send(0x1, 0x4);

    uint16_t addr, cmd;
    rx_test.recv(&addr, &cmd, 1000/ portTICK_RATE_MS);
    printf("RMT RCV --- addr: 0x%04x cmd: 0x%04x\n", addr, cmd);
    rx_test.recv(&addr, &cmd, 1000/ portTICK_RATE_MS);
    printf("RMT RCV --- addr: 0x%04x cmd: 0x%04x\n", addr, cmd);
    rx_test.recv(&addr, &cmd, 1000/ portTICK_RATE_MS);
    printf("RMT RCV --- addr: 0x%04x cmd: 0x%04x\n", addr, cmd);
    rx_test.recv(&addr, &cmd, 1000/ portTICK_RATE_MS);
    printf("RMT RCV --- addr: 0x%04x cmd: 0x%04x\n", addr, cmd);
}
