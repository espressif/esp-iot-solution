/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include "indicator.h"

#include <string.h>

#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "sdkconfig.h"

static const char *TAG = "indicator";

#define INDICATOR_MAX_CHANNELS 8
#define INDICATOR_LIGHTS_PER_CHANNEL 3

typedef struct {
    int gpio_r;
    int gpio_y;
    int gpio_g;
} indicator_gpio_t;

typedef struct {
    indicator_light_action_t action;
    bool blink_phase;
} indicator_light_state_t;

static const indicator_gpio_t s_gpio_map[INDICATOR_MAX_CHANNELS] = {
#if CONFIG_VIBE_INDICATOR_CHANNEL_COUNT >= 1
    { CONFIG_VIBE_INDICATOR_CH0_GPIO_R, CONFIG_VIBE_INDICATOR_CH0_GPIO_Y, CONFIG_VIBE_INDICATOR_CH0_GPIO_G },
#endif
#if CONFIG_VIBE_INDICATOR_CHANNEL_COUNT >= 2
    { CONFIG_VIBE_INDICATOR_CH1_GPIO_R, CONFIG_VIBE_INDICATOR_CH1_GPIO_Y, CONFIG_VIBE_INDICATOR_CH1_GPIO_G },
#endif
#if CONFIG_VIBE_INDICATOR_CHANNEL_COUNT >= 3
    { CONFIG_VIBE_INDICATOR_CH2_GPIO_R, CONFIG_VIBE_INDICATOR_CH2_GPIO_Y, CONFIG_VIBE_INDICATOR_CH2_GPIO_G },
#endif
#if CONFIG_VIBE_INDICATOR_CHANNEL_COUNT >= 4
    { CONFIG_VIBE_INDICATOR_CH3_GPIO_R, CONFIG_VIBE_INDICATOR_CH3_GPIO_Y, CONFIG_VIBE_INDICATOR_CH3_GPIO_G },
#endif
#if CONFIG_VIBE_INDICATOR_CHANNEL_COUNT >= 5
    { CONFIG_VIBE_INDICATOR_CH4_GPIO_R, CONFIG_VIBE_INDICATOR_CH4_GPIO_Y, CONFIG_VIBE_INDICATOR_CH4_GPIO_G },
#endif
#if CONFIG_VIBE_INDICATOR_CHANNEL_COUNT >= 6
    { CONFIG_VIBE_INDICATOR_CH5_GPIO_R, CONFIG_VIBE_INDICATOR_CH5_GPIO_Y, CONFIG_VIBE_INDICATOR_CH5_GPIO_G },
#endif
#if CONFIG_VIBE_INDICATOR_CHANNEL_COUNT >= 7
    { CONFIG_VIBE_INDICATOR_CH6_GPIO_R, CONFIG_VIBE_INDICATOR_CH6_GPIO_Y, CONFIG_VIBE_INDICATOR_CH6_GPIO_G },
#endif
#if CONFIG_VIBE_INDICATOR_CHANNEL_COUNT >= 8
    { CONFIG_VIBE_INDICATOR_CH7_GPIO_R, CONFIG_VIBE_INDICATOR_CH7_GPIO_Y, CONFIG_VIBE_INDICATOR_CH7_GPIO_G },
#endif
};

static indicator_light_state_t s_lights[INDICATOR_MAX_CHANNELS][INDICATOR_LIGHTS_PER_CHANNEL];
static SemaphoreHandle_t s_mutex;
static esp_timer_handle_t s_blink_timer;
static bool s_hw_ready;
static uint8_t s_slow_tick;

static int gpio_for_light_id(int channel, int light_id)
{
    if (channel < 0 || channel >= INDICATOR_MAX_CHANNELS) {
        return -1;
    }
    switch (light_id) {
    case 0:
        return s_gpio_map[channel].gpio_r;
    case 1:
        return s_gpio_map[channel].gpio_y;
    case 2:
        return s_gpio_map[channel].gpio_g;
    default:
        return -1;
    }
}

static void gpio_set_safe(int gpio, int level)
{
    if (gpio < 0) {
        return;
    }
    gpio_set_level((gpio_num_t)gpio, level);
}

static bool light_should_be_lit(const indicator_light_state_t *light)
{
    switch (light->action) {
    case INDICATOR_LIGHT_ACTION_ON:
        return true;
    case INDICATOR_LIGHT_ACTION_SLOW_BLINK:
    case INDICATOR_LIGHT_ACTION_FAST_BLINK:
        return light->blink_phase;
    default:
        return false;
    }
}

static void apply_light_locked(int channel, int light_id)
{
    const indicator_light_state_t *light = &s_lights[channel][light_id];
    int gpio = gpio_for_light_id(channel, light_id);

    if (!light_should_be_lit(light)) {
        gpio_set_safe(gpio, 0);
        return;
    }

    if (gpio < 0) {
        return;
    }
    gpio_set_safe(gpio, 1);
}

static void apply_all_locked(void)
{
    for (int ch = 0; ch < CONFIG_VIBE_INDICATOR_CHANNEL_COUNT; ch++) {
        for (int light = 0; light < INDICATOR_LIGHTS_PER_CHANNEL; light++) {
            apply_light_locked(ch, light);
        }
    }
}

static void indicator_lock(void)
{
    if (s_mutex != NULL) {
        (void)xSemaphoreTake(s_mutex, portMAX_DELAY);
    }
}

static void indicator_unlock(void)
{
    if (s_mutex != NULL) {
        (void)xSemaphoreGive(s_mutex);
    }
}

