/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include "freertos/timers.h"

#include <hal/gpio_hal.h>
#include <esp_log.h>

#ifdef CONFIG_ENABLE_PWM_DRIVER
#include "pwm.h"
#endif

#ifdef CONFIG_ENABLE_SM2135E_DRIVER
#include "sm2135e.h"
#endif

#ifdef CONFIG_ENABLE_SM2135EH_DRIVER
#include "sm2135eh.h"
#endif

#ifdef CONFIG_ENABLE_BP5758D_DRIVER
#include "bp5758d.h"
#endif

#ifdef CONFIG_ENABLE_BP1658CJ_DRIVER
#include "bp1658cj.h"
#endif

#ifdef CONFIG_ENABLE_SM2x35EGH_DRIVER
#include "sm2x35egh.h"
#endif

#ifdef CONFIG_ENABLE_WS2812_DRIVER
#include "ws2812.h"
#endif

#ifdef CONFIG_ENABLE_KP18058_DRIVER
#include "kp18058.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Supported drivers
 *
 */
typedef enum {
    DRIVER_SELECT_INVALID = 0,
    /* PWM */
    DRIVER_ESP_PWM = 1,

    /* IIC */
    DRIVER_SM2135E,
    DRIVER_SM2135EH,
    DRIVER_SM2235EGH,
    DRIVER_SM2335EGH,
    DRIVER_BP5758D, // Available for BP5758 BP5758D BP5768D
    DRIVER_BP1658CJ,
    DRIVER_KP18058,

    /* Single Bus */
    DRIVER_WS2812,

    DRIVER_SELECT_MAX,
} lightbulb_driver_t;

/**
 * @brief Supported Control Modes
 * @note
 */
typedef enum {
    COLOR_MODE = BIT(1),
    WHITE_MODE = BIT(2),
    COLOR_AND_WHITE_MODE = COLOR_MODE | WHITE_MODE,
} lightbulb_mode_t;

/**
 * @brief Supported effects
 *
 */
typedef enum {
    EFFECT_BREATH = 0,
    EFFECT_BLINK = 1,
} lightbulb_effect_t;

/**
 * @brief Lighting test unit
 *
 */
typedef enum {
    LIGHTING_RAINBOW = BIT(0),
    LIGHTING_WARM_TO_COLD = BIT(1),
    LIGHTING_COLD_TO_WARM = BIT(2),
    LIGHTING_BASIC_FIVE = BIT(3),
    LIGHTING_COLOR_MUTUAL_WHITE = BIT(4),
    LIGHTING_COLOR_EFFECT = BIT(5),
    LIGHTING_WHITE_EFFECT = BIT(6),
    LIGHTING_ALEXA = BIT(7),
    LIGHTING_COLOR_VALUE_INCREMENT = BIT(8),
    LIGHTING_WHITE_BRIGHTNESS_INCREMENT = BIT(9),
    LIGHTING_ALL_UNIT = UINT32_MAX,
} lightbulb_lighting_unit_t;

/**
 * @brief Balance coefficient
 * @note This coefficient will be applied to the last processed data.
 *       Usually used for trimming current or for color calibration.
 *       Set the range is 0.5-1.0, if not set the default is 1.0.
 *
 */
typedef struct {
    float r_balance_coe;
    float g_balance_coe;
    float b_balance_coe;
} lightbulb_custom_balance_coefficient_t;

/**
 * @brief Use an external custom gamma table.
 * @note You need to generate an array of length 256,
 *       the values in the array correspond to different grayscale levels.
 */
typedef struct {
    uint16_t *custom_table[3]; // Please pass in 3 pointers, which represent the custom grayscale of the R G B channel
    int table_size; // Currently only supported length is 256
} lightbulb_custom_table_t;

/**
 * @brief These configurations are used for color calibration.
 * @note If you customize the gamma table, it will not be generated using the x_curve_coe variable
 *       The coefficient variable will generate a grayscale table using a specific formula.Please refer to the implementation of `gamma_table_create()`.
 *
 * @note The curve coefficient data range is 0.8-2.2, if not set, the default is 1.0.
 *       If the curve coefficient is 1.0, the resulting data is linear.
 */
