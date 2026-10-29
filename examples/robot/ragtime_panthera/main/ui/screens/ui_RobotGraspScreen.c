/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "../ui.h"

/* 1024x600 layout (absolute, because camera stays native 640x480):
 *
 *  [ title bar ................................ status ]
 *  [ camera 640x480 ][ side rail: power/modes/actions ]
 *  [ motor status chips J1..J6 + Grip          ]
 */

#define UI_COLOR_BG          0x12151A
#define UI_COLOR_PANEL       0x1C2129
#define UI_COLOR_PANEL_ALT   0x252B34
#define UI_COLOR_BORDER      0x343B46
#define UI_COLOR_TEXT        0xE8ECF1
#define UI_COLOR_MUTED       0x8B949E
#define UI_COLOR_ACCENT      0x3D9CF0
#define UI_COLOR_ACCENT_DIM  0x1F4F7A
#define UI_COLOR_OK          0x3FB950
#define UI_COLOR_WARN        0xD29922

#define UI_SCREEN_W          1024
#define UI_SCREEN_H          600
#define UI_PAD               10
#define UI_TOP_H             44
#define UI_CAMERA_W          640
#define UI_CAMERA_H          480
#define UI_SIDE_X            (UI_PAD + UI_CAMERA_W + UI_PAD)
#define UI_SIDE_W            (UI_SCREEN_W - UI_SIDE_X - UI_PAD)
#define UI_SIDE_Y            (UI_PAD + UI_TOP_H + 6)
#define UI_SIDE_H            (UI_SCREEN_H - UI_SIDE_Y - UI_PAD)

lv_obj_t *ui_RobotGraspScreen = NULL;
lv_obj_t *ui_Container2 = NULL;
lv_obj_t *ui_uvcframe = NULL;
lv_obj_t *ui_GraspButton = NULL;
lv_obj_t *ui_GraspLabel = NULL;
lv_obj_t *ui_EnableLabel = NULL;
lv_obj_t *ui_EnableSwitch = NULL;
lv_obj_t *ui_GoZeroButton = NULL;
lv_obj_t *ui_GoZeroLabel = NULL;
lv_obj_t *ui_ZeroGSwitch = NULL;
lv_obj_t *ui_ZeroGLabel = NULL;
lv_obj_t *ui_StatusLabel = NULL;
lv_obj_t *ui_TitleLabel = NULL;
lv_obj_t *ui_MotorStatusPanel = NULL;
lv_obj_t *ui_MotorStatusChips[UI_MOTOR_STATUS_COUNT] = {NULL};
lv_obj_t *ui_MotorStatusLabels[UI_MOTOR_STATUS_COUNT] = {NULL};

static const char *k_motor_names[UI_MOTOR_STATUS_COUNT] = {
    "J1", "J2", "J3", "J4", "J5", "J6", "Grip"
};

