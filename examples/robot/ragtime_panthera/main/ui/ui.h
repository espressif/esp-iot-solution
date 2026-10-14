/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef _ROBOTIC_ARM_UI_H
#define _ROBOTIC_ARM_UI_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"
#include "ui_events.h"
#include "screens/ui_RobotGraspScreen.h"

void ui_panthera_grasp_init(void);
void ui_panthera_grasp_destroy(void);

#ifdef __cplusplus
}
#endif

#endif
