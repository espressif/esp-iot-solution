/*
 * SPDX-FileCopyrightText: 2022-2024 Espressif Systems (Shanghai) CO LTD
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

#ifdef __cplusplus
extern "C" {
#endif

#ifdef CONFIG_ENABLE_PWM_DRIVER
#include "pwm.h"
#endif

#ifdef CONFIG_ENABLE_SM2135EH_DRIVER
#include "sm2135eh.h"
#endif

#ifdef CONFIG_ENABLE_BP57x8D_DRIVER
#include "bp57x8d.h"
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

/**
 * @brief Supported drivers
 *
 */
typedef enum {
    DRIVER_SELECT_INVALID = 0,
    /* PWM */
    DRIVER_ESP_PWM = 1,

    /* IIC */
    DRIVER_SM2135E,     // This version is no longer supported. Please checkout to v0.5.2
    DRIVER_SM2135EH,
    DRIVER_SM2x35EGH,   // Available for SM2235EGH SM2335EGH
    DRIVER_BP57x8D,     // Available for BP5758 BP5758D BP5768D
    DRIVER_BP1658CJ,
    DRIVER_KP18058,

    /* Single Bus */
    DRIVER_WS2812,

    DRIVER_SELECT_MAX,
} lightbulb_driver_t;

/**
 * @brief Supported LED bead combinations.
 */
typedef enum {
    LED_BEADS_INVALID = 0, /**< Invalid LED bead combination. */

    LED_BEADS_1CH_C,       /**< Single-channel: Cold white LED bead. */
    LED_BEADS_1CH_W,       /**< Single-channel: Warm white LED bead. */

    LED_BEADS_2CH_CW,      /**< Two channels: Warm white + cold white LED beads combination. */

    LED_BEADS_3CH_RGB,     /**< Three channels: Red + green + blue LED beads combination. */

    LED_BEADS_4CH_RGBC,    /**< Four channels: Red + green + blue + cold white LED beads combination. */
    LED_BEADS_4CH_RGBCC,   /**< Four channels: Red + green + blue + 2 * cold white LED beads combination. */
    LED_BEADS_4CH_RGBW,    /**< Four channels: Red + green + blue + warm white LED beads combination. */
    LED_BEADS_4CH_RGBWW,    /**< Four channels: Red + green + blue + 2 * warm white LED beads combination. */

    LED_BEADS_5CH_RGBCW,   /**< Five channels: Red + green + blue + cold white + warm white LED beads combination. */
    LED_BEADS_5CH_RGBCC,   /**< Five channels: Red + green + blue + 2 * cold white + RGB mix warm white LED beads combination. */
    LED_BEADS_5CH_RGBWW,   /**< Five channels: Red + green + blue + 2 * warm white + RGB mix cold white LED beads combination. */
    LED_BEADS_5CH_RGBC,    /**< Five channels: Red + green + blue + cold white + RGB mix warm white LED beads combination. */
    LED_BEADS_5CH_RGBW,    /**< Five channels: Red + green + blue + warm white + RGB mix cold white LED beads combination. */

    LED_BEADS_MAX,         /**< Maximum number of supported LED bead combinations. */
} lightbulb_led_beads_comb_t;

/**
 * @brief Supported effects
 *
 */
typedef enum {
    EFFECT_BREATH = 0,
    EFFECT_BLINK = 1,
} lightbulb_effect_t;

/**
 * @brief Lighting test units.
 */
typedef enum {
    LIGHTING_RAINBOW = BIT(0),                      /**< Rainbow lighting effect. */
    LIGHTING_WARM_TO_COLD = BIT(1),                 /**< Transition from warm to cold lighting. */
    LIGHTING_COLD_TO_WARM = BIT(2),                 /**< Transition from cold to warm lighting. */
    LIGHTING_BASIC_FIVE = BIT(3),                   /**< Basic five lighting colors. */
    LIGHTING_COLOR_MUTUAL_WHITE = BIT(4),           /**< Color and mutual white lighting. */
    LIGHTING_COLOR_EFFECT = BIT(5),                 /**< Color-specific lighting effect. */
    LIGHTING_WHITE_EFFECT = BIT(6),                 /**< White-specific lighting effect. */
    LIGHTING_ALEXA = BIT(7),                        /**< Alexa integration lighting. */
    LIGHTING_COLOR_VALUE_INCREMENT = BIT(8),        /**< Incrementing color values lighting. */
    LIGHTING_WHITE_BRIGHTNESS_INCREMENT = BIT(9),   /**< Incrementing white brightness lighting. */
    LIGHTING_ALL_UNIT = UINT32_MAX,                /**< All lighting units. */
} lightbulb_lighting_unit_t;

