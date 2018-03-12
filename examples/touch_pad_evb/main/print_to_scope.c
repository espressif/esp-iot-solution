#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/uart.h"

#define SCOPE_DEBUG_TXD_IO      GPIO_NUM_17
#define SCOPE_DEBUG_RXD_IO      GPIO_NUM_16
#define SCOPE_DEBUG_BAUD_RATE   576000

//Data send adapt CRC16 verification,The following is the function of CRC16,please refer
//-------------------------------------------------------------------------------------------
unsigned short CRC_CHECK(unsigned char *Buf, unsigned char CRC_CNT)
{
    unsigned short CRC_Temp;
    unsigned char i,j;
    CRC_Temp = 0xffff;
    for (i=0;i<CRC_CNT; i++){
        CRC_Temp ^= Buf[i];
        for (j=0;j<8;j++) {
            if (CRC_Temp & 0x01)
                CRC_Temp = (CRC_Temp >>1 ) ^ 0xa001;
            else
                CRC_Temp = CRC_Temp >> 1;
        }
    }
    return(CRC_Temp);
}

//-------------------------------------------------------------------------------------------
//the following is MPU code,please refer.
//-------------------------------------------------------------------------------------------
int print_to_scope(uint8_t uart_num, uint16_t *ch_buf)
{
    static uint8_t uart_used = 0xff;
    uint8_t TxBuf[20];
    uint16_t data[4];
    uint16_t CRC_Tmp;
    if(uart_num != uart_used) {
        uart_used = uart_num;
        uart_config_t uart_config = {
            .baud_rate = SCOPE_DEBUG_BAUD_RATE,
            .data_bits = UART_DATA_8_BITS,
            .parity = UART_PARITY_DISABLE,
            .stop_bits = UART_STOP_BITS_1,
            .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
        };
        uart_param_config(uart_num, &uart_config);
        // Set UART pins using UART0 default pins i.e. no changes
        uart_set_pin(uart_num, SCOPE_DEBUG_TXD_IO, SCOPE_DEBUG_RXD_IO,
                UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
        uart_driver_install(uart_num, 1024, 1024 * 2, 0, NULL, 0);
    }

    data[0] = *ch_buf;
    data[1] = *(ch_buf+1);
    data[2] = *(ch_buf+2);
    data[3] = *(ch_buf+3);

    //add data to Tx buf
    for(int i=0;i<4;i++){
        TxBuf[2*i+0] = data[i]&0xff;        // ch[i] low byte
        TxBuf[2*i+1] = (data[i]>>8)&0xff;   // ch[i] hi byte
    }
    //add CRC to TC BUF
    CRC_Tmp = CRC_CHECK(TxBuf,8);
    TxBuf[8] = CRC_Tmp&0xff;
    TxBuf[9] = (CRC_Tmp>>8)&0xff;
    uart_wait_tx_done(uart_num, portMAX_DELAY);
    return uart_write_bytes(uart_num, (const char *)TxBuf, 10);
}