typedef struct {
    lightbulb_custom_table_t *table;
    lightbulb_custom_balance_coefficient_t *balance;
    float r_curve_coe;
    float g_curve_coe;
    float b_curve_coe;
} lightbulb_gamma_data_t;

/**
 * @brief The working mode of the lightbulb.
 *
 */
typedef enum {
    WORK_NONE = 0,
    WORK_COLOR = 1,
    WORK_WHITE = 2,
    /* NOT SUPPORT */
    // WORK_COLOR_AND_WHITE = 3,
} lightbulb_works_mode_t;

/**
 * @brief The working status of the lightbulb.
 * @attention Both the variable `value` and the variable `brightness` are used to mark light brightness.
 *            They respectively indicate the brightness of color light and white light.
 *
 */
typedef struct {
    lightbulb_works_mode_t mode;
    bool on; // on/off status of the lightbulb.

    /* Use HSV model to display color light */
    uint16_t hue;       // range: 0-360
    uint8_t saturation; // range: 0-100
    uint8_t value;      // range: 0-100

    /* Use CCT (color temperatures) display white light */
    /**
     * 0%    ->  .. -> 100%
     * 2200K ->  .. -> 7000K
     * warm  ->  .. -> cold
     *
     */
    uint8_t cct_percentage;     // range: 0-100
    uint8_t brightness;         // range: 0-100
} lightbulb_status_t;

/**
 * @brief Output limit or gain without changing color
 *
 */
typedef struct {
    /* Scale the incoming value
     * range: 10% <= value <= 100%
     * step: 1%
     * min default: 10%
     * max default: 100%
     */
    uint8_t white_max_brightness; //Limit the maximum brightness of white light output
    uint8_t white_min_brightness; //Limit the minimum brightness of white light output
    uint8_t color_max_value;
    uint8_t color_min_value;

    /* Dynamically adjust the final power
     * range: 100% <= value <= 300%
     * step: 10%
     * max default: 100%
     */
    uint16_t white_max_power;
    uint16_t color_max_power;
} lightbulb_power_limit_t;

/**
 * @brief Used to map percentages to kelvin
 *
 */
typedef struct {
    uint16_t min; // Minimum color temperature value, default is 2200 k
    uint16_t max; // Maximum color temperature value, default is 7000 k
} lightbulb_cct_kelvin_range_t;

/**
 * @brief Function pointers to store lightbulb status
 *
 */
typedef esp_err_t (*lightbulb_status_storage_cb_t)(lightbulb_status_t);

/**
 * @brief Function pointers to monitor the workings of the underlying hardware
 *
 */
typedef bool (*hardware_monitor_user_cb_t)(void);

/**
 * @brief Some Lightbulb Capability Configuration Options
 *
 */
typedef struct {
    uint16_t fades_ms;                        // Fade time, data range: 100ms - 3000ms, default is 800ms.
    uint16_t storage_delay_ms;                // The storage delay time is used to solve the adverse effects of short-term repeated erasing and writing of nvs.
    lightbulb_mode_t mode_mask;               // Select the lightbulb light mode, the driver will initialize the corresponding port according to this option.
    lightbulb_status_storage_cb_t storage_cb; // This callback function will be called when the lightbulb status starts to be stored.
    hardware_monitor_user_cb_t monitor_cb;    // This callback function will be called when there is a problem with the underlying hardware,
    // and if it returns true then subsequent communication will be aborted.
    bool enable_fades : 1;                    // Color switching uses fade effects instead of direct rapid changes.
    bool enable_lowpower : 1;                 // Low-power regulation with lights off.
    bool enable_status_storage : 1;           // Store lightbulb state to nvs.
    bool enable_mix_cct : 1;                  // CCT output uses two-channel mixing instead of single-channel control.
    bool sync_change_brightness_value : 1;    // Enable this option if you need to use a parameter to mark the brightness of the white and color output
    bool disable_auto_on : 1;                 // Enable this option if you don't need automatic on when color/white value is set
} lightbulb_capability_t;