/**
 * @brief CCT (Correlated Color Temperature) mapping data
 *
 * @note This structure is used for precise color temperature calibration.
 *       Each color temperature value (cct_kelvin) needs to have a corresponding percentage (cct_percentage) determined,
 *       which is used to calibrate the color temperature more accurately.
 *       The rgbcw array specifies the current coefficients for the RGBCW channels.
 *       These coefficients are instrumental in adjusting the intensity of each color channel
 *       (Red, Green, Blue, Cool White, Warm White) to achieve the desired color temperature.
 *       They are also used for power limiting to ensure energy efficiency and LED longevity.
 *       Therefore, the sum of all values in the rgbcw array must equal 1 to maintain the correct power balance.
 *
 */
typedef struct {
    float rgbcw[5];             /**< Array of float coefficients for CCT data (R, G, B, C, W).
                                    These coefficients are used to adjust the intensity of each color channel. */

    uint8_t cct_percentage;     /**< Percentage representation of the color temperature.
                                    Used to calibrate the light's color temperature within a predefined range. */

    uint16_t cct_kelvin;        /**< The specific color temperature value in Kelvin.
                                    Used to define the perceived warmth or coolness of the light emitted. */
} lightbulb_cct_mapping_data_t;

/**
 * @brief Gamma correction and color balance configuration
 *
 * @note This structure is used for calibrating the brightness proportions and color balance of a lightbulb.
 *       Typically, the human eye perceives changes in brightness in a non-linear manner, which is why gamma correction is necessary.
 *       This helps to adjust the brightness to match the non-linear perception of the human eye, ensuring a more natural and
 *       visually comfortable light output. The color balance coefficients are used to adjust the intensity of each color channel
 *       (Red, Green, Blue, Cool White, Warm White) to achieve the desired color balance and overall light quality.
 *
 */
typedef struct {
    float balance_coefficient[5]; /**< Array of float coefficients for adjusting the intensity of each color channel (R, G, B, C, W).
                                       These coefficients help in achieving the desired color balance for the light output. */

    float curve_coefficient;      /**< Coefficient for gamma correction. This value is used to modify the luminance levels
                                       to suit the non-linear characteristics of human vision, thus improving the overall
                                       visual appearance of the light. */
} lightbulb_gamma_config_t;

/**
 * @brief The working mode of the lightbulb.
 */
typedef enum {
    WORK_INVALID = 0,           /**< Invalid working mode. */
    WORK_COLOR = 1,             /**< Color mode, where the lightbulb emits colored light. */
    WORK_WHITE = 2,             /**< White mode, where the lightbulb emits white light. */
} lightbulb_works_mode_t;

/**
 * @brief The working status of the lightbulb.
 *
 * @attention Both the variable `value` and the variable `brightness` are used to mark light brightness.
 *            They respectively indicate the brightness of color light and white light.
 *
 * @note Due to the differences in led beads, the percentage does not represent color temperature.
 *       The real meaning is the output ratio of cold and warm led beads: 0% lights up only the warm beads, 50% lights up an equal number of cold and warm beads, and 100% lights up only the cold beads.
 *       For simplicity, we can roughly assume that 0% represents the lowest color temperature of the beads, and 100% represents the highest color temperature.
 *       The meaning of intermediate percentages varies under different color temperature calibration schemes:
 *          - Standard Mode: The percentage is proportionally mapped between the lowest and highest color temperatures.
 *          - Precise Mode:
 *              - For hardware CCT schemes (only applicable to PWM-driven): The percentage actually represents the duty cycle of the PWM-driven color temperature channel, with each percentage corresponding to an accurate and real color temperature.
 *              - For those with CW channels: The percentage represents the proportion of cold and warm beads involved in the output. The increase in percentage does not linearly correspond to the increase in color temperature, and instruments are needed for accurate determination.
 *              - For those using RGB channels to mix cold and warm colors: The percentage has no real significance and can be ignored. In actual use, it can serve as an index to reference corresponding color temperature values."
 */
