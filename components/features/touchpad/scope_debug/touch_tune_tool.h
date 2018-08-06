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
#pragma once

#ifdef CONFIG_DATA_SCOPE_DEBUG

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__IBMC__) || defined(__SUNPRO_C) || defined(__SUNPRO_CC)
#pragma pack(1)
#else
#pragma pack(push, 1)
#endif

typedef enum tune_frame {
    TUNE_CTRL_DISCOVER  = 0,    /* No payload. */
    TUNE_CTRL_INQUIRY   = 1,    /* Payload is inquiry. */
    TUNE_CTRL_CANCEL    = 2,    /* No payload. */
    TUNE_DEV_INFO       = 3,    /* Payload is dev_info. */
    TUNE_DEV_SETTING    = 4,    /* Payload is dev_setting. */
    TUNE_DEV_PARAMETER  = 5,    /* Payload is dev_para. */
    TUNE_DEV_DATA       = 6,    /* Payload is dev_data. */
    TUNE_FRAME_MAX,
} tune_frame_type_t;

typedef enum tune_dev_comb {
    TUNE_CHAR_BUTTON = 'B',
    TUNE_CHAR_MATRIX = 'M',
    TUME_CHAR_SLIDE  = 'S',
    TUNE_CHAR_MAX,
} tune_dev_char_t;

typedef enum tune_dev_cid {
    TUNE_CID_ESP32      = 0,
    TUNE_CID_MAX,
} tune_dev_cid_t;

typedef enum tune_dev_ver {
    TUNE_VERSION_V0     = 0,
    TUNE_VERSION_V1     = 1,
    TUNE_VERSION_MAX,
} tune_dev_ver_t;               /* Used to check if the SW and Tool versions match. */

typedef struct {
    tune_dev_char_t dev_comb;   /* Device type, button/matrix/slider */
    uint8_t ch_num_h;           /* If device type is button/slider, "ch_num_h" = 1;
                                   If device type is matrix, "ch_num_h" is row channel number. */
    uint8_t ch_num_l;           /* If device type is button/slider, "ch_num_l" is channel number;
                                   If device type is matrix, "ch_num_l" is column channel number. */
    uint8_t ch[25];             /* The sequential GPIO corresponding to the touch sensor.  */
} tune_dev_comb_t;

typedef struct tune_ctrl_inquiry {
    uint8_t inq_type;           /* TUNE_DEV_INFO, TUNE_DEV_SETTING, TUNE_DEV_PARAMETER, TUNE_DEV_DATA */
    uint8_t mac[6];             /* Device station MAC */
} tune_ctrl_inquiry_t;

typedef struct {
    tune_dev_cid_t dev_cid;     /* Refer to "tune_dev_cid_t". */
    tune_dev_ver_t dev_ver;     /* Refer to "tune_dev_ver_t" */
    uint8_t dev_mac[6];         /* Device station MAC */
} tune_dev_info_t;

typedef struct {
    uint32_t ch_bits;           /* BIT0 represent TOUCH0. */
    tune_dev_comb_t dev_comb[10]; /* Refer to "tune_dev_comb_t" */
} tune_dev_setting_t;           /* User should set this frame. */

typedef struct {
    uint16_t filter_period;     /* TOUCHPAD_FILTER_IDLE_PERIOD */
    uint8_t debounce_ms;        /* TOUCHPAD_STATE_SWITCH_DEBOUNCE */
    uint8_t base_reset_cnt;     /* TOUCHPAD_BASELINE_RESET_COUNT_THRESHOLD */
    uint16_t base_update_cnt;   /* TOUCHPAD_BASELINE_UPDATE_COUNT_THRESHOLD */
    uint8_t touch_th;           /* TOUCHPAD_TOUCH_THRESHOLD_PERCENT */
    uint8_t noise_th;           /* TOUCHPAD_NOISE_THRESHOLD_PERCENT */
    uint8_t hys_th;             /* TOUCHPAD_HYSTERESIS_THRESHOLD_PERCENT */
    uint8_t base_reset_th;      /* TOUCHPAD_BASELINE_RESET_THRESHOLD_PERCENT */
    uint8_t base_slider_th;     /* TOUCHPAD_SLIDER_TRIGGER_THRESHOLD_PERCENT */
} tune_dev_parameter_t;

typedef struct {
    uint8_t ch;                 /* Channel number. */
    uint16_t raw;               /* Touch raw data. */
    uint16_t baseline;          /* Touch baseline. */
    uint16_t diff;              /* Diff data = Raw - baseline. */
    uint16_t status;            /* If the button is slide, the status is the position of slide.
                                   If the button is button, the status is the state of button. */
} tune_dev_data_t;

typedef struct {
    uint8_t frame_type;         /* Refer to "tune_frame_type_t". */
    uint8_t payload[100];       /* Different "frame_type" have different data formats.
                                    union {
                                        tune_ctrl_inquiry_t inquiry;     // frame_type = TUNE_CTRL_INQUIRY.
                                        tune_dev_info_t dev_info;        // frame_type = TUNE_DEV_INFO.
                                        tune_dev_setting_t dev_setting;  // frame_type = TUNE_DEV_SETTING.
                                        tune_dev_parameter_t dev_para;   // frame_type = TUNE_DEV_PARAMETER.
                                        tune_dev_data_t dev_data;        // frame_type = TUNE_DEV_DATA.
                                    } payload; */
} tune_frame_t;

typedef struct {
    uint16_t head;              /* 0x55, 0xAA */
    uint16_t length;            /* 2Byte, High byte in the former. */
    tune_frame_t frame;         /* Frame info */
    uint8_t check_sum;          /* check sum byte */
} tune_frame_all_t;

typedef union {
    uint8_t byte[4];
    uint32_t data;
} uint32_convert;

typedef union {
    uint8_t byte[2];
    uint16_t data;
} uint16_convert;

typedef struct {
    uint8_t frame_type;
    uint32_t time;
} touch_tool_evt_t;

#if defined(__IBMC__) || defined(__SUNPRO_C) || defined(__SUNPRO_CC)
#pragma pack()
#else
#pragma pack(pop)
#endif

/**
 * @brief Set device parameter. this function be called in touchpad.c.
 *
 * @param dev_para a pointer of tune_dev_parameter_t.
 *
 * @return Always return ESP_OK.
 */
esp_err_t tune_tool_set_device_parameter(tune_dev_parameter_t *dev_para);

/**
 * @brief Set device data. this function be called in touchpad.c.
 *
 * @param dev_data a pointer of tune_dev_data_t.
 *
 * @return Always return ESP_OK.
 */
esp_err_t tune_tool_set_device_data(tune_dev_data_t *dev_data);

/**
 * @brief Set device info. this function be called in touchpad.c.
 *
 * @param dev_info a pointer of tune_dev_info_t.
 *
 * @return Always return ESP_OK.
 */
esp_err_t tune_tool_set_device_info(tune_dev_info_t *dev_info);

/**
 * @brief Add device set. This function should be called by user.
 *
 * @param ch_comb a pointer of tune_dev_comb_t.
 *
 * @return Always return ESP_OK.
 */
esp_err_t tune_tool_add_device_setting(tune_dev_comb_t *ch_comb);

/**
 * @brief This function is responsible for uart initialization and create read and write(send) task.
 * @note  This function should be called by user.
 */
void touch_tune_tool_init();

#ifdef __cplusplus
}
#endif

#endif