/**
 * @brief Port enumeration names for IIC chips
 *
 */
typedef enum {
    OUT1 = 0,
    OUT2,
    OUT3,
    OUT4,
    OUT5,
    OUT_MAX,
} iic_out_pin_t;

/**
 * @brief Lightbulb Configuration Options
 * @attention If the `gamma_conf` `external_limit` variable is not set, will use the default value.
 */
typedef struct {
    lightbulb_driver_t type;

    union {
#ifdef CONFIG_ENABLE_PWM_DRIVER
        driver_pwm_t pwm;
#endif
#ifdef CONFIG_ENABLE_SM2135E_DRIVER
        driver_sm2135e_t sm2135e;
#endif
#ifdef CONFIG_ENABLE_SM2135EH_DRIVER
        driver_sm2135eh_t sm2135eh;
#endif
#ifdef CONFIG_ENABLE_BP5758D_DRIVER
        driver_bp5758d_t bp5758d;
#endif
#ifdef CONFIG_ENABLE_BP1658CJ_DRIVER
        driver_bp1658cj_t bp1658cj;
#endif
#ifdef CONFIG_ENABLE_SM2x35EGH_DRIVER
        driver_sm2x35egh_t sm2235egh;
        driver_sm2x35egh_t sm2335egh;
#endif
#ifdef CONFIG_ENABLE_KP18058_DRIVER
        driver_kp18058_t kp18058;
#endif
#ifdef CONFIG_ENABLE_WS2812_DRIVER
        driver_ws2812_t ws2812;
#endif
    } driver_conf;

    lightbulb_cct_kelvin_range_t *kelvin_range;
    lightbulb_gamma_data_t *gamma_conf;
    lightbulb_power_limit_t *external_limit;
    union {
        struct {
            gpio_num_t red;
            gpio_num_t green;
            gpio_num_t blue;
            gpio_num_t cold_cct;
            gpio_num_t warm_brightness;
        } pwm_io;
        struct {
            iic_out_pin_t red;
            iic_out_pin_t green;
            iic_out_pin_t blue;
            iic_out_pin_t cold_white;
            iic_out_pin_t warm_yellow;
        } iic_io;
    } io_conf;

    lightbulb_capability_t capability;
    lightbulb_status_t init_status;
} lightbulb_config_t;

/**
 * @brief Effect function configuration options
 *
 */
typedef struct {
    lightbulb_effect_t effect_type;
    lightbulb_works_mode_t mode;
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    uint16_t cct;
    uint8_t min_brightness;
    uint8_t max_brightness;
    uint16_t effect_cycle_ms;

    /*
     * If total_ms > 0 will enable auto-stop timer.
     * When the timer triggers will automatically call the stop function and user callback function (if provided).
     */
    int total_ms;
    void(*user_cb)(void);

    /*
     * If set to true, the auto-stop timer can only be stopped by effect_stop/effect_start interfaces or triggered by FreeRTOS.
     * Any set APIs will only save the status, the status will not be written.
     */
    bool interrupt_forbidden;
} lightbulb_effect_config_t;

/**
 * @brief Initialize the lightbulb
 *
 * @param config Configuration parameters
 * @return esp_err_t
 */
esp_err_t lightbulb_init(lightbulb_config_t *config);

/**
 * @brief Deinitialize the lightbulb and release resources
 *
 * @return esp_err_t
 */
esp_err_t lightbulb_deinit(void);

/**
 * @brief Set lightbulb fade time
 *
 * @param fades_ms range: 100ms - 3000ms
 * @return esp_err_t
 */
esp_err_t lightbulb_set_fade_time(uint32_t fades_ms);

/**
 * @brief Enable/Disable the lightbulb fade function
 *
 * @param is_enable Enable/Disable
 * @return esp_err_t
 */
esp_err_t lightbulb_set_fades_function(bool is_enable);