typedef struct {
    lightbulb_works_mode_t mode; /**< The working mode of the lightbulb (color, white, etc.). */
    bool on;                     /**< On/off status of the lightbulb. */

    /* Use HSV model to display color light */
    uint16_t hue;                /**< Hue value for color light (range: 0-360). */
    uint8_t saturation;          /**< Saturation value for color light (range: 0-100). */
    uint8_t value;               /**< Brightness value for color light (range: 0-100). */

    /* Use CCT (color temperatures) to display white light */
    /**
     * 0%    ->  .. -> 100%
     * 2200K ->  .. -> 7000K
     * warm  ->  .. -> cold
     *
     */
    uint8_t cct_percentage;      /**< Cold and warm led bead output ratio (range: 0-100). */
    uint8_t brightness;          /**< Brightness value for white light (range: 0-100). */
} lightbulb_status_t;

/**
 * @brief Output limit or gain without changing color.
 */
typedef struct {
    /* Scale the incoming value
     * range: 10% <= value <= 100%
     * step: 1%
     * default min: 10%
     * default max: 100%
     */
    uint8_t white_max_brightness; /**< Maximum brightness limit for white light output. */
    uint8_t white_min_brightness; /**< Minimum brightness limit for white light output. */
    uint8_t color_max_value;      /**< Maximum value limit for color light output. */
    uint8_t color_min_value;      /**< Minimum value limit for color light output. */

    /* Dynamically adjust the final power
     * range: 100% <= value <= 500%
     * step: 10%
     */
    uint16_t white_max_power;     /**< Maximum power limit for white light output. */
    uint16_t color_max_power;     /**< Maximum power limit for color light output. */
} lightbulb_power_limit_t;

/**
 * @brief Used to map percentages to Kelvin values.
 */
typedef struct {
    uint16_t min; /**< Minimum color temperature value in Kelvin, default is 2200 K. */
    uint16_t max; /**< Maximum color temperature value in Kelvin, default is 7000 K. */
} lightbulb_cct_kelvin_range_t;

/**
 * @brief Function pointer type to store lightbulb status.
 *
 * @param status The lightbulb_status_t structure containing the current status of the lightbulb.
 * @return esp_err_t
 *
 */
typedef esp_err_t (*lightbulb_status_storage_cb_t)(lightbulb_status_t status);

/**
 * @brief Some Lightbulb Capability Configuration Options.
 */
typedef struct {
    uint16_t fade_time_ms;                      /**< Fade time in milliseconds (ms), data range: 100ms - 3000ms, default is 800ms. */
    uint16_t storage_delay_ms;                  /**< Storage delay time in milliseconds (ms), used to mitigate adverse effects of short-term repeated erasing and writing of NVS. */
    lightbulb_led_beads_comb_t led_beads;       /**< Configuration for the combination of LED beads. Please select the appropriate type for the onboard LED. */
    lightbulb_status_storage_cb_t storage_cb;   /**< Callback function to be called when the lightbulb status starts to be stored. */
    bool enable_fade : 1;                       /**< Enable this option to use fade effects for color switching instead of direct rapid changes. */
    bool enable_lowpower : 1;                   /**< Enable low-power regulation when the lights are off. */
    bool enable_status_storage : 1;             /**< Enable this option to store the lightbulb state in NVS. */
    bool enable_hardware_cct : 1;               /**< Enable this option if your driver uses hardware CCT. Some PWM type drivers may need to set this option. */
    bool enable_precise_cct_control : 1;        /**< Enable this option if you need precise CCT control. Must set 'enable_hardware_cct' to false in order to enable it.*/
    bool sync_change_brightness_value : 1;      /**< Enable this option if you need to use a parameter to mark the brightness of the white and color output. */
    bool disable_auto_on : 1;                   /**< Enable this option if you don't need automatic on when the color/white value is set. */
} lightbulb_capability_t;

/**
 * @brief Port enumeration names for IIC chips.
 */
typedef enum {
    OUT1 = 0,   /**< IIC output port 1. */
    OUT2,       /**< IIC output port 2. */
    OUT3,       /**< IIC output port 3. */
    OUT4,       /**< IIC output port 4. */
    OUT5,       /**< IIC output port 5. */
    OUT_MAX,    /**< The maximum value for the IIC output port enumeration, this is invalid value. */
} lightbulb_iic_out_pin_t;

