/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file dali_command.h
 * @brief DALI command code definitions (IEC 62386).
 *
 * This file defines all standard DALI command codes used as the second
 * byte of a forward frame. They are grouped by function:
 *
 *   - Direct arc-power control  (DAPC, S-bit = 0, value 0x00–0xFE)
 *   - Indirect control commands (S-bit = 1, 0x00–0x1F)
 *   - Configuration commands    (S-bit = 1, 0x20–0x8F, send-twice required)
 *   - Query commands            (S-bit = 1, 0x90–0xFF, backward frame expected)
 *   - Special bus commands      (first byte = 101CCCC1)
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------
 * Direct Arc-Power Control (DAPC)
 *
 * Used with is_cmd = false in dali_transaction().
 * Value range: 0x01–0xFE (1–254).
 *   0x00 = fade to minimum then switch off
 *   0xFE = MASK (no change; value is not loaded into the output register)
 * ------------------------------------------------------------------------- */

/** DAPC: Fade to minimum level then switch off. */
#define DALI_DAPC_OFF               0x00U
/** DAPC: Mask value — command is ignored; no change to output level. */
#define DALI_DAPC_MASK              0xFEU

/* -------------------------------------------------------------------------
 * Indirect Power Control Commands  (IEC 62386-102, Table 15)
 *
 * Used with is_cmd = true. None of these commands return a backward frame.
 * Commands 0x01 (UP) and 0x02 (DOWN) activate continuous dimming for 200 ms.
 * ------------------------------------------------------------------------- */

/** Switch off lamp without fading. */
#define DALI_CMD_OFF                0x00U
/** Dim up at the selected fade rate for 200 ms. */
#define DALI_CMD_UP                 0x01U
/** Dim down at the selected fade rate for 200 ms. */
#define DALI_CMD_DOWN               0x02U
/** Increase light output by one step (no fade); lamp stays off if already off. */
#define DALI_CMD_STEP_UP            0x03U
/** Decrease light output by one step (no fade); lamp stays on at minimum. */
#define DALI_CMD_STEP_DOWN          0x04U
/** Set output to the stored maximum level. */
#define DALI_CMD_RECALL_MAX_LEVEL   0x05U
/** Set output to the stored minimum level. */
#define DALI_CMD_RECALL_MIN_LEVEL   0x06U
/** Decrease light output by one step (no fade); switch off if already at minimum. */
#define DALI_CMD_STEP_DOWN_AND_OFF  0x07U
/** Switch on (if off) and increase one step (no fade). */
#define DALI_CMD_ON_AND_STEP_UP     0x08U
/** Enable DAPC sequence mode. */
#define DALI_CMD_ENABLE_DAPC_SEQ    0x09U

/* Go-to-scene commands: set lamp to the level stored in scene n (0–15). */
#define DALI_CMD_GO_TO_SCENE_0      0x10U
#define DALI_CMD_GO_TO_SCENE_1      0x11U
#define DALI_CMD_GO_TO_SCENE_2      0x12U
#define DALI_CMD_GO_TO_SCENE_3      0x13U
#define DALI_CMD_GO_TO_SCENE_4      0x14U
#define DALI_CMD_GO_TO_SCENE_5      0x15U
#define DALI_CMD_GO_TO_SCENE_6      0x16U
#define DALI_CMD_GO_TO_SCENE_7      0x17U
#define DALI_CMD_GO_TO_SCENE_8      0x18U
#define DALI_CMD_GO_TO_SCENE_9      0x19U
#define DALI_CMD_GO_TO_SCENE_10     0x1AU
#define DALI_CMD_GO_TO_SCENE_11     0x1BU
#define DALI_CMD_GO_TO_SCENE_12     0x1CU
#define DALI_CMD_GO_TO_SCENE_13     0x1DU
#define DALI_CMD_GO_TO_SCENE_14     0x1EU
#define DALI_CMD_GO_TO_SCENE_15     0x1FU

