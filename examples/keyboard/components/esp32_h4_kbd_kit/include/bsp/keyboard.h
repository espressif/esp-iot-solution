/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/*!< Keyboard active level */
#define KBD_ATTIVE_LEVEL      1
#define KBD_TICKS_INTERVAL_US 1000

/*!< Keyboard matrix GPIO */
#define KBD_ROW_NUM  6
#define KBD_COL_NUM  15

#define KBD_ROW_IO_0 2
#define KBD_ROW_IO_1 3
#define KBD_ROW_IO_2 20
#define KBD_ROW_IO_3 19
#define KBD_ROW_IO_4 18
#define KBD_ROW_IO_5 17

#define KBD_COL_IO_0 26
#define KBD_COL_IO_1 27
#define KBD_COL_IO_2 29
#define KBD_COL_IO_3 16
#define KBD_COL_IO_4 1
#define KBD_COL_IO_5 0
#define KBD_COL_IO_6 39
#define KBD_COL_IO_7 38
#define KBD_COL_IO_8 37
#define KBD_COL_IO_9 36
#define KBD_COL_IO_10 35
#define KBD_COL_IO_11 34
#define KBD_COL_IO_12 33
#define KBD_COL_IO_13 30
#define KBD_COL_IO_14 31

#define KBD_OUTPUT_IOS {KBD_ROW_IO_0, KBD_ROW_IO_1, KBD_ROW_IO_2, KBD_ROW_IO_3, KBD_ROW_IO_4, KBD_ROW_IO_5}
#define KBD_INPUT_IOS  {KBD_COL_IO_0, KBD_COL_IO_1, KBD_COL_IO_2, KBD_COL_IO_3, KBD_COL_IO_4, KBD_COL_IO_5, \
                        KBD_COL_IO_6, KBD_COL_IO_7, KBD_COL_IO_8, KBD_COL_IO_9, KBD_COL_IO_10, KBD_COL_IO_11,\
                        KBD_COL_IO_12, KBD_COL_IO_13, KBD_COL_IO_14}

/*!< WS2812 power control GPIO */
#define KBD_WS2812_POWER_IO  15

/*!< Battery monitor GPIO */
#define KBD_BATTERY_MONITOR_ADC_UNIT ADC_UNIT_1
#define KBD_BATTERY_MONITOR_IO       28
#define KBD_BATTERY_MONITOR_CHANNEL  ADC_CHANNEL_0

#ifdef __cplusplus
}
#endif