/**
 * @brief Lightbulb Configuration Options.
 */
typedef struct {
    lightbulb_driver_t type;                  /**< Type of the lightbulb driver. */

    union {
#ifdef CONFIG_ENABLE_PWM_DRIVER
        driver_pwm_t pwm;
#endif
#ifdef CONFIG_ENABLE_SM2135EH_DRIVER
        driver_sm2135eh_t sm2135eh;
#endif
#ifdef CONFIG_ENABLE_BP57x8D_DRIVER
        driver_bp57x8d_t bp57x8d;
#endif
#ifdef CONFIG_ENABLE_BP1658CJ_DRIVER
        driver_bp1658cj_t bp1658cj;
#endif
#ifdef CONFIG_ENABLE_SM2x35EGH_DRIVER
        driver_sm2x35egh_t sm2x35egh;
#endif
#ifdef CONFIG_ENABLE_KP18058_DRIVER
        driver_kp18058_t kp18058;
#endif
#ifdef CONFIG_ENABLE_WS2812_DRIVER
        driver_ws2812_t ws2812;
#endif
    } driver_conf;                          /**< Configuration specific to the lightbulb driver. */

    /**
     * This configuration is used to set up the CCT (Correlated Color Temperature) calibration scheme.
     * The default mode is the standard mode, which requires no additional configuration.
     * For the precise mode, a color mixing table needs to be configured.
     * To enable this precise CCT control, the 'enable_precise_cct_control' should be set to true in the capability settings.
     */
    union {
        struct {
            uint16_t kelvin_min;
            uint16_t kelvin_max;
        } standard;
        struct {
            lightbulb_cct_mapping_data_t *table;
            int table_size;
        } precise;
    } cct_mix_mode;

    lightbulb_gamma_config_t *gamma_conf;       /**< Pointer to the gamma configuration data. */
    lightbulb_power_limit_t *external_limit;    /**< Pointer to the external power limit configuration. */

    union {
        struct {
            gpio_num_t red;
            gpio_num_t green;
            gpio_num_t blue;
            gpio_num_t cold_cct;
            gpio_num_t warm_brightness;
        } pwm_io;                             /**< Configuration for PWM driver I/O pins. */

        struct {
            lightbulb_iic_out_pin_t red;
            lightbulb_iic_out_pin_t green;
            lightbulb_iic_out_pin_t blue;
            lightbulb_iic_out_pin_t cold_white;
            lightbulb_iic_out_pin_t warm_yellow;
        } iic_io;                             /**< Configuration for IIC driver I/O pins. */
    } io_conf;                                /**< Union for I/O configuration based on the selected driver type. */

    lightbulb_capability_t capability;        /**< Lightbulb capability configuration. */
    lightbulb_status_t init_status;           /**< Initial status of the lightbulb. */
} lightbulb_config_t;

/**
 * @brief Effect function configuration options.
 */
typedef struct {
    lightbulb_effect_t effect_type;      /**< Type of the effect to be configured. */
    lightbulb_works_mode_t mode;        /**< Working mode of the lightbulb during the effect. */
    uint8_t red;                        /**< Red component value for the effect (0-255). */
    uint8_t green;                      /**< Green component value for the effect (0-255). */
    uint8_t blue;                       /**< Blue component value for the effect (0-255). */
    uint16_t cct;                       /**< Color temperature value for the effect. */
    uint8_t min_brightness;             /**< Minimum brightness level for the effect (0-100). */
    uint8_t max_brightness;             /**< Maximum brightness level for the effect (0-100). */
    uint16_t effect_cycle_ms;           /**< Cycle time for the effect in milliseconds (ms). */

    /*
     * If total_ms > 0 will enable auto-stop timer.
     * When the timer triggers, it will automatically call the stop function and the user callback function (if provided).
     */
    int total_ms;                       /**< Total duration of the effect in milliseconds (ms). If greater than 0, enables an auto-stop timer. */
    void (*user_cb)(void);              /**< User-defined callback function to be called when the auto-stop timer triggers. */

    /*
     * If set to true, the auto-stop timer can only be stopped by effect_stop/effect_start interfaces or triggered by FreeRTOS.
     * Any set APIs will only save the status, the status will not be written.
     */
    bool interrupt_forbidden;           /**< If true, the auto-stop timer can only be stopped by specific interfaces or FreeRTOS triggers. */
} lightbulb_effect_config_t;

