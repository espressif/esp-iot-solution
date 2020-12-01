/*
 * This file is subject to the terms of the GFX License. If a copy of
 * the license was not distributed with this file, you can obtain one at:
 *
 *              http://ugfx.org/license.html
 */

#ifndef _TEMPLATE_H
#define _TEMPLATE_H

#define TEMPLATE_CONTRAST             0x81
#define TEMPLATE_ALLON_NORMAL         0xA4
#define TEMPLATE_ALLON                0xA5
#define TEMPLATE_POSITIVE_DISPLAY     0xA6
#define TEMPLATE_INVERT_DISPLAY       0xA7
#define TEMPLATE_DISPLAY_OFF          0xAE
#define TEMPLATE_DISPLAY_ON           0xAF

#define TEMPLATE_LCD_BIAS_7           0xA3
#define TEMPLATE_LCD_BIAS_9           0xA2

#define TEMPLATE_ADC_NORMAL           0xA0
#define TEMPLATE_ADC_REVERSE          0xA1

#define TEMPLATE_COM_SCAN_INC         0xC0
#define TEMPLATE_COM_SCAN_DEC         0xC8

#define TEMPLATE_START_LINE           0x40
#define TEMPLATE_PAGE                 0xB0
#define TEMPLATE_COLUMN_MSB           0x10
#define TEMPLATE_COLUMN_LSB           0x00
#define TEMPLATE_RMW                  0xE0

#define TEMPLATE_RESISTOR_RATIO       0x20
#define TEMPLATE_POWER_CONTROL        0x28

#endif /* _TEMPLATE_H */
