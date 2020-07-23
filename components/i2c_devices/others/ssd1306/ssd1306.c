// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#include "driver/i2c.h"
#include "iot_ssd1306.h"
#include "ssd1306_fonts.h"
#include <time.h>
#include <sys/time.h>

//I2C
#define OLED_IIC_FREQ_HZ            100000              /*!< I2C colock frequency >*/
#define OLED_IIC_TX_BUF_DISABLE     0                   /*!< I2C Tx buffer disable >*/
#define OLED_IIC_RX_BUF_DISABLE     0                   /*!< I2C Rx buffer disable >*/

#define WRITE_BIT                   I2C_MASTER_WRITE    /*!< I2C master write */
#define READ_BIT                    I2C_MASTER_READ     /*!< I2C master read */
#define ACK_CHECK_EN                1                   /*!< I2C master will check ack from slave*/
#define ACK_CHECK_DIS               0                   /*!< I2C master will not check ack from slave */
#define ACK_VAL                     0x0                 /*!< I2C ack value */
#define NACK_VAL                    0x1                 /*!< I2C nack value */

typedef struct {
    i2c_bus_handle_t bus;
    uint16_t dev_addr;
    uint8_t s_chDisplayBuffer[128][8];
} ssd1306_dev_t;

static uint32_t _pow(uint8_t m, uint8_t n)
{
    uint32_t result = 1;
    while (n--) {
        result *= m;
    }
    return result;
}

void iot_set_column_address(ssd1306_handle_t dev)
{
    iot_ssd1306_write_byte(dev, SSD1306_SET_LOWER_ADDRESS, SSD1306_CMD);
    iot_ssd1306_write_byte(dev, SSD1306_SET_HIGHER_ADDRESS, SSD1306_CMD);
}

esp_err_t iot_ssd1306_write_byte(ssd1306_handle_t dev, uint8_t chData,
        uint8_t chCmd)
{
    ssd1306_dev_t* device = (ssd1306_dev_t*) dev;
    esp_err_t ret;
#ifdef INTERFACE_4WIRE_SPI

#endif

#ifdef INTERFACE_IIC
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (device->dev_addr << 1) | WRITE_BIT,
    ACK_CHECK_EN);
    if (chCmd) {
        i2c_master_write_byte(cmd, SSD1306_WRITE_DAT, ACK_CHECK_EN);
    } else {
        i2c_master_write_byte(cmd, SSD1306_WRITE_CMD, ACK_CHECK_EN);
    }
    i2c_master_write_byte(cmd, chData, ACK_CHECK_EN);
    i2c_master_stop(cmd);
    ret = iot_i2c_bus_cmd_begin(device->bus, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
#endif
    return ret;
}

esp_err_t iot_ssd1306_fill_rectangle(ssd1306_handle_t dev, uint8_t chXpos1,
        uint8_t chYpos1, uint8_t chXpos2, uint8_t chYpos2, uint8_t chDot)
{
    uint8_t chXpos, chYpos;

    for (chXpos = chXpos1; chXpos <= chXpos2; chXpos++) {
        for (chYpos = chYpos1; chYpos <= chYpos2; chYpos++) {
            iot_ssd1306_fill_point(dev, chXpos, chYpos, chDot);
        }
    }
    return ESP_OK;
}

void iot_ssd1306_draw_num(ssd1306_handle_t dev, uint8_t chXpos, uint8_t chYpos,
        uint32_t chNum, uint8_t chLen, uint8_t chSize)
{
    uint8_t i;
    uint8_t chTemp, chShow = 0;

    for (i = 0; i < chLen; i++) {
        chTemp = (chNum / _pow(10, chLen - i - 1)) % 10;
        if (chShow == 0 && i < (chLen - 1)) {
            if (chTemp == 0) {
                iot_ssd1306_draw_char(dev, chXpos + (chSize / 2) * i, chYpos,
                        ' ', chSize, 1);
                continue;
            } else {
                chShow = 1;
            }
        }
        iot_ssd1306_draw_char(dev, chXpos + (chSize / 2) * i, chYpos,
                chTemp + '0', chSize, 1);
    }
}