/**
 * @brief Initialize the lightbulb.
 *
 * @param config Pointer to the configuration parameters for the lightbulb.
 * @return esp_err_t
 */
esp_err_t lightbulb_init(lightbulb_config_t *config);

/**
 * @brief Deinitialize the lightbulb and release resources.
 *
 * @return esp_err_t
 */
esp_err_t lightbulb_deinit(void);

/**
 * @brief Set lightbulb fade time.
 *
 * @param fade_time_ms Fade time in milliseconds (ms). Range: 100ms - 3000ms.
 * @return esp_err_t
 */
esp_err_t lightbulb_set_fade_time(uint32_t fade_time_ms);

/**
 * @brief Enable/Disable the lightbulb fade function.
 *
 * @param is_enable A boolean flag indicating whether to enable (true) or disable (false) the fade function.
 * @return esp_err_t
 */
esp_err_t lightbulb_set_fades_function(bool is_enable);

/**
 * @brief Enable/Disable the lightbulb storage function.
 *
 * @param is_enable A boolean flag indicating whether to enable (true) or disable (false) the storage function.
 * @return esp_err_t
 */
esp_err_t lightbulb_set_storage_function(bool is_enable);

/**
 * @brief Re-update the lightbulb status.
 *
 * @param new_status Pointer to the new status to be applied to the lightbulb.
 * @param trigger A boolean flag indicating whether the update should be triggered immediately.
 * @return esp_err_t
 */
esp_err_t lightbulb_update_status(lightbulb_status_t *new_status, bool trigger);

/**
 * @brief Get lightbulb fade function enabled status.
 *
 * @return true if the fade function is enabled.
 * @return false if the fade function is disabled.
 */
bool lightbulb_get_fades_function_status(void);

/**
 * @brief Convert HSV model to RGB model.
 * @note RGB model color depth is 8 bit (0-255).
 *
 * @param hue Hue value in the range of 0-360 degrees.
 * @param saturation Saturation value in the range of 0-100.
 * @param value Value (brightness) value in the range of 0-100.
 * @param red Pointer to a variable to store the resulting Red component (0-255).
 * @param green Pointer to a variable to store the resulting Green component (0-255).
 * @param blue Pointer to a variable to store the resulting Blue component (0-255).
 * @return esp_err_t
 */
esp_err_t lightbulb_hsv2rgb(uint16_t hue, uint8_t saturation, uint8_t value, uint8_t *red, uint8_t *green, uint8_t *blue);

/**
 * @brief Convert RGB model to HSV model.
 * @note RGB model color depth is 8 bit (0-255).
 *
 * @param red Red component value in the range of 0-255.
 * @param green Green component value in the range of 0-255.
 * @param blue Blue component value in the range of 0-255.
 * @param hue Pointer to a variable to store the resulting Hue value (0-360 degrees).
 * @param saturation Pointer to a variable to store the resulting Saturation value (0-100).
 * @param value Pointer to a variable to store the resulting Value (brightness) value (0-100).
 * @return esp_err_t
 */
esp_err_t lightbulb_rgb2hsv(uint16_t red, uint16_t green, uint16_t blue, uint16_t *hue, uint8_t *saturation, uint8_t *value);

/**
 * @brief Convert xyY model to RGB model.
 * @note Refer: https://www.easyrgb.com/en/convert.php#inputFORM
 *              https://www.easyrgb.com/en/math.php
 *
 * @param x x coordinate value in the range of 0 to 1.0.
 * @param y y coordinate value in the range of 0 to 1.0.
 * @param Y Y (luminance) value in the range of 0 to 100.0.
 * @param red Pointer to a variable to store the resulting Red component (0-255).
 * @param green Pointer to a variable to store the resulting Green component (0-255).
 * @param blue Pointer to a variable to store the resulting Blue component (0-255).
 * @return esp_err_t An error code indicating the success or failure of the operation.
 */
esp_err_t lightbulb_xyy2rgb(float x, float y, float Y, uint8_t *red, uint8_t *green, uint8_t *blue);