/**
 * @brief Enable/Disable the lightbulb storage function
 *
 * @param is_enable Enable/Disable
 * @return esp_err_t
 */
esp_err_t lightbulb_set_storage_function(bool is_enable);

/**
 * @brief Re-update the lightbulb status variable
 *
 * @param new_status new status
 * @param trigger If set to true, then it will be updated immediately
 * @return esp_err_t
 */
esp_err_t lightbulb_update_status_variable(lightbulb_status_t *new_status, bool trigger);

/**
 * @brief Get lightbulb fade function enabled status
 *
 * @return true Enabled
 * @return false Disabled
 */
bool lightbulb_get_fades_function_status(void);

/**
 * @brief Convert HSV model to RGB model
 * @note RGB model color depth is 8 bit
 *
 * @param hue range: 0-360
 * @param saturation range: 0-100
 * @param value range: 0-100
 * @param red range: 0-255
 * @param green range: 0-255
 * @param blue range: 0-255
 * @return esp_err_t
 */
esp_err_t lightbulb_hsv2rgb(uint16_t hue, uint8_t saturation, uint8_t value, uint8_t *red, uint8_t *green, uint8_t *blue);

/**
 * @brief Convert RGB model to HSV model
 * @note RGB model color depth is 8 bit
 *
 * @param red range: 0-255
 * @param green range: 0-255
 * @param blue range: 0-255
 * @param hue range: 0-360
 * @param saturation range: 0-100
 * @param value range: 0-100
 * @return esp_err_t
 */
esp_err_t lightbulb_rgb2hsv(uint16_t red, uint16_t green, uint16_t blue, uint16_t *hue, uint8_t *saturation, uint8_t *value);

/**
 * @brief Convert xyY model to RGB model
 * @note Refer: https://www.easyrgb.com/en/convert.php#inputFORM
 *              https://www.easyrgb.com/en/math.php
 *
 * @param x range: 0-1.0
 * @param y range: 0-1.0
 * @param Y range: 0-100.0
 * @param red range: 0-255
 * @param green range: 0-255
 * @param blue range: 0-255
 * @return esp_err_t
 */
esp_err_t lightbulb_xyy2rgb(float x, float y, float Y, uint8_t *red, uint8_t *green, uint8_t *blue);

/**
 * @brief Convert RGB model to xyY model
 *
 * @param red range: 0-255
 * @param green range: 0-255
 * @param blue range: 0-255
 * @param x range: 0-1.0
 * @param y range: 0-1.0
 * @param Y range: 0-100.0
 * @return esp_err_t
 */
esp_err_t lightbulb_rgb2xyy(uint8_t red, uint8_t green, uint8_t blue, float *x, float *y, float *Y);

/**
 * @brief Convert CCT kelvin to percentage
 *
 * @param kelvin default range: 2200k - 7000k
 * @param percentage range: 0 - 100
 * @return esp_err_t
 */
esp_err_t lightbulb_kelvin2percentage(uint16_t kelvin, uint8_t *percentage);

/**
 * @brief Convert percentage to kelvin
 *
 * @param percentage range: 0 - 100
 * @param kelvin default range: 2200k - 7000k
 * @return esp_err_t
 */
esp_err_t lightbulb_percentage2kelvin(uint8_t percentage, uint16_t *kelvin);

/**
 * @brief Set hue
 *
 * @param hue range: 0-360
 * @return esp_err_t
 */
esp_err_t lightbulb_set_hue(uint16_t hue);

/**
 * @brief Set saturation
 *
 * @param saturation range: 0-100
 * @return esp_err_t
 */
esp_err_t lightbulb_set_saturation(uint8_t saturation);

/**
 * @brief Set value
 *
 * @param value range: 0-100
 * @return esp_err_t
 */
esp_err_t lightbulb_set_value(uint8_t value);

/**
 * @brief Set color temperature (CCT)
 * @note Supports use percentage or Kelvin values
 * @param cct range: 0-100 or 2200-7000
 * @return esp_err_t
 */
esp_err_t lightbulb_set_cct(uint16_t cct);