void iot_ssd1306_draw_char(ssd1306_handle_t dev, uint8_t chXpos, uint8_t chYpos,
        uint8_t chChr, uint8_t chSize, uint8_t chMode)
{
    uint8_t i, j;
    uint8_t chTemp, chYpos0 = chYpos;

    chChr = chChr - ' ';
    for (i = 0; i < chSize; i++) {
        if (chSize == 12) {
            if (chMode) {
                chTemp = c_chFont1206[chChr][i];
            } else {
                chTemp = ~c_chFont1206[chChr][i];
            }
        } else {
            if (chMode) {
                chTemp = c_chFont1608[chChr][i];
            } else {
                chTemp = ~c_chFont1608[chChr][i];
            }
        }

        for (j = 0; j < 8; j++) {
            if (chTemp & 0x80) {
                iot_ssd1306_fill_point(dev, chXpos, chYpos, 1);
            } else {
                iot_ssd1306_fill_point(dev, chXpos, chYpos, 0);
            }
            chTemp <<= 1;
            chYpos++;

            if ((chYpos - chYpos0) == chSize) {
                chYpos = chYpos0;
                chXpos++;
                break;
            }
        }
    }
}

esp_err_t iot_ssd1306_draw_string(ssd1306_handle_t dev, uint8_t chXpos,
        uint8_t chYpos, const uint8_t *pchString, uint8_t chSize,
        uint8_t chMode)
{
    esp_err_t ret = ESP_OK;
    while (*pchString != '\0') {
        if (chXpos > (SSD1306_WIDTH - chSize / 2)) {
            chXpos = 0;
            chYpos += chSize;
            if (chYpos > (SSD1306_HEIGHT - chSize)) {
                chYpos = chXpos = 0;
                ret = iot_ssd1306_clear_screen(dev, 0x00);
                if (ret == ESP_FAIL) {
                    return ret;
                }
            }
        }
        iot_ssd1306_draw_char(dev, chXpos, chYpos, *pchString, chSize, chMode);
        chXpos += chSize / 2;
        pchString++;
    }
    return ret;
}

void iot_ssd1306_fill_point(ssd1306_handle_t dev, uint8_t chXpos, uint8_t chYpos,
        uint8_t chPoint)
{
    ssd1306_dev_t* device = (ssd1306_dev_t*) dev;
    uint8_t chPos, chBx, chTemp = 0;

    if (chXpos > 127 || chYpos > 63) {
        return;
    }
    chPos = 7 - chYpos / 8; //
    chBx = chYpos % 8;
    chTemp = 1 << (7 - chBx);

    if (chPoint) {
        device->s_chDisplayBuffer[chXpos][chPos] |= chTemp;
    } else {
        device->s_chDisplayBuffer[chXpos][chPos] &= ~chTemp;
    }
}

void iot_ssd1306_draw_1616char(ssd1306_handle_t dev, uint8_t chXpos, uint8_t chYpos,
        uint8_t chChar)
{
    uint8_t i, j;
    uint8_t chTemp = 0, chYpos0 = chYpos, chMode = 0;

    for (i = 0; i < 32; i++) {
        chTemp = c_chFont1612[chChar - 0x30][i];
        for (j = 0; j < 8; j++) {
            chMode = chTemp & 0x80 ? 1 : 0;
            iot_ssd1306_fill_point(dev, chXpos, chYpos, chMode);
            chTemp <<= 1;
            chYpos++;
            if ((chYpos - chYpos0) == 16) {
                chYpos = chYpos0;
                chXpos++;
                break;
            }
        }
    }
}