static void style_panel(lv_obj_t *obj)
{
    lv_obj_set_style_bg_color(obj, lv_color_hex(UI_COLOR_PANEL), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(obj, 1, 0);
    lv_obj_set_style_border_color(obj, lv_color_hex(UI_COLOR_BORDER), 0);
    lv_obj_set_style_radius(obj, 10, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
}

static lv_obj_t *create_section_title(lv_obj_t *parent, const char *text)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, lv_color_hex(UI_COLOR_MUTED), 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
    lv_obj_set_width(label, LV_PCT(100));
    return label;
}

static void create_toggle_row(lv_obj_t *parent, const char *text, lv_obj_t **out_label, lv_obj_t **out_switch)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_height(row, 48);
    lv_obj_set_style_bg_color(row, lv_color_hex(UI_COLOR_PANEL_ALT), 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_border_color(row, lv_color_hex(UI_COLOR_BORDER), 0);
    lv_obj_set_style_radius(row, 8, 0);
    lv_obj_set_style_pad_hor(row, 12, 0);
    lv_obj_set_style_pad_ver(row, 6, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    *out_label = lv_label_create(row);
    lv_label_set_text(*out_label, text);
    lv_obj_set_style_text_color(*out_label, lv_color_hex(UI_COLOR_TEXT), 0);
    lv_obj_set_style_text_font(*out_label, &lv_font_montserrat_16, 0);

    *out_switch = lv_switch_create(row);
    lv_obj_set_size(*out_switch, 58, 32);
    lv_obj_set_style_bg_color(*out_switch, lv_color_hex(0x4A5563), LV_PART_MAIN);
    lv_obj_set_style_bg_color(*out_switch, lv_color_hex(UI_COLOR_ACCENT), LV_PART_INDICATOR | LV_STATE_CHECKED);
}

static void create_action_button(lv_obj_t *parent, const char *text, lv_obj_t **out_btn, lv_obj_t **out_label, bool primary)
{
    *out_btn = lv_button_create(parent);
    lv_obj_set_width(*out_btn, LV_PCT(100));
    lv_obj_set_height(*out_btn, 50);
    lv_obj_set_style_radius(*out_btn, 8, 0);
    lv_obj_set_style_shadow_width(*out_btn, 0, 0);
    if (primary) {
        lv_obj_set_style_bg_color(*out_btn, lv_color_hex(UI_COLOR_ACCENT), 0);
        lv_obj_set_style_bg_color(*out_btn, lv_color_hex(UI_COLOR_ACCENT_DIM), LV_STATE_PRESSED);
    } else {
        lv_obj_set_style_bg_color(*out_btn, lv_color_hex(UI_COLOR_PANEL_ALT), 0);
        lv_obj_set_style_border_width(*out_btn, 1, 0);
        lv_obj_set_style_border_color(*out_btn, lv_color_hex(UI_COLOR_BORDER), 0);
        lv_obj_set_style_bg_color(*out_btn, lv_color_hex(UI_COLOR_BORDER), LV_STATE_PRESSED);
    }
    lv_obj_set_style_opa(*out_btn, LV_OPA_40, LV_STATE_DISABLED);

    *out_label = lv_label_create(*out_btn);
    lv_label_set_text(*out_label, text);
    lv_obj_set_style_text_color(*out_label, lv_color_hex(UI_COLOR_TEXT), 0);
    lv_obj_set_style_text_font(*out_label, &lv_font_montserrat_18, 0);
    lv_obj_center(*out_label);
}

void ui_RobotGraspScreen_screen_init(void)
{
    ui_RobotGraspScreen = lv_obj_create(NULL);
    lv_obj_remove_flag(ui_RobotGraspScreen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(ui_RobotGraspScreen, lv_color_hex(UI_COLOR_BG), 0);
    lv_obj_set_style_bg_opa(ui_RobotGraspScreen, LV_OPA_COVER, 0);

    /* Top bar */
    lv_obj_t *top_bar = lv_obj_create(ui_RobotGraspScreen);
    lv_obj_remove_flag(top_bar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(top_bar, UI_PAD, UI_PAD);
    lv_obj_set_size(top_bar, UI_SCREEN_W - 2 * UI_PAD, UI_TOP_H);
    style_panel(top_bar);
    lv_obj_set_style_pad_hor(top_bar, 16, 0);
    lv_obj_set_flex_flow(top_bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(top_bar, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    ui_TitleLabel = lv_label_create(top_bar);
    lv_label_set_text(ui_TitleLabel, "Ragtime Panthera");
    lv_obj_set_style_text_color(ui_TitleLabel, lv_color_hex(UI_COLOR_TEXT), 0);
    lv_obj_set_style_text_font(ui_TitleLabel, &lv_font_montserrat_20, 0);

    ui_StatusLabel = lv_label_create(top_bar);
    lv_label_set_text(ui_StatusLabel, "Status: Standby");
    lv_obj_set_style_text_color(ui_StatusLabel, lv_color_hex(UI_COLOR_OK), 0);
    lv_obj_set_style_text_font(ui_StatusLabel, &lv_font_montserrat_16, 0);

    /* Camera panel */
    const int camera_y = UI_PAD + UI_TOP_H + 6;
    lv_obj_t *camera_panel = lv_obj_create(ui_RobotGraspScreen);
    lv_obj_remove_flag(camera_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(camera_panel, UI_PAD, camera_y);
    lv_obj_set_size(camera_panel, UI_CAMERA_W, UI_CAMERA_H);
    style_panel(camera_panel);
    lv_obj_set_style_pad_all(camera_panel, 0, 0);
    lv_obj_set_style_bg_color(camera_panel, lv_color_hex(0x000000), 0);

    ui_Container2 = lv_obj_create(camera_panel);
    lv_obj_remove_style_all(ui_Container2);
    lv_obj_set_size(ui_Container2, UI_CAMERA_W, UI_CAMERA_H);
    lv_obj_set_align(ui_Container2, LV_ALIGN_CENTER);
    lv_obj_remove_flag(ui_Container2, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(ui_Container2, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(ui_Container2, LV_OPA_COVER, 0);

    ui_uvcframe = lv_image_create(ui_Container2);
    lv_obj_set_size(ui_uvcframe, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_align(ui_uvcframe, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_uvcframe, LV_OBJ_FLAG_ADV_HITTEST);
    lv_obj_remove_flag(ui_uvcframe, LV_OBJ_FLAG_SCROLLABLE);

    /* Motor status strip under the camera */
    const int status_y = camera_y + UI_CAMERA_H + 4;
    ui_MotorStatusPanel = lv_obj_create(ui_RobotGraspScreen);
    lv_obj_remove_flag(ui_MotorStatusPanel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(ui_MotorStatusPanel, UI_PAD, status_y);
    lv_obj_set_size(ui_MotorStatusPanel, UI_CAMERA_W, UI_SCREEN_H - status_y - UI_PAD);
    style_panel(ui_MotorStatusPanel);
    lv_obj_set_style_pad_all(ui_MotorStatusPanel, 4, 0);
    lv_obj_set_style_pad_column(ui_MotorStatusPanel, 4, 0);
    lv_obj_set_flex_flow(ui_MotorStatusPanel, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(ui_MotorStatusPanel, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    for (int i = 0; i < UI_MOTOR_STATUS_COUNT; i++) {
        ui_MotorStatusChips[i] = lv_obj_create(ui_MotorStatusPanel);
        lv_obj_remove_flag(ui_MotorStatusChips[i], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(ui_MotorStatusChips[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_flex_grow(ui_MotorStatusChips[i], 1);
        lv_obj_set_height(ui_MotorStatusChips[i], LV_PCT(100));
        lv_obj_set_style_bg_color(ui_MotorStatusChips[i], lv_color_hex(UI_COLOR_PANEL_ALT), 0);
        lv_obj_set_style_bg_opa(ui_MotorStatusChips[i], LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(ui_MotorStatusChips[i], lv_color_hex(UI_COLOR_BORDER), LV_STATE_PRESSED);
        lv_obj_set_style_border_width(ui_MotorStatusChips[i], 1, 0);
        lv_obj_set_style_border_color(ui_MotorStatusChips[i], lv_color_hex(UI_COLOR_BORDER), 0);
        lv_obj_set_style_border_color(ui_MotorStatusChips[i], lv_color_hex(UI_COLOR_ACCENT), LV_STATE_PRESSED);
        lv_obj_set_style_radius(ui_MotorStatusChips[i], 6, 0);
        lv_obj_set_style_pad_all(ui_MotorStatusChips[i], 0, 0);
        lv_obj_set_flex_flow(ui_MotorStatusChips[i], LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(ui_MotorStatusChips[i], LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        ui_MotorStatusLabels[i] = lv_label_create(ui_MotorStatusChips[i]);
        lv_label_set_text_fmt(ui_MotorStatusLabels[i], "%s\n--", k_motor_names[i]);
        lv_obj_set_style_text_color(ui_MotorStatusLabels[i], lv_color_hex(UI_COLOR_MUTED), 0);
        lv_obj_set_style_text_font(ui_MotorStatusLabels[i], &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_align(ui_MotorStatusLabels[i], LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_add_flag(ui_MotorStatusLabels[i], LV_OBJ_FLAG_EVENT_BUBBLE);
        lv_obj_remove_flag(ui_MotorStatusLabels[i], LV_OBJ_FLAG_CLICKABLE);
    }

    /* Right control rail */
    lv_obj_t *side = lv_obj_create(ui_RobotGraspScreen);
    lv_obj_remove_flag(side, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(side, UI_SIDE_X, UI_SIDE_Y);
    lv_obj_set_size(side, UI_SIDE_W, UI_SIDE_H);
    style_panel(side);
    lv_obj_set_style_pad_all(side, 12, 0);
    lv_obj_set_style_pad_row(side, 8, 0);
    lv_obj_set_flex_flow(side, LV_FLEX_FLOW_COLUMN);

    create_section_title(side, "POWER");
    create_toggle_row(side, "Enable", &ui_EnableLabel, &ui_EnableSwitch);

    create_section_title(side, "MODES");
    create_toggle_row(side, "Zero Gravity", &ui_ZeroGLabel, &ui_ZeroGSwitch);
    lv_obj_add_state(ui_ZeroGSwitch, LV_STATE_DISABLED);
    lv_obj_set_style_text_color(ui_ZeroGLabel, lv_color_hex(UI_COLOR_MUTED), 0);

    create_section_title(side, "ACTIONS");
    create_action_button(side, "Grasp", &ui_GraspButton, &ui_GraspLabel, true);
    create_action_button(side, "Go Zero", &ui_GoZeroButton, &ui_GoZeroLabel, false);
}

void ui_RobotGraspScreen_screen_destroy(void)
{
    if (ui_RobotGraspScreen) {
        lv_obj_del(ui_RobotGraspScreen);
    }

    ui_RobotGraspScreen = NULL;
    ui_Container2 = NULL;
    ui_uvcframe = NULL;
    ui_GraspButton = NULL;
    ui_GraspLabel = NULL;
    ui_EnableLabel = NULL;
    ui_EnableSwitch = NULL;
    ui_GoZeroButton = NULL;
    ui_GoZeroLabel = NULL;
    ui_ZeroGSwitch = NULL;
    ui_ZeroGLabel = NULL;
    ui_StatusLabel = NULL;
    ui_TitleLabel = NULL;
    ui_MotorStatusPanel = NULL;
    for (int i = 0; i < UI_MOTOR_STATUS_COUNT; i++) {
        ui_MotorStatusChips[i] = NULL;
        ui_MotorStatusLabels[i] = NULL;
    }
}