/* -------------------------------------------------------------------------
 * Configuration Commands  (IEC 62386-102, Table 16)
 *
 * Most of these commands must be sent twice within 100 ms (send_twice = true).
 * Use dali_transaction() with send_twice = true for those marked [2x].
 * Commands that load a DTR value require a prior DATA_TRANSFER_REGISTER
 * special command to populate the DTR.
 * ------------------------------------------------------------------------- */

/** [2x] Reset all device parameters to their power-on defaults. */
#define DALI_CMD_RESET                          0x20U
/** [2x] Store the current actual level in the DTR. */
#define DALI_CMD_STORE_ACTUAL_LEVEL             0x21U
/** [2x] Set maximum level from DTR. */
#define DALI_CMD_STORE_DTR_AS_MAX_LEVEL         0x2AU
/** [2x] Set minimum level from DTR. */
#define DALI_CMD_STORE_DTR_AS_MIN_LEVEL         0x2BU
/** [2x] Set system failure level from DTR. */
#define DALI_CMD_STORE_DTR_AS_FAIL_LEVEL        0x2CU
/** [2x] Set power-on level from DTR. */
#define DALI_CMD_STORE_DTR_AS_POWER_ON_LEVEL    0x2DU
/** [2x] Set fade time from DTR. */
#define DALI_CMD_STORE_DTR_AS_FADE_TIME         0x2EU
/** [2x] Set fade rate from DTR. */
#define DALI_CMD_STORE_DTR_AS_FADE_RATE         0x2FU

/* Store-DTR-as-scene commands: save DTR value as scene n level [2x]. */
#define DALI_CMD_STORE_DTR_AS_SCENE_0           0x40U
#define DALI_CMD_STORE_DTR_AS_SCENE_1           0x41U
#define DALI_CMD_STORE_DTR_AS_SCENE_2           0x42U
#define DALI_CMD_STORE_DTR_AS_SCENE_3           0x43U
#define DALI_CMD_STORE_DTR_AS_SCENE_4           0x44U
#define DALI_CMD_STORE_DTR_AS_SCENE_5           0x45U
#define DALI_CMD_STORE_DTR_AS_SCENE_6           0x46U
#define DALI_CMD_STORE_DTR_AS_SCENE_7           0x47U
#define DALI_CMD_STORE_DTR_AS_SCENE_8           0x48U
#define DALI_CMD_STORE_DTR_AS_SCENE_9           0x49U
#define DALI_CMD_STORE_DTR_AS_SCENE_10          0x4AU
#define DALI_CMD_STORE_DTR_AS_SCENE_11          0x4BU
#define DALI_CMD_STORE_DTR_AS_SCENE_12          0x4CU
#define DALI_CMD_STORE_DTR_AS_SCENE_13          0x4DU
#define DALI_CMD_STORE_DTR_AS_SCENE_14          0x4EU
#define DALI_CMD_STORE_DTR_AS_SCENE_15          0x4FU

/* Remove-from-scene commands [2x]. */
#define DALI_CMD_REMOVE_FROM_SCENE_0            0x50U
#define DALI_CMD_REMOVE_FROM_SCENE_1            0x51U
#define DALI_CMD_REMOVE_FROM_SCENE_2            0x52U
#define DALI_CMD_REMOVE_FROM_SCENE_3            0x53U
#define DALI_CMD_REMOVE_FROM_SCENE_4            0x54U
#define DALI_CMD_REMOVE_FROM_SCENE_5            0x55U
#define DALI_CMD_REMOVE_FROM_SCENE_6            0x56U
#define DALI_CMD_REMOVE_FROM_SCENE_7            0x57U
#define DALI_CMD_REMOVE_FROM_SCENE_8            0x58U
#define DALI_CMD_REMOVE_FROM_SCENE_9            0x59U
#define DALI_CMD_REMOVE_FROM_SCENE_10           0x5AU
#define DALI_CMD_REMOVE_FROM_SCENE_11           0x5BU
#define DALI_CMD_REMOVE_FROM_SCENE_12           0x5CU
#define DALI_CMD_REMOVE_FROM_SCENE_13           0x5DU
#define DALI_CMD_REMOVE_FROM_SCENE_14           0x5EU
#define DALI_CMD_REMOVE_FROM_SCENE_15           0x5FU

