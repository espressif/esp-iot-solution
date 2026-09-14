/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef UI_ROBOTGRASPSCREEN_H
#define UI_ROBOTGRASPSCREEN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

#define UI_MOTOR_STATUS_COUNT 7

void ui_RobotGraspScreen_screen_init(void);
void ui_RobotGraspScreen_screen_destroy(void);

extern lv_obj_t *ui_RobotGraspScreen;
extern lv_obj_t *ui_Container2;
extern lv_obj_t *ui_uvcframe;
extern lv_obj_t *ui_GraspButton;
extern lv_obj_t *ui_GraspLabel;
extern lv_obj_t *ui_SyncSwitch;
extern lv_obj_t *ui_EnableLabel;
extern lv_obj_t *ui_SyncLabel;
extern lv_obj_t *ui_EnableSwitch;
extern lv_obj_t *ui_GoZeroButton;
extern lv_obj_t *ui_GoZeroLabel;

/* MIT gravity compensation (Zero Gravity); enabled after motors are powered. */
extern lv_obj_t *ui_ZeroGSwitch;
extern lv_obj_t *ui_ZeroGLabel;
extern lv_obj_t *ui_StatusLabel;
extern lv_obj_t *ui_TitleLabel;

/* Motor online/fault chips under the camera (J1..J6 + Grip). */
extern lv_obj_t *ui_MotorStatusPanel;
extern lv_obj_t *ui_MotorStatusChips[UI_MOTOR_STATUS_COUNT];
extern lv_obj_t *ui_MotorStatusLabels[UI_MOTOR_STATUS_COUNT];

#ifdef __cplusplus
}
#endif

#endif
