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
 * Used with config->is_cmd = false in dali_master_do_transaction().
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
 * Used with config->is_cmd = true. None of these commands return a backward frame.
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
/** @cond */
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
/** @endcond */
/* -------------------------------------------------------------------------
 * Configuration Commands  (IEC 62386-102, Table 16)
 *
 * Most of these commands must be sent twice within 100 ms (send_twice = true).
 * Use dali_master_do_transaction() with config->send_twice = true for those marked [2x].
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
/** @cond */
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
/** @endcond */

/* Remove-from-scene commands [2x]. */
/** @cond */
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
/** @endcond */

/* Add-to-group commands [2x]: assign device to group n (0–15). */
/** @cond */
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
/** @endcond */

/* Remove-from-group commands [2x]. */
/** @cond */
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
/** @endcond */

/** [2x] Store DTR value as the device's short address. */
#define DALI_CMD_STORE_DTR_AS_SHORT_ADDR        0x80U
/** [2x] Enable write to memory bank. */
#define DALI_CMD_ENABLE_WRITE_MEMORY            0x81U

/* -------------------------------------------------------------------------
 * Query Commands  (IEC 62386-102, Table 17)
 *
 * These commands return an 8-bit backward frame. Use dali_master_do_transaction()
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
/** @cond */
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
/** @endcond */

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
 * Pass these as the config->addr field to dali_master_do_transaction() with
 * config->addr_type = DALI_ADDR_SPECIAL.
 * @{
 */

/** Terminate an ongoing initialize or commission sequence. */
#define DALI_SPECIAL_TERMINATE              0xA1U
/** Load a value into the Data Transfer Register (DTR); data in second byte. */
#define DALI_SPECIAL_DATA_TRANSFER_REG      0xA3U
/** [2x] Enter addressing mode; second byte: 0x00 = all, 0xFF = unaddressed. */
#define DALI_SPECIAL_INITIALIZE             0xA5U
/** [2x] Generate a new 24-bit random address for all devices in INIT mode. */
#define DALI_SPECIAL_RANDOMIZE              0xA7U
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

/* -------------------------------------------------------------------------
 * Part 209 Commands  (IEC 62386 Part 209 — Color Control, DT8)
 *
 * Usage prerequisite:
 *   1. Load DTR0/DTR1/DTR2 via DALI_SPECIAL_DATA_TRANSFER_REG / REG1 / REG2
 *      before any set-color command.
 *   2. Send DALI_SPECIAL_ENABLE_DEVICE_TYPE (0xC1) with data=8 immediately
 *      before each Part 209 application-extended command.
 *   3. Call DALI_209_ACTIVATE (0xE2) after loading DTRs to latch the new color.
 *
 * Color modes (IEC 62386-209 §8.1):
 *   - XY chromaticity : load X/Y into DTR0/DTR1 and use SET_TEMPORARY_X/Y.
 *   - Color temperature: load Tc in Mirek (DTR0=lo, DTR1=hi);
 *                         Mirek = 1 000 000 / Kelvin  (153 ≈ 6500 K, 370 ≈ 2700 K)
 *   - RGB             : load R→DTR0, G→DTR1, B→DTR2.
 *   - RGBWAF          : load R/G/B first, then W/A/F, then ACTIVATE.
 *
 * All command bytes below are the second byte of a forward frame
 * (is_cmd = true).  Query commands return an 8-bit backward frame unless
 * otherwise noted.
 * ------------------------------------------------------------------------- */

/**
 * @defgroup dali_part209_cmd DALI Part 209 – Color Control command codes (DT8)
 * @{
 */

/* --- Immediate color control commands ------------------------------------ */

/**
 * Set temporary X coordinate from DTR0 (low byte) and DTR1 (high byte).
 * Call DALI_209_ACTIVATE afterwards to latch the new XY value.
 */
#define DALI_209_SET_TEMPORARY_X_COORDINATE         0xE0U
/**
 * Set temporary Y coordinate from DTR0 (low byte) and DTR1 (high byte).
 * Call DALI_209_ACTIVATE afterwards to latch the new XY value.
 */
#define DALI_209_SET_TEMPORARY_Y_COORDINATE         0xE1U

/**
 * Activate the color value previously loaded into the temporary registers.
 * Must be sent after pre-loading DTRs for any color mode.
 */
#define DALI_209_ACTIVATE                           0xE2U

/* --- XY chromaticity step commands --------------------------------------- */

/** Increase X-coordinate by one step (no DTR pre-load needed). */
#define DALI_209_X_COORD_STEP_UP                    0xE3U
/** Decrease X-coordinate by one step. */
#define DALI_209_X_COORD_STEP_DOWN                  0xE4U
/** Increase Y-coordinate by one step. */
#define DALI_209_Y_COORD_STEP_UP                    0xE5U
/** Decrease Y-coordinate by one step. */
#define DALI_209_Y_COORD_STEP_DOWN                  0xE6U

/**
 * Set temporary color temperature from DTR0 (low byte) and DTR1 (high byte).
 * Unit: Mirek (= 1 000 000 / Kelvin).  Range: 1–65534 (0x0000 and 0xFFFF reserved).
 * Call DALI_209_ACTIVATE afterwards to latch the new Tc.
 */
#define DALI_209_SET_COLOR_TEMPERATURE             0xE7U

/* --- Color temperature step commands ------------------------------------ */

/** Increase color temperature (Tc) by one step toward cooler (lower Mirek). */
#define DALI_209_COLOR_TEMPERATURE_STEP_COOLER     0xE8U
/** Increase color temperature (Tc) by one step toward warmer (higher Mirek). */
#define DALI_209_COLOR_TEMPERATURE_STEP_WARMER     0xE9U

/**
 * Set temporary primary-N dimlevel for primary channel N (0–5).
 * DTR0 = dimlevel (0x00–0xFE), DTR1 = primary index N.
 * Call DALI_209_ACTIVATE afterwards.
 */
#define DALI_209_SET_PRIMARY_N_DIMLEVEL             0xEAU

/**
 * Set temporary RGB dimlevel.
 * Load: DTR0 = R, DTR1 = G, DTR2 = B (each 0x00–0xFE, 0xFF = no change).
 * Call DALI_209_ACTIVATE afterwards.
 */
#define DALI_209_SET_TEMPORARY_RGB_DIMLEVEL         0xEBU

/**
 * Set temporary WAF dimlevel.
 * Load: DTR0 = W, DTR1 = A, DTR2 = F (each 0x00–0xFE, 0xFF = no change).
 * Call DALI_209_ACTIVATE afterwards.
 */
#define DALI_209_SET_TEMPORARY_WAF_DIMLEVEL         0xECU

/**
 * Select which RGBWAF channels are controlled by the RGB/WAF temporary values.
 */
#define DALI_209_SET_TEMPORARY_RGBWAF_CONTROL       0xEDU

/** Copy the reported color value into the temporary color registers. */
#define DALI_209_COPY_REPORT_TO_TEMPORARY           0xEEU

/* --- Stored color configuration commands [2x] --------------------------- */

/** [2x] Store DTR0/DTR1 as the physical cool-white color temperature limit (Tc_coolest). */
#define DALI_209_STORE_COLOR_TEMPERATURE_LIMIT_COOL    0xF0U
/** [2x] Store DTR0/DTR1 as the physical warm-white color temperature limit (Tc_warmest). */
#define DALI_209_STORE_COLOR_TEMPERATURE_LIMIT_WARM    0xF1U

/* --- Query commands (return 8-bit backward frame) ------------------------- */

/**
 * Query the device color status byte (IEC 62386-209 §8.3.1).
 * Bit fields:
 *   [0]   Color mode active (1 = active)
 *   [1]   Color temperature free running
 *   [2]   Automatic activation enabled
 *   [3]   Color temperature step active
 *   [4]   XY-coordinate supported
 *   [5]   Color temperature supported
 *   [6]   Primary N supported
 *   [7]   RGBWAF supported
 */
#define DALI_209_QUERY_COLOR_STATUS                0xF8U

/**
 * Query the color capabilities bitmask.
 * Indicates which color modes the device supports (same bit layout as status byte [4:7]).
 */
#define DALI_209_QUERY_COLOR_CAPABILITIES          0xF9U

/**
 * Query a color value by index (placed in DTR0 before issuing this command).
 * Returns the low byte; repeat with incremented index for high byte.
 *
 * DTR0 index map (IEC 62386-209 Table 4):
 *   0x00 = X-coordinate (lo byte)    0x01 = X-coordinate (hi byte)
 *   0x02 = Y-coordinate (lo byte)    0x03 = Y-coordinate (hi byte)
 *   0x04 = Tc (lo byte)              0x05 = Tc (hi byte)
 *   0x06 = primary-0 dimlevel        …
 *   0x10 = R dimlevel                0x11 = G dimlevel  0x12 = B dimlevel
 *   0x13 = W dimlevel                0x14 = A dimlevel  0x15 = F dimlevel
 *
 * To read color temperature (Tc):
 *   1. SET DTR0 = 0x04 (DALI_SPECIAL_DATA_TRANSFER_REG, data=0x04)
 *   2. Issue DALI_209_QUERY_COLOR_VALUE → returns Tc low byte
 *   3. SET DTR0 = 0x05
 *   4. Issue DALI_209_QUERY_COLOR_VALUE → returns Tc high byte
 *   Tc_mirek = (hi << 8) | lo
 */
#define DALI_209_QUERY_COLOR_VALUE                 0xFAU

/** Query the cool-white color temperature limit (Tc_coolest) low byte (pre-load DTR0=index). */
#define DALI_209_QUERY_COLOR_TEMPERATURE_LIMIT_COOL    0xFBU
/** Query the warm-white color temperature limit (Tc_warmest) low byte. */
#define DALI_209_QUERY_COLOR_TEMPERATURE_LIMIT_WARM    0xFCU

/** Query the extended version number for Part 209. */
#define DALI_209_QUERY_EXTENDED_VERSION             0xFFU

/** @} */ /* dali_part209_cmd */

/* -------------------------------------------------------------------------
 * Part 103 Commands  (IEC 62386 Part 103 — Input Devices)
 *
 * Input devices (sensors, push-buttons, etc.) use a separate address space
 * from control gear.  Their forward frames are constructed identically to
 * Part 102 but the command byte range and the special command set differ.
 *
 * Address byte construction for input devices:
 *   Short:     0AAAAAAS  (A = 0–63, S = 1 for command, 0 for instance addressing)
 *   Group:     100AAAAS  (A = 0–15)
 *   Broadcast: 1111111S
 *   Instance:  the second byte carries the instance number (0–31)
 *
 * Device-level query commands (second byte, is_cmd = true, BF expected):
 * -------------------------------------------------------------------------*/

/**
 * @defgroup dali_part103_cmd DALI Part 103 – Input Device command codes
 * @{
 */

/* --- Part 103 device-level configuration commands [2x] ------------------- */

/** [2x] Start quiescent mode: suppress input-device/application-controller activity. */
#define DALI_103_START_QUIESCENT_MODE               0x1DU
/** [2x] Stop quiescent mode. */
#define DALI_103_STOP_QUIESCENT_MODE                0x1EU
/** [2x] Reset all device parameters to factory defaults (Part 103). */
#define DALI_103_RESET                              0x10U
/** [2x] Set input device short address from DTR0. */
#define DALI_103_SET_SHORT_ADDRESS                  0x14U
/** [2x] Enable write to memory bank. */
#define DALI_103_ENABLE_WRITE_MEMORY                0x15U
/** [2x] Set event priority: DTR0 = priority. */
#define DALI_103_SET_EVENT_PRIORITY                 0x61U
/** [2x] Enable instance. */
#define DALI_103_ENABLE_INSTANCE                    0x62U
/** [2x] Disable instance. */
#define DALI_103_DISABLE_INSTANCE                   0x63U
/** [2x] Set primary instance group. */
#define DALI_103_SET_PRIMARY_INSTANCE_GROUP         0x64U
/** [2x] Set instance group 1. */
#define DALI_103_SET_INSTANCE_GROUP_1               0x65U
/** [2x] Set instance group 2. */
#define DALI_103_SET_INSTANCE_GROUP_2               0x66U
/** [2x] Set event scheme. */
#define DALI_103_SET_EVENT_SCHEME                   0x67U
/** [2x] Set event filter. */
#define DALI_103_SET_EVENT_FILTER                   0x68U

/* --- Part 103 device-level query commands (BF expected) ------------------ */

/** Query: input device status byte. */
#define DALI_103_QUERY_DEVICE_STATUS                0x30U
/** Query: application controller error. */
#define DALI_103_QUERY_APPLICATION_CONTROLLER_ERROR 0x31U
/** Query: input device error. */
#define DALI_103_QUERY_INPUT_DEVICE_ERROR           0x32U
/** Query: missing short address. */
#define DALI_103_QUERY_MISSING_SHORT_ADDRESS        0x33U
/** Query: version number. */
#define DALI_103_QUERY_VERSION_NUMBER               0x34U
/** Query: number of instances. */
#define DALI_103_QUERY_NUMBER_OF_INSTANCES          0x35U
/** Query: content of DTR0. */
#define DALI_103_QUERY_CONTENT_DTR0                 0x36U
/** Query: content of DTR1. */
#define DALI_103_QUERY_CONTENT_DTR1                 0x37U
/** Query: content of DTR2. */
#define DALI_103_QUERY_CONTENT_DTR2                 0x38U
/** Query: high byte of the device random address. */
#define DALI_103_QUERY_RANDOM_ADDRESS_H             0x39U
/** Query: middle byte of the device random address. */
#define DALI_103_QUERY_RANDOM_ADDRESS_M             0x3AU
/** Query: low byte of the device random address. */
#define DALI_103_QUERY_RANDOM_ADDRESS_L             0x3BU
/** Query: device groups 0–7 membership mask. */
#define DALI_103_QUERY_GROUPS_0_7                   0x41U
/** Query: device groups 8–15 membership mask. */
#define DALI_103_QUERY_GROUPS_8_15                  0x42U
/** Query: device groups 16–23 membership mask. */
#define DALI_103_QUERY_GROUPS_16_23                 0x43U
/** Query: device groups 24–31 membership mask. */
#define DALI_103_QUERY_GROUPS_24_31                 0x44U
/** Query: input device capabilities. */
#define DALI_103_QUERY_INPUT_DEVICE_CAPABILITIES    0x46U
/** Query: short address is not a standard device-level command; use special QUERY_SHORT_ADDRESS during commissioning. */
#define DALI_103_QUERY_SHORT_ADDRESS                0x3FU

/* --- Part 103 instance-level commands ------------------------------------ */

/** Query: input value of the addressed instance. */
#define DALI_103_QUERY_INSTANCE_TYPE                0x80U
/** Query: resolution of the instance sensor value. */
#define DALI_103_QUERY_RESOLUTION                   0x81U
/** Query: current input value. */
#define DALI_103_QUERY_INPUT_INST_VALUE             0x8CU
/** Query: latched input value. */
#define DALI_103_QUERY_INPUT_INST_VALUE_LATCH       0x8DU
/** Query: whether the instance is enabled. */
#define DALI_103_QUERY_INSTANCE_ENABLED             0x86U
/** Query: primary instance group. */
#define DALI_103_QUERY_PRIMARY_INSTANCE_GROUP       0x88U
/** Query: event scheme for this instance. */
#define DALI_103_QUERY_INSTANCE_SCHEME              0x8BU
/** Query event filter bits 0-7 */
#define DALI_103_QUERY_EVENT_FILTER_ZERO_TO_SEVEN   0x90U
/** Query event filter bits 8-15 */
#define DALI_103_QUERY_EVENT_FILTER_EIGHT_TO_FIFTEEN    0x91U
/** Query event filter bits 16-23 */
#define DALI_103_QUERY_EVENT_FILTER_SIXTEEN_TO_TWENTYTHREE 0x92U

/* --- Part 103 special commands (input device addressing) ----------------- */

/**
 * @defgroup dali_103_special Part 103 special command instance-byte values
 *
 * IEC 62386-103 control-device special commands are 24-bit frames:
 * [0xC1][special-command-id][parameter].
 * @{
 */
/**< Special address for Part 103 */
#define DALI_103_SPECIAL_ADDR                   0xC1U
/** Terminate an ongoing input device commissioning sequence. */
#define DALI_103_SPECIAL_TERMINATE              0x00U
/** [2x] Enter input device addressing mode; 0xFF = all, 0x00 = unaddressed. */
#define DALI_103_SPECIAL_INITIALIZE             0x01U
/** [2x] Generate new 24-bit random address for all input devices in INIT mode. */
#define DALI_103_SPECIAL_RANDOMIZE              0x02U
/** Query: return YES (0xFF) if any device random address ≤ search address. */
#define DALI_103_SPECIAL_COMPARE                0x03U
/** Withdraw the selected input device from the addressing process. */
#define DALI_103_SPECIAL_WITHDRAW               0x04U
/** Set the high byte of the search address. */
#define DALI_103_SPECIAL_SEARCH_ADDR_H          0x05U
/** Set the middle byte of the search address. */
#define DALI_103_SPECIAL_SEARCH_ADDR_M          0x06U
/** Set the low byte of the search address. */
#define DALI_103_SPECIAL_SEARCH_ADDR_L          0x07U
/** [2x] Program a raw 6-bit short address (0..63) into the selected input device. */
#define DALI_103_SPECIAL_PROGRAM_SHORT_ADDR     0x08U
/** Verify that the device matching the search address has the given short address. */
#define DALI_103_SPECIAL_VERIFY_SHORT_ADDR      0x09U
/** Query: return the short address of the device matching the search address. */
#define DALI_103_SPECIAL_QUERY_SHORT_ADDR       0x0AU
/** Write enable for instance configuration commands. */
#define DALI_103_SPECIAL_WRITE_MEMORY_LOCATION  0x20U
/** Load a value into DTR0. */
#define DALI_103_SPECIAL_DTR0                   0x30U
/** Load a value into DTR1. */
#define DALI_103_SPECIAL_DTR1                   0x31U
/** Load a value into DTR2. */
#define DALI_103_SPECIAL_DTR2                   0x32U

/** @} */ /* dali_103_special */
/** @} */ /* dali_part103_cmd */

/* -------------------------------------------------------------------------
 * Part 303 Commands  (IEC 62386 Part 303 — Occupancy Sensor, instance type 3)
 *
 * The DLS-203-P PIR sensor implements instance type 3 (occupancy) on
 * at least instance 0.  Configuration commands use DALI_103 instance-level
 * addressing: send DALI_103_SPECIAL_SET_INSTANCE first to select the
 * occupancy instance, then issue the Part 303 command.
 *
 * All command bytes below are the second byte of a forward frame
 * (is_cmd = true, addr_type = DALI_ADDR_SHORT/GROUP/BROADCAST).
 * -------------------------------------------------------------------------*/

/**
 * @defgroup dali_part303_cmd DALI Part 303 – Occupancy Sensor command codes
 * @{
 */

/* --- Part 303 configuration commands [2x] -------------------------------- */

/** [2x] Set hold timer: DTR0 = value (unit: 10 s). */
#define DALI_303_SET_HOLD_TIMER                     0x21U
/** [2x] Set report timer: DTR0 = timer value. */
#define DALI_303_SET_REPORT_TIMER                   0x22U
/** [2x] Set deadtime timer: DTR0 = value. */
#define DALI_303_SET_DEADTIME_TIMER                 0x23U
/** Cancel hold timer. */
#define DALI_303_CANCEL_HOLD_TIMER                  0x24U

/* --- Part 303 query commands (BF expected) ------------------------------- */

/** Query: current deadtime timer value. */
#define DALI_303_QUERY_DEADTIME_TIMER               0x2CU
/** Query: current hold timer value. */
#define DALI_303_QUERY_HOLD_TIMER                   0x2DU
/** Query: current report timer value. */
#define DALI_303_QUERY_REPORT_TIMER                 0x2EU
/** Query: whether movement catching is enabled. */
#define DALI_303_QUERY_CATCHING                     0x2FU
/**
 * Query: occupancy state.
 * Returns 0xFF = occupied, 0x00 = unoccupied (no reply = device absent).
 */
#define DALI_303_QUERY_OCCUPANCY                    0xF1U
/**
 * Query: occupancy state latch (reads and clears the latched occupancy event).
 * Returns 0xFF if an occupancy event was latched since the last read.
 */
#define DALI_303_QUERY_OCCUPANCY_LATCH              0xF2U

/** @} */ /* dali_part303_cmd */

/* -------------------------------------------------------------------------
 * Part 304 Commands  (IEC 62386 Part 304 — Light Sensor, instance type 4)
 *
 * The DLS-203-P includes a daylight sensor (2–65000 Lux) that maps to
 * Part 304 (illuminance input device instance).  Use
 * DALI_103_SPECIAL_SET_INSTANCE to select the illuminance instance (typically
 * instance 1 on devices that combine PIR + daylight), then issue Part 304
 * commands.
 *
 * Illuminance is reported via DALI event messages (push model).  The device
 * autonomously transmits readings when the value changes beyond the configured
 * hysteresis; direct polling of the current value is not standardised.
 * -------------------------------------------------------------------------*/

/**
 * @defgroup dali_part304_cmd DALI Part 304 – Light Sensor command codes
 * @{
 */

/* --- Part 304 configuration commands [2x] -------------------------------- */

/** [2x] Set report timer: DTR0 = value. */
#define DALI_304_SET_REPORT_TIMER                   0x30U
/** [2x] Set hysteresis: DTR0 = value. */
#define DALI_304_SET_HYSTERESIS                     0x31U
/** [2x] Set deadtime timer: DTR0 = value. */
#define DALI_304_SET_DEADTIME_TIMER                 0x32U
/** [2x] Set minimum hysteresis: DTR0 = value. */
#define DALI_304_SET_HYSTERESIS_MIN                 0x33U

/* --- Part 304 query commands (BF expected) ------------------------------- */

/** Query: minimum hysteresis value. */
#define DALI_304_QUERY_HYSTERESIS_MIN                   0x3CU
/** Query: deadtime timer value. */
#define DALI_304_QUERY_DEADTIME_TIMER                   0x3DU
/** Query: report timer value. */
#define DALI_304_QUERY_REPORT_TIMER                     0x3EU
/** Query: hysteresis value. */
#define DALI_304_QUERY_HYSTERESIS                       0x3FU
/** @} */ /* dali_part304_cmd */

#ifdef __cplusplus
}
#endif