/* Add-to-group commands [2x]: assign device to group n (0–15). */
#define DALI_CMD_ADD_TO_GROUP_0                 0x60U
#define DALI_CMD_ADD_TO_GROUP_1                 0x61U
#define DALI_CMD_ADD_TO_GROUP_2                 0x62U
#define DALI_CMD_ADD_TO_GROUP_3                 0x63U
#define DALI_CMD_ADD_TO_GROUP_4                 0x64U
#define DALI_CMD_ADD_TO_GROUP_5                 0x65U
#define DALI_CMD_ADD_TO_GROUP_6                 0x66U
#define DALI_CMD_ADD_TO_GROUP_7                 0x67U
#define DALI_CMD_ADD_TO_GROUP_8                 0x68U
#define DALI_CMD_ADD_TO_GROUP_9                 0x69U
#define DALI_CMD_ADD_TO_GROUP_10                0x6AU
#define DALI_CMD_ADD_TO_GROUP_11                0x6BU
#define DALI_CMD_ADD_TO_GROUP_12                0x6CU
#define DALI_CMD_ADD_TO_GROUP_13                0x6DU
#define DALI_CMD_ADD_TO_GROUP_14                0x6EU
#define DALI_CMD_ADD_TO_GROUP_15                0x6FU

/* Remove-from-group commands [2x]. */
#define DALI_CMD_REMOVE_FROM_GROUP_0            0x70U
#define DALI_CMD_REMOVE_FROM_GROUP_1            0x71U
#define DALI_CMD_REMOVE_FROM_GROUP_2            0x72U
#define DALI_CMD_REMOVE_FROM_GROUP_3            0x73U
#define DALI_CMD_REMOVE_FROM_GROUP_4            0x74U
#define DALI_CMD_REMOVE_FROM_GROUP_5            0x75U
#define DALI_CMD_REMOVE_FROM_GROUP_6            0x76U
#define DALI_CMD_REMOVE_FROM_GROUP_7            0x77U
#define DALI_CMD_REMOVE_FROM_GROUP_8            0x78U
#define DALI_CMD_REMOVE_FROM_GROUP_9            0x79U
#define DALI_CMD_REMOVE_FROM_GROUP_10           0x7AU
#define DALI_CMD_REMOVE_FROM_GROUP_11           0x7BU
#define DALI_CMD_REMOVE_FROM_GROUP_12           0x7CU
#define DALI_CMD_REMOVE_FROM_GROUP_13           0x7DU
#define DALI_CMD_REMOVE_FROM_GROUP_14           0x7EU
#define DALI_CMD_REMOVE_FROM_GROUP_15           0x7FU

/** [2x] Store DTR value as the device's short address. */
#define DALI_CMD_STORE_DTR_AS_SHORT_ADDR        0x80U
/** [2x] Enable write to memory bank. */
#define DALI_CMD_ENABLE_WRITE_MEMORY            0x81U

/* -------------------------------------------------------------------------
 * Query Commands  (IEC 62386-102, Table 17)
 *
 * These commands return an 8-bit backward frame. Use dali_transaction()
 * with result != NULL. A result of DALI_RESULT_NO_REPLY means the device
 * did not respond.
 * ------------------------------------------------------------------------- */