static int slow_blink_divisor(void)
{
    int fast = CONFIG_VIBE_INDICATOR_FAST_BLINK_HALF_MS;
    int slow = CONFIG_VIBE_INDICATOR_SLOW_BLINK_HALF_MS;
    if (fast <= 0) {
        return 1;
    }
    return (slow + fast - 1) / fast;
}

static void blink_timer_cb(void *arg)
{
    (void)arg;
    indicator_lock();

    bool any_blink = false;
    s_slow_tick = (uint8_t)((s_slow_tick + 1) % slow_blink_divisor());

    for (int ch = 0; ch < CONFIG_VIBE_INDICATOR_CHANNEL_COUNT; ch++) {
        for (int light = 0; light < INDICATOR_LIGHTS_PER_CHANNEL; light++) {
            indicator_light_state_t *state = &s_lights[ch][light];
            if (state->action == INDICATOR_LIGHT_ACTION_FAST_BLINK) {
                state->blink_phase = !state->blink_phase;
                any_blink = true;
            } else if (state->action == INDICATOR_LIGHT_ACTION_SLOW_BLINK && s_slow_tick == 0) {
                state->blink_phase = !state->blink_phase;
                any_blink = true;
            }
        }
    }

    if (any_blink) {
        apply_all_locked();
    }
    indicator_unlock();
}

static esp_err_t configure_output_gpio(int gpio)
{
    if (gpio < 0) {
        return ESP_OK;
    }
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << gpio),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    return gpio_config(&cfg);
}

esp_err_t indicator_init(void)
{
    esp_err_t err = ESP_OK;

    if (s_hw_ready) {
        return ESP_OK;
    }

    s_mutex = xSemaphoreCreateMutex();
    if (s_mutex == NULL) {
        ESP_LOGE(TAG, "mutex alloc failed");
        return ESP_ERR_NO_MEM;
    }

    for (int ch = 0; ch < CONFIG_VIBE_INDICATOR_CHANNEL_COUNT; ch++) {
        err = configure_output_gpio(s_gpio_map[ch].gpio_r);
        if (err != ESP_OK) {
            goto err;
        }
        err = configure_output_gpio(s_gpio_map[ch].gpio_y);
        if (err != ESP_OK) {
            goto err;
        }
        err = configure_output_gpio(s_gpio_map[ch].gpio_g);
        if (err != ESP_OK) {
            goto err;
        }
        gpio_set_safe(s_gpio_map[ch].gpio_r, 0);
        gpio_set_safe(s_gpio_map[ch].gpio_y, 0);
        gpio_set_safe(s_gpio_map[ch].gpio_g, 0);
        for (int light = 0; light < INDICATOR_LIGHTS_PER_CHANNEL; light++) {
            int gpio = gpio_for_light_id(ch, light);
            if (gpio < 0) {
                ESP_LOGW(TAG, "indicator %d light %d has no GPIO configured", ch, light);
            }
            s_lights[ch][light].action = INDICATOR_LIGHT_ACTION_OFF;
            s_lights[ch][light].blink_phase = false;
        }
    }

    const esp_timer_create_args_t timer_args = {
        .callback = blink_timer_cb,
        .name = "vibe_blink",
    };
    err = esp_timer_create(&timer_args, &s_blink_timer);
    if (err != ESP_OK) {
        goto err;
    }

    err = esp_timer_start_periodic(s_blink_timer,
                                   (uint64_t)CONFIG_VIBE_INDICATOR_FAST_BLINK_HALF_MS * 1000ULL);
    if (err != ESP_OK) {
        esp_timer_delete(s_blink_timer);
        s_blink_timer = NULL;
        goto err;
    }

    s_hw_ready = true;
    ESP_LOGI(TAG, "ready: %d indicator group(s), slow=%d ms fast=%d ms",
             CONFIG_VIBE_INDICATOR_CHANNEL_COUNT,
             CONFIG_VIBE_INDICATOR_SLOW_BLINK_HALF_MS,
             CONFIG_VIBE_INDICATOR_FAST_BLINK_HALF_MS);
    return ESP_OK;

err:
    if (s_blink_timer != NULL) {
        esp_timer_delete(s_blink_timer);
        s_blink_timer = NULL;
    }
    if (s_mutex != NULL) {
        vSemaphoreDelete(s_mutex);
        s_mutex = NULL;
    }
    return err;
}

int indicator_get_channel_count(void)
{
    return CONFIG_VIBE_INDICATOR_CHANNEL_COUNT;
}

const char *indicator_validate_light(int indicator_id, int light_id, int light_action)
{
    if (indicator_get_channel_count() == 0) {
        return "indicator_count is 0";
    }
    if (indicator_id < 0 || indicator_id >= indicator_get_channel_count()) {
        return "indicator_id out of range";
    }
    if (light_id < 0 || light_id > 2) {
        return "light_id out of range, expected 0-2";
    }
    if (light_action < INDICATOR_LIGHT_ACTION_OFF || light_action > INDICATOR_LIGHT_ACTION_FAST_BLINK) {
        return "unsupported light_action, expected 0-3";
    }
    return NULL;
}

const char *indicator_set_light(int indicator_id, int light_id, int light_action)
{
    const char *err = indicator_validate_light(indicator_id, light_id, light_action);
    if (err != NULL) {
        return err;
    }

    indicator_lock();
    indicator_light_state_t *light = &s_lights[indicator_id][light_id];
    light->action = (indicator_light_action_t)light_action;
    light->blink_phase = (light_action == INDICATOR_LIGHT_ACTION_SLOW_BLINK ||
                          light_action == INDICATOR_LIGHT_ACTION_FAST_BLINK);
    apply_light_locked(indicator_id, light_id);
    indicator_unlock();

    ESP_LOGI(TAG, "indicator=%d light=%d action=%d", indicator_id, light_id, light_action);
    return NULL;
}