/**
 * @brief Convert RGB model to xyY model.
 *
 * @param red Red component value in the range of 0 to 255.
 * @param green Green component value in the range of 0 to 255.
 * @param blue Blue component value in the range of 0 to 255.
 * @param x Pointer to a variable to store the resulting x coordinate (0-1.0).
 * @param y Pointer to a variable to store the resulting y coordinate (0-1.0).
 * @param Y Pointer to a variable to store the resulting Y (luminance) value (0-100.0).
 * @return esp_err_t An error code indicating the success or failure of the operation.
 */
esp_err_t lightbulb_rgb2xyy(uint8_t red, uint8_t green, uint8_t blue, float *x, float *y, float *Y);

/**
 * @brief Convert CCT (Color Temperature) kelvin to percentage.
 *
 * @param kelvin Default range: 2200k - 7000k (Color Temperature in kelvin).
 * @param percentage Pointer to a variable to store the resulting percentage (0 - 100).
 * @return esp_err_t
 */
esp_err_t lightbulb_kelvin2percentage(uint16_t kelvin, uint8_t *percentage);

/**
 * @brief Convert percentage to CCT (Color Temperature) kelvin.
 * @attention
 *
 * @param percentage Percentage value in the range of 0 to 100.
 * @param kelvin Pointer to a variable to store the resulting Color Temperature in kelvin.
 *               Default range: 2200k - 7000k.
 * @return esp_err_t
 */
esp_err_t lightbulb_percentage2kelvin(uint8_t percentage, uint16_t *kelvin);

/**
 * @brief Set the hue value.
 *
 * @param hue Hue value in the range of 0-360 degrees.
 * @return esp_err_t An error code indicating the success or failure of the operation.
 */
esp_err_t lightbulb_set_hue(uint16_t hue);

/**
 * @brief Set the saturation value.
 *
 * @param saturation Saturation value in the range of 0-100.
 * @return esp_err_t An error code indicating the success or failure of the operation.
 */
esp_err_t lightbulb_set_saturation(uint8_t saturation);

/**
 * @brief Set the value (brightness) of the lightbulb.
 *
 * @param value Brightness value in the range of 0-100.
 * @return esp_err_t An error code indicating the success or failure of the operation.
 */
esp_err_t lightbulb_set_value(uint8_t value);

/**
 * @brief Set the color temperature (CCT) of the lightbulb.
 * @note Supports using either percentage or Kelvin values.
 *
 * @param cct CCT value in the range of 0-100 or 2200-7000.
 * @return esp_err_t An error code indicating the success or failure of the operation.
 */
esp_err_t lightbulb_set_cct(uint16_t cct);

/**
 * @brief Set the brightness of the lightbulb.
 *
 * @param brightness Brightness value in the range of 0-100.
 * @return esp_err_t An error code indicating the success or failure of the operation.
 */
esp_err_t lightbulb_set_brightness(uint8_t brightness);

/**
 * @brief Set the xyY color model for the lightbulb.
 * @attention The xyY color model cannot fully correspond to the HSV color model, so the color may be biased.
 *            The grayscale will be recalculated in lightbulb, so we cannot directly operate the underlying driver through the xyY interface.
 *
 * @param x x-coordinate value in the range of 0 to 1.0.
 * @param y y-coordinate value in the range of 0 to 1.0.
 * @param Y Y-coordinate (luminance) value in the range of 0 to 100.0.
 * @return esp_err_t An error code indicating the success or failure of the operation.
 */
esp_err_t lightbulb_set_xyy(float x, float y, float Y);

/**
 * @brief Set the HSV (Hue, Saturation, Value) color model for the lightbulb.
 *
 * @param hue Hue value in the range of 0 to 360 degrees.
 * @param saturation Saturation value in the range of 0 to 100.
 * @param value Value (brightness) value in the range of 0 to 100.
 * @return esp_err_t An error code indicating the success or failure of the operation.
 */
esp_err_t lightbulb_set_hsv(uint16_t hue, uint8_t saturation, uint8_t value);

/**
 * @brief Set the color temperature (CCT) and brightness of the lightbulb.
 * @note Supports using either percentage or Kelvin values.
 *
 * @param cct CCT value in the range of 0-100 or 2200-7000.
 * @param brightness Brightness value in the range of 0-100.
 * @return esp_err_t An error code indicating the success or failure of the operation.
 */