/** Query: device status byte (bit-field, see IEC 62386-102 §8.4.1). */
#define DALI_CMD_QUERY_STATUS                   0x90U
/** Query: confirm that a control gear is present (returns 0xFF if yes). */
#define DALI_CMD_QUERY_CONTROL_GEAR             0x91U
/** Query: lamp failure flag (bit 1 of status byte). */
#define DALI_CMD_QUERY_LAMP_FAILURE             0x92U
/** Query: lamp arc power on flag (bit 2 of status byte). */
#define DALI_CMD_QUERY_LAMP_POWER_ON            0x93U
/** Query: limit error flag (bit 3 of status byte). */
#define DALI_CMD_QUERY_LIMIT_ERROR              0x94U
/** Query: reset state flag (bit 4 of status byte). */
#define DALI_CMD_QUERY_RESET_STATE              0x95U
/** Query: missing short address flag (bit 5 of status byte). */
#define DALI_CMD_QUERY_MISSING_SHORT_ADDR       0x96U
/** Query: DALI version number. */
#define DALI_CMD_QUERY_VERSION                  0x97U
/** Query: content of the Data Transfer Register (DTR). */
#define DALI_CMD_QUERY_CONTENT_DTR              0x98U
/** Query: device type number (e.g., 0 = fluorescent, 6 = LED). */
#define DALI_CMD_QUERY_DEVICE_TYPE              0x99U
/** Query: physical minimum arc-power level. */
#define DALI_CMD_QUERY_PHY_MIN_LEVEL            0x9AU
/** Query: power failure flag (bit 7 of status byte). */
#define DALI_CMD_QUERY_POWER_FAILURE            0x9BU
/** Query: content of Data Transfer Register 1 (DTR1). */
#define DALI_CMD_QUERY_CONTENT_DTR1             0x9CU
/** Query: content of Data Transfer Register 2 (DTR2). */
#define DALI_CMD_QUERY_CONTENT_DTR2             0x9DU

/** Query: current actual arc-power output level (0x00–0xFF). */
#define DALI_CMD_QUERY_ACTUAL_LEVEL             0xA0U
/** Query: stored maximum arc-power level. */
#define DALI_CMD_QUERY_MAX_LEVEL                0xA1U
/** Query: stored minimum arc-power level. */
#define DALI_CMD_QUERY_MIN_LEVEL                0xA2U
/** Query: stored power-on arc-power level. */
#define DALI_CMD_QUERY_POWER_ON_LEVEL           0xA3U
/** Query: stored system-failure arc-power level. */
#define DALI_CMD_QUERY_SYSTEM_FAILURE_LEVEL     0xA4U
/** Query: fade time and fade rate (high nibble = fade time, low nibble = fade rate). */
#define DALI_CMD_QUERY_FADE_TIME_RATE           0xA5U

/* Query scene level for scene n (0–15). */
#define DALI_CMD_QUERY_SCENE_LEVEL_0            0xB0U
#define DALI_CMD_QUERY_SCENE_LEVEL_1            0xB1U
#define DALI_CMD_QUERY_SCENE_LEVEL_2            0xB2U
#define DALI_CMD_QUERY_SCENE_LEVEL_3            0xB3U
#define DALI_CMD_QUERY_SCENE_LEVEL_4            0xB4U
#define DALI_CMD_QUERY_SCENE_LEVEL_5            0xB5U
#define DALI_CMD_QUERY_SCENE_LEVEL_6            0xB6U
#define DALI_CMD_QUERY_SCENE_LEVEL_7            0xB7U
#define DALI_CMD_QUERY_SCENE_LEVEL_8            0xB8U
#define DALI_CMD_QUERY_SCENE_LEVEL_9            0xB9U
#define DALI_CMD_QUERY_SCENE_LEVEL_10           0xBAU
#define DALI_CMD_QUERY_SCENE_LEVEL_11           0xBBU
#define DALI_CMD_QUERY_SCENE_LEVEL_12           0xBCU
#define DALI_CMD_QUERY_SCENE_LEVEL_13           0xBDU
#define DALI_CMD_QUERY_SCENE_LEVEL_14           0xBEU
#define DALI_CMD_QUERY_SCENE_LEVEL_15           0xBFU