/**
 * @brief Set brightness
 *
 * @param brightness
 * @return esp_err_t
 */
esp_err_t lightbulb_set_brightness(uint8_t brightness);

/**
 * @brief Set xyY
 * @attention The xyY color model cannot fully correspond to the HSV color model, so the color may be biased.
 *            The grayscale will be recalculated in lightbulb, so we cannot directly operate the underlying driver through the xyY interface.
 *
 * @param x range: 0-1.0
 * @param y range: 0-1.0
 * @param Y range: 0-100.0
 * @return esp_err_t
 */
esp_err_t lightbulb_set_xyy(float x, float y, float Y);

/**
 * @brief Set hsv
 *
 * @param hue range: 0-360
 * @param saturation range: 0-100
 * @param value range: 0-100
 * @return esp_err_t
 */
esp_err_t lightbulb_set_hsv(uint16_t hue, uint8_t saturation, uint8_t value);

/**
 * @brief Set cct and brightness
 * @note Supports use percentage or Kelvin
 * @param cct range: 0-100 or 2200-7000k
 * @param brightness range: 0-100
 * @return esp_err_t
 */
esp_err_t lightbulb_set_cctb(uint16_t cct, uint8_t brightness);

/**
 * @brief Set on/off
 *
 * @param status on/off status
 * @return esp_err_t
 */
esp_err_t lightbulb_set_switch(bool status);

/**
 * @brief Get hue
 *
 * @return int16_t
 */
int16_t lightbulb_get_hue(void);

/**
 * @brief Get saturation
 *
 * @return int8_t
 */
int8_t lightbulb_get_saturation(void);

/**
 * @brief Get value
 *
 * @return int8_t
 */
int8_t lightbulb_get_value(void);

/**
 * @brief Get CCT percentage
 *
 * @return int8_t
 */
int8_t lightbulb_get_cct_percentage(void);

/**
 * @brief Get CCT kelvin
 *
 * @return int16_t
 */
int16_t lightbulb_get_cct_kelvin(void);

/**
 * @brief Get brightness
 *
 * @return int8_t
 */
int8_t lightbulb_get_brightness(void);

/**
 * @brief Get on/off status
 *
 * @return true on
 * @return false off
 */
bool lightbulb_get_switch(void);

/**
 * @brief Get work mode
 *
 * @return lightbulb_works_mode_t
 */
lightbulb_works_mode_t lightbulb_get_mode(void);

/**
 * @brief Get all status
 *
 * @param status
 * @return esp_err_t
 */
esp_err_t lightbulb_get_all_detail(lightbulb_status_t *status);

/**
 * @brief Get lightbulb status from nvs
 *
 * @param value Stored state
 * @return esp_err_t
 */
esp_err_t lightbulb_status_get_from_nvs(lightbulb_status_t *value);

/**
 * @brief Store lightbulb state to nvs.
 *
 * @param value Current running state
 * @return esp_err_t
 */
esp_err_t lightbulb_status_set_to_nvs(const lightbulb_status_t *value);

/**
 * @brief Erase lightbulb state stored in nvs.
 *
 * @return esp_err_t
 */
esp_err_t lightbulb_status_erase_nvs_storage(void);

/**
 * @brief Start some blinking/breathing effects
 *
 * @param config
 * @return esp_err_t
 */
esp_err_t lightbulb_basic_effect_start(lightbulb_effect_config_t *config);

/**
 * @brief Stop the effect in progress and keep the current lighting output
 *
 * @return esp_err_t
 */
esp_err_t lightbulb_basic_effect_stop(void);

/**
 * @brief Stop the effect in progress and restore the previous lighting output
 *
 * @return esp_err_t
 */
esp_err_t lightbulb_basic_effect_stop_and_restore(void);

/**
 * @brief Used to test lightbulb hardware functionality
 *
 * @param mask test unit
 * @param speed_ms switching speed
 */
void lightbulb_lighting_output_test(lightbulb_lighting_unit_t mask, uint16_t speed_ms);

#ifdef __cplusplus
}
#endif