void iot_ssd1306_draw_3216char(ssd1306_handle_t dev, uint8_t chXpos, uint8_t chYpos,
        uint8_t chChar)
{
    uint8_t i, j;
    uint8_t chTemp = 0, chYpos0 = chYpos, chMode = 0;

    for (i = 0; i < 64; i++) {
        chTemp = c_chFont3216[chChar - 0x30][i];
        for (j = 0; j < 8; j++) {
            chMode = chTemp & 0x80 ? 1 : 0;
            iot_ssd1306_fill_point(dev, chXpos, chYpos, chMode);
            chTemp <<= 1;
            chYpos++;
            if ((chYpos - chYpos0) == 32) {
                chYpos = chYpos0;
                chXpos++;
                break;
            }
        }
    }
}

void iot_ssd1306_draw_bitmap(ssd1306_handle_t dev, uint8_t chXpos, uint8_t chYpos,
        const uint8_t *pchBmp, uint8_t chWidth, uint8_t chHeight)
{
    uint16_t i, j, byteWidth = (chWidth + 7) / 8;

    for (j = 0; j < chHeight; j++) {
        for (i = 0; i < chWidth; i++) {
            if (*(pchBmp + j * byteWidth + i / 8) & (128 >> (i & 7))) {
                iot_ssd1306_fill_point(dev, chXpos + i, chYpos + j, 1);
            }
        }
    }
}

esp_err_t iot_ssd1306_init(ssd1306_handle_t dev)
{
    esp_err_t ret;
    PIN_FUNC_SELECT(SSD1306_CS_MUX, SSD1306_CS_FUNC);
    PIN_FUNC_SELECT(SSD1306_RES_MUX, SSD1306_RES_FUNC);
    PIN_FUNC_SELECT(SSD1306_DC_MUX, SSD1306_DC_FUNC);
    PIN_FUNC_SELECT(SSD1306_DIN_MUX, SSD1306_DIN_FUNC);
    PIN_FUNC_SELECT(SSD1306_CLK_MUX, SSD1306_CLK_FUNC);

#ifdef	INTERFACE_IIC
    __SSD1306_CS_CLR(); //CS reset
    __SSD1306_DC_CLR(); //D/C reset
    __SSD1306_RES_SET();//RES set
#endif

    iot_ssd1306_write_byte(dev, 0xAE, SSD1306_CMD); //--turn off oled panel
    iot_ssd1306_write_byte(dev, 0x00, SSD1306_CMD); //---set low column address
    iot_ssd1306_write_byte(dev, 0x10, SSD1306_CMD); //---set high column address
    iot_ssd1306_write_byte(dev, 0x40, SSD1306_CMD); //--set start line address  Set Mapping RAM Display Start Line (0x00~0x3F)
    iot_ssd1306_write_byte(dev, 0x81, SSD1306_CMD); //--set contrast control register
    iot_ssd1306_write_byte(dev, 0xCF, SSD1306_CMD); // Set SEG Output Current Brightness
    iot_ssd1306_write_byte(dev, 0xA1, SSD1306_CMD); //--Set SEG/Column Mapping
    iot_ssd1306_write_byte(dev, 0xC0, SSD1306_CMD); //Set COM/Row Scan Direction
    iot_ssd1306_write_byte(dev, 0xA6, SSD1306_CMD); //--set normal display
    iot_ssd1306_write_byte(dev, 0xA8, SSD1306_CMD); //--set multiplex ratio(1 to 64)
    iot_ssd1306_write_byte(dev, 0x3f, SSD1306_CMD); //--1/64 duty
    iot_ssd1306_write_byte(dev, 0xD3, SSD1306_CMD); //-set display offset	Shift Mapping RAM Counter (0x00~0x3F)
    iot_ssd1306_write_byte(dev, 0x00, SSD1306_CMD); //-not offset
    iot_ssd1306_write_byte(dev, 0xd5, SSD1306_CMD); //--set display clock divide ratio/oscillator frequency
    iot_ssd1306_write_byte(dev, 0x80, SSD1306_CMD); //--set divide ratio, Set Clock as 100 Frames/Sec
    iot_ssd1306_write_byte(dev, 0xD9, SSD1306_CMD); //--set pre-charge period
    iot_ssd1306_write_byte(dev, 0xF1, SSD1306_CMD); //Set Pre-Charge as 15 Clocks & Discharge as 1 Clock
    iot_ssd1306_write_byte(dev, 0xDA, SSD1306_CMD); //--set com pins hardware configuration
    iot_ssd1306_write_byte(dev, 0x12, SSD1306_CMD);
    iot_ssd1306_write_byte(dev, 0xDB, SSD1306_CMD); //--set vcomh
    iot_ssd1306_write_byte(dev, 0x40, SSD1306_CMD); //Set VCOM Deselect Level
    iot_ssd1306_write_byte(dev, 0x20, SSD1306_CMD); //-Set Page Addressing Mode (0x00/0x01/0x02)
    iot_ssd1306_write_byte(dev, 0x02, SSD1306_CMD);
    iot_ssd1306_write_byte(dev, 0x8D, SSD1306_CMD); //--set Charge Pump enable/disable
    iot_ssd1306_write_byte(dev, 0x14, SSD1306_CMD); //--set(0x10) disable
    iot_ssd1306_write_byte(dev, 0xA4, SSD1306_CMD); // Disable Entire Display On (0xa4/0xa5)
    iot_ssd1306_write_byte(dev, 0xA6, SSD1306_CMD); // Disable Inverse Display On (0xa6/a7)
    iot_ssd1306_write_byte(dev, 0xAF, SSD1306_CMD); //--turn on oled panel

    ret = iot_ssd1306_clear_screen(dev, 0x00);
    return ret;
}