/** Query: group membership mask for groups 0–7 (bit n = member of group n). */
#define DALI_CMD_QUERY_GROUPS_0_7               0xC0U
/** Query: group membership mask for groups 8–15. */
#define DALI_CMD_QUERY_GROUPS_8_15              0xC1U
/** Query: high byte of the 24-bit random address. */
#define DALI_CMD_QUERY_RANDOM_ADDR_H            0xC2U
/** Query: middle byte of the 24-bit random address. */
#define DALI_CMD_QUERY_RANDOM_ADDR_M            0xC3U
/** Query: low byte of the 24-bit random address. */
#define DALI_CMD_QUERY_RANDOM_ADDR_L            0xC4U
/** Query: read a byte from the memory bank at the current address pointer. */
#define DALI_CMD_READ_MEMORY_LOCATION           0xC5U
/** Query: extended version number for a specific device type. */
#define DALI_CMD_QUERY_EXTENDED_VERSION         0xFFU

/* -------------------------------------------------------------------------
 * Special Commands  (IEC 62386-102, Table 18)
 *
 * These commands use DALI_ADDR_SPECIAL addressing. The first byte is fixed
 * per command (101CCCC1) and is passed directly as the addr parameter.
 * Most are used during the commissioning (addressing) procedure.
 * ------------------------------------------------------------------------- */

/**
 * @defgroup dali_special_cmd Special command first-byte values
 *
 * Pass these as the @p addr argument to dali_transaction() with
 * @p addr_type = DALI_ADDR_SPECIAL.
 * @{
 */

/** Terminate an ongoing initialise or commission sequence. */
#define DALI_SPECIAL_TERMINATE              0xA1U
/** Load a value into the Data Transfer Register (DTR); data in second byte. */
#define DALI_SPECIAL_DATA_TRANSFER_REG      0xA3U
/** [2x] Enter addressing mode; second byte: 0xFF = all, 0x00 = unaddressed. */
#define DALI_SPECIAL_INITIALISE             0xA5U
/** [2x] Generate a new 24-bit random address for all devices in INIT mode. */
#define DALI_SPECIAL_RANDOMISE              0xA7U
/** Query: return YES (0xFF) if any device random address ≤ search address. */
#define DALI_SPECIAL_COMPARE                0xA9U
/** [2x] Withdraw the selected device from the addressing process. */
#define DALI_SPECIAL_WITHDRAW               0xABU
/** Set the high byte of the search address; data in second byte. */
#define DALI_SPECIAL_SEARCH_ADDR_H          0xB1U
/** Set the middle byte of the search address; data in second byte. */
#define DALI_SPECIAL_SEARCH_ADDR_M          0xB3U
/** Set the low byte of the search address; data in second byte. */
#define DALI_SPECIAL_SEARCH_ADDR_L          0xB5U
/** [2x] Program a short address into the device matching the search address. */
#define DALI_SPECIAL_PROGRAM_SHORT_ADDR     0xB7U
/** Verify that the selected device has the given short address. */
#define DALI_SPECIAL_VERIFY_SHORT_ADDR      0xB9U
/** Query: return the short address of the device matching the search address. */
#define DALI_SPECIAL_QUERY_SHORT_ADDR       0xBBU
/** Select a device via physical means (e.g., pressing a button on the gear). */
#define DALI_SPECIAL_PHYSICAL_SELECTION     0xBDU
/** Enable a device-type-specific extension; device type in second byte. */
#define DALI_SPECIAL_ENABLE_DEVICE_TYPE     0xC1U
/** Load a value into DTR1; data in second byte. */
#define DALI_SPECIAL_DATA_TRANSFER_REG1     0xC3U
/** Load a value into DTR2; data in second byte. */
#define DALI_SPECIAL_DATA_TRANSFER_REG2     0xC5U
/** [2x] Write a byte to the memory bank at the current address pointer. */
#define DALI_SPECIAL_WRITE_MEMORY_LOCATION  0xC7U

/** @} */ /* dali_special_cmd */

#ifdef __cplusplus
}
#endif