esp_err_t lightbulb_set_cctb(uint16_t cct, uint8_t brightness);

/**
 * @brief Set the on/off status of the lightbulb.
 *
 * @param status On/off status (true for on, false for off).
 * @return esp_err_t An error code indicating the success or failure of the operation.
 */
esp_err_t lightbulb_set_switch(bool status);

/**
 * @brief Get the hue value of the lightbulb.
 *
 * @return int16_t The hue value in the range of 0 to 360 degrees.
 */
int16_t lightbulb_get_hue(void);

/**
 * @brief Get the saturation value of the lightbulb.
 *
 * @return int8_t The saturation value in the range of 0 to 100.
 */
int8_t lightbulb_get_saturation(void);

/**
 * @brief Get the value (brightness) of the lightbulb.
 *
 * @return int8_t The brightness value in the range of 0 to 100.
 */
int8_t lightbulb_get_value(void);

/**
 * @brief Get the color temperature (CCT) percentage of the lightbulb.
 *
 * @return int8_t The CCT percentage value in the range of 0 to 100.
 */
int8_t lightbulb_get_cct_percentage(void);

/**
 * @brief Get the color temperature (CCT) Kelvin value of the lightbulb.
 *
 * @return int16_t The CCT Kelvin value in the range of 2200 to 7000.
 */
int16_t lightbulb_get_cct_kelvin(void);

/**
 * @brief Get the brightness value of the lightbulb.
 *
 * @return int8_t The brightness value in the range of 0 to 100.
 */
int8_t lightbulb_get_brightness(void);

/**
 * @brief Get the on/off status of the lightbulb.
 *
 * @return true The lightbulb is on.
 * @return false The lightbulb is off.
 */
bool lightbulb_get_switch(void);

/**
 * @brief Get the work mode of the lightbulb.
 *
 * @return lightbulb_works_mode_t The current work mode of the lightbulb.
 */
lightbulb_works_mode_t lightbulb_get_mode(void);

/**
 * @brief Get all the status details of the lightbulb.
 *
 * @param status A pointer to a `lightbulb_status_t` structure where the status details will be stored.
 * @return esp_err_t An error code indicating the success or failure of the operation.
 */
esp_err_t lightbulb_get_all_detail(lightbulb_status_t *status);

/**
 * @brief Get the lightbulb status from NVS.
 *
 * @param value Pointer to a `lightbulb_status_t` structure where the stored state will be read into.
 * @return esp_err_t An error code indicating the success or failure of the operation.
 */
esp_err_t lightbulb_status_get_from_nvs(lightbulb_status_t *value);

/**
 * @brief Store the lightbulb state to NVS.
 *
 * @param value Pointer to a `lightbulb_status_t` structure representing the current running state to be stored.
 * @return esp_err_t An error code indicating the success or failure of the operation.
 */
esp_err_t lightbulb_status_set_to_nvs(const lightbulb_status_t *value);

/**
 * @brief Erase the lightbulb state stored in NVS.
 *
 * @return esp_err_t An error code indicating the success or failure of the operation.
 */
esp_err_t lightbulb_status_erase_nvs_storage(void);

/**
 * @brief Start some blinking/breathing effects.
 *
 * @param config Pointer to a `lightbulb_effect_config_t` structure containing the configuration for the effect.
 * @return esp_err_t An error code indicating the success or failure of the operation.
 */
esp_err_t lightbulb_basic_effect_start(lightbulb_effect_config_t *config);

/**
 * @brief Stop the effect in progress and keep the current lighting output.
 *
 * @return esp_err_t An error code indicating the success or failure of the operation.
 */
esp_err_t lightbulb_basic_effect_stop(void);

/**
 * @brief Stop the effect in progress and restore the previous lighting output.
 *
 * @return esp_err_t An error code indicating the success or failure of the operation.
 */
esp_err_t lightbulb_basic_effect_stop_and_restore(void);

/**
 * @brief Used to test lightbulb hardware functionality.
 *
 * @param mask A bitmask representing the test unit or combination of test units to be tested.
 * @param speed_ms The switching speed in milliseconds for the lighting patterns.
 */
void lightbulb_lighting_output_test(lightbulb_lighting_unit_t mask, uint16_t speed_ms);

#ifdef __cplusplus
}
#endif