ssd1306_handle_t iot_ssd1306_create(i2c_bus_handle_t bus, uint16_t dev_addr)
{
    ssd1306_dev_t* dev = (ssd1306_dev_t*) calloc(1, sizeof(ssd1306_dev_t));
    dev->bus = bus;
    dev->dev_addr = dev_addr;
    iot_ssd1306_init((ssd1306_handle_t) dev);
    return (ssd1306_handle_t) dev;
}

esp_err_t iot_ssd1306_delete(ssd1306_handle_t dev, bool del_bus)
{
    ssd1306_dev_t* device = (ssd1306_dev_t*) dev;
    esp_err_t ret = ESP_OK;
    if (del_bus) {
        ret = iot_i2c_bus_delete(device->bus);
        if (ret == ESP_FAIL) {
            return ret;
        }
        device->bus = NULL;
    }
    free(device);
    return ret;
}

esp_err_t iot_ssd1306_refresh_gram(ssd1306_handle_t dev)
{
    ssd1306_dev_t* device = (ssd1306_dev_t*) dev;
    uint8_t i, j;
    esp_err_t ret;

    for (i = 0; i < 8; i++) {
        ret = iot_ssd1306_write_byte(dev, SSD1306_SET_PAGE_ADDR + i, SSD1306_CMD);
        if (ret == ESP_FAIL) {
            return ret;
        }
        iot_set_column_address(dev);
        for (j = 0; j < 128; j++) {
            ret = iot_ssd1306_write_byte(dev, device->s_chDisplayBuffer[j][i],
            SSD1306_DAT);
            if (ret == ESP_FAIL) {
                return ret;
            }
        }
    }
    return ret;
}

esp_err_t iot_ssd1306_clear_screen(ssd1306_handle_t dev, uint8_t chFill)
{
    ssd1306_dev_t* device = (ssd1306_dev_t*) dev;
    uint8_t i, j;
    esp_err_t ret;
    for (i = 0; i < 8; i++) {
        ret = iot_ssd1306_write_byte(dev, SSD1306_SET_PAGE_ADDR + i, SSD1306_CMD);
        if (ret == ESP_FAIL) {
            return ret;
        }
        iot_set_column_address(dev);
        for (j = 0; j < 128; j++)
            device->s_chDisplayBuffer[j][i] = chFill;
    }
    return ESP_OK;
}

