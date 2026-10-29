/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#pragma once

// From bq27220 Technical Reference Manual Table 2-1. Standard Commands
#define COMMAND_CONTROL                 0x00
#define COMMAND_AT_RATE                 0x02
#define COMMAND_AT_RATE_TIME_TO_EMPTY   0x04
#define COMMAND_TEMPERATURE             0x06
#define COMMAND_VOLTAGE                 0x08
#define COMMAND_BATTERY_STATUS          0x0A
#define COMMAND_CURRENT                 0x0C
#define COMMAND_REMAINING_CAPACITY      0x10
#define COMMAND_FULL_CHARGE_CAPACITY    0x12
#define COMMAND_AVERAGE_CURRENT         0x14
#define COMMAND_TIME_TO_EMPTY           0x16
#define COMMAND_TIME_TO_FULL            0x18
#define COMMAND_STANDBY_CURRENT         0x1A
#define COMMAND_STANDBY_TIME_TO_EMPTY   0x1C
#define COMMAND_MAX_LOAD_CURRENT        0x1E
#define COMMAND_MAX_LOAD_TIME_TO_EMPTY  0x20
#define COMMAND_RAW_COULOMB_COUNT       0x22
#define COMMAND_AVERAGE_POWER           0x24
#define COMMAND_INTERNAL_TEMPERATURE    0x28
#define COMMAND_CYCLE_COUNT             0x2A
#define COMMAND_STATE_OF_CHARGE         0x2C
#define COMMAND_STATE_OF_HEALTH         0x2E
#define COMMAND_CHARGE_VOLTAGE          0x30
#define COMMAND_CHARGE_CURRENT          0x32
#define COMMAND_BTP_DISCHARGE_SET       0x34
#define COMMAND_BTP_CHARGE_SET          0x36
#define COMMAND_OPERATION_STATUS        0x3A
#define COMMAND_DESIGN_CAPACITY         0x3C
#define COMMAND_SELECT_SUBCLASS         0x3E
#define COMMAND_MAC_DATA                0x40
#define COMMAND_MAC_DATA_SUM            0x60
#define COMMAND_MAC_DATA_LEN            0x61
#define COMMAND_ANALOG_COUNT            0x79
#define COMMAND_RAW_CURRENT             0x7A
#define COMMAND_RAW_VOLTAGE             0x7C
#define COMMAND_RAW_INT_TEMP            0x7E

// Table 2-2. Control() MAC Subcommands
#define CONTROL_CONTROL_STATUS          0x0000
#define CONTROL_DEVICE_NUMBER           0x0001
#define CONTROL_FW_VERSION              0x0002
#define CONTROL_HW_VERSION              0x0003
#define CONTROL_BOARD_OFFSET            0x0009
#define CONTROL_CC_OFFSET               0x000A
#define CONTROL_CC_OFFSET_SAVE          0x000B
#define CONTROL_OCV_CMD                 0x000C
#define CONTROL_BAT_INSERT              0x000D
#define CONTROL_BAT_REMOVE              0x000E
#define CONTROL_SET_SNOOZE              0x0013
#define CONTROL_CLEAR_SNOOZE            0x0014
#define CONTROL_SET_PROFILE_1           0x0015
#define CONTROL_SET_PROFILE_2           0x0016
#define CONTROL_SET_PROFILE_3           0x0017
#define CONTROL_SET_PROFILE_4           0x0018
#define CONTROL_SET_PROFILE_5           0x0019
#define CONTROL_SET_PROFILE_6           0x001A
#define CONTROL_CAL_TOGGLE              0x002D
#define CONTROL_SEALED                  0x0030
#define CONTROL_RESET                   0x0041
#define CONTROL_EXIT_CAL                0x0080
#define CONTROL_ENTER_CAL               0x0081
#define CONTROL_ENTER_CFG_UPDATE        0x0090
#define CONTROL_EXIT_CFG_UPDATE_REINIT  0x0091
#define CONTROL_EXIT_CFG_UPDATE         0x0092
#define CONTROL_RETURN_TO_ROM           0x0F00

#define UNSEALKEY1 (0x0414u)
#define UNSEALKEY2 (0x3672u)

#define FULLACCESSKEY (0xffffu)

typedef enum {
    OPERATION_STATUS_SEC_SEALED = 0b11,
    OPERATION_STATUS_SEC_UNSEALED = 0b10,
    OPERATION_STATUS_SEC_FULL = 0b01,
} bq27220_operation_status_sec_t;
