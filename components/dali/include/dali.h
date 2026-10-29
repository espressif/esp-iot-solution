/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file dali.h
 * @brief DALI (IEC 62386) component — Umbrella Header.
 *
 * This is the single public include that consumers of this component should
 * use.  It aggregates the enabled sub-module headers in dependency order.
 * Which sub-modules are included depends on Kconfig (see Kconfig in this
 * component's directory):
 *
 *   dali_system_components.h  — Part 101: physical layer, RMT driver, base types
 *                               (always included)
 *   dali_control_gear.h       — Part 102: control-gear commissioning
 *                               (CONFIG_DALI_PART102_ENABLED)
 *   dali_color_control_dt8.h  — Part 209: DT8 color/CCT control
 *                               (CONFIG_DALI_PART209_ENABLED, requires Part 102)
 *   dali_control_device.h     — Part 103: input-device commissioning & queries
 *                               (CONFIG_DALI_PART103_ENABLED)
 *   dali_device_sensors.h     — Part 303/304: occupancy & light-sensor helpers
 *                               (CONFIG_DALI_PART303_304_ENABLED, requires Part 103)
 *
 * ### Signal characteristics (IEC 62386 Part 101)
 * - Half-period Te:  416.67 µs ± 10 %
 * - Logic 1:  1 Te low  → 1 Te high
 * - Logic 0:  1 Te high → 1 Te low
 * - Start bit: logic 1
 * - Stop bits: 2 Te high
 *
 * ### Frame timing rules
 * - Forward frame (FF):  1 start + 16 data + 2 stop = 38 Te
 * - Backward frame (BF): 1 start +  8 data + 2 stop = 22 Te
 * - No-reply IFG:  next FF > 22 Te after current FF ends
 * - Reply IFG:     BF arrives 7–22 Te after FF; next FF > 22 Te after BF
 * - Send-twice:    second FF within 100 ms of first FF
 */

#pragma once

#include "sdkconfig.h"
#include "dali_system_components.h"

#if CONFIG_DALI_PART102_ENABLED
#include "dali_control_gear.h"
#endif

#if CONFIG_DALI_PART209_ENABLED
#include "dali_color_control_dt8.h"
#endif

#if CONFIG_DALI_PART103_ENABLED
#include "dali_control_device.h"
#endif

#if CONFIG_DALI_PART303_304_ENABLED
#include "dali_device_sensors.h"
#endif
