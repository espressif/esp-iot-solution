/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_check.h"
#include "keyboard_button.h"
#include "kbd_gpio.h"
#include "kbd_gptimer.h"

static const char *TAG = "keyboard_button";

#define OUTPUT_MASK_HIGE 0xFFFFFFFF
#define OUTPUT_MASK_LOW  0x00000000

#define KBD_TIMER_NOTIFY (1<<0)
#define KBD_EXIT         (1<<1)
#define KBD_EXIT_OK      (1<<2)

#define CALL_EVENT_CB(ev)                                                   \
    if (kbd->cb_info[ev]) {                                                 \
        for (int i = 0; i < kbd->cb_size[ev]; i++) {                           \
            kbd->cb_info[ev][i].cb(kbd, report,kbd->cb_info[ev][i].user_data);      \
        }                                                                   \
    }                                                                       \

typedef struct {
    keyboard_btn_callback_t cb;
    void *user_data;
    keyboard_btn_event_data_t event_data;
} kbd_cb_info_t;

typedef struct keyboard_btn_t {
    int *input_gpios;
    int input_gpio_num;
    int *output_gpios;
    int output_gpio_num;
    bool enable_power_save;
    bool can_enter_power_save;
    EventGroupHandle_t event_group;
    gptimer_handle_t gptimer_handle;
    kbd_cb_info_t *cb_info[KBD_EVENT_MAX];
    size_t cb_size[KBD_EVENT_MAX];
    bool gptimer_start;
    uint32_t debounce_ticks;
    uint32_t ticks_interval;
    uint32_t key_pressed_num;
    uint8_t  active_level;
    /*!< Size: output_gpio_num * sizeof(uint32_t) */
    uint8_t *button_level;
    /*!< Size: output_gpio_num * input_gpio_num * sizeof(uint8_t) */
    uint8_t  *debounce_cnt;
    /*!< Size: output_gpio_num * input_gpio_num * sizeof(keyboard_data_t) */
    keyboard_btn_data_t *key_data;
    /*!< Size: output_gpio_num * input_gpio_num * sizeof(keyboard_data_t) */
    keyboard_btn_data_t *key_release_data;
} keyboard_btn_t;

static bool IRAM_ATTR kbd_gptimer_cb(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_ctx)
{
    keyboard_btn_t *kbd = (keyboard_btn_t *)user_ctx;
    /*!< Determine if xSemaphore exists */
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    /*!< Floating-point numbers cannot be computed in an interrupt, informing the task to start an arithmetic control */
    xEventGroupSetBitsFromISR(kbd->event_group, KBD_TIMER_NOTIFY, &xHigherPriorityTaskWoken);
    return (xHigherPriorityTaskWoken == pdTRUE);
}

static inline void kbd_handler(keyboard_btn_t *kbd)
{
    bool key_change_inc_flag = false;
    bool key_change_dec_flag = false;
    uint32_t key_last_pressed_num = kbd->key_pressed_num;
    uint32_t key_release_num = 0;
    // Clear all output level
    uint32_t output_level = kbd->active_level ? OUTPUT_MASK_LOW : OUTPUT_MASK_HIGE;
    if (kbd->enable_power_save) {
        kbd_gpios_set_hold_dis(kbd->output_gpios, kbd->output_gpio_num);
    }
    kbd_gpios_set_level(kbd->output_gpios, kbd->output_gpio_num, output_level);

    for (int i = 0; i < kbd->output_gpio_num; i++) {
        /*!< Set the output level */
        kbd_gpio_set_level(kbd->output_gpios[i], kbd->active_level ? 1 : 0);
        /*!< Read the input level */
        uint32_t input_level = kbd_gpios_read_level(kbd->input_gpios, kbd->input_gpio_num);
        /*!< Clear the output level */
        kbd_gpio_set_level(kbd->output_gpios[i], kbd->active_level ? 0 : 1);
        for (int j = 0; j < kbd->input_gpio_num; j++) {
            uint8_t currect_btn_num = i * kbd->input_gpio_num + j;
            uint8_t read_gpio_level = (input_level >> j) & 0x01;
            uint8_t button_level = kbd->active_level ? kbd->button_level[currect_btn_num] : !kbd->button_level[currect_btn_num];
            if (read_gpio_level != button_level) {
                kbd->can_enter_power_save = false;
                if (++kbd->debounce_cnt[currect_btn_num] >= kbd->debounce_ticks) {
                    kbd->debounce_cnt[currect_btn_num] = 0;
                    kbd->button_level[currect_btn_num] = kbd->active_level ? read_gpio_level : !read_gpio_level;
                    // Make a report
                    if (kbd->button_level[currect_btn_num] == 1) {
                        key_change_inc_flag = true;
                        kbd->key_data[kbd->key_pressed_num].output_index = i;
                        kbd->key_data[kbd->key_pressed_num].input_index = j;
                        kbd->key_pressed_num++;
                    } else {
                        key_change_dec_flag = true;
                        /*!< Remove kbd->key_data and move the data forward */
                        for (int k = 0; k < kbd->key_pressed_num; k++) {
                            if (kbd->key_data[k].output_index == i && kbd->key_data[k].input_index == j) {
                                for (int l = k; l < kbd->key_pressed_num; l++) {
                                    kbd->key_data[l] = kbd->key_data[l + 1];
                                }
                                break;
                            }
                        }
                        kbd->key_pressed_num--;

                        kbd->key_release_data[key_release_num].output_index = i;
                        kbd->key_release_data[key_release_num].input_index = j;
                        key_release_num++;
                    }
                }
            } else {
                kbd->debounce_cnt[currect_btn_num] = 0;
            }
        }

    }

    if (!(key_change_inc_flag || key_change_dec_flag)) {
        return;
    }

    /*!< Report the pressed event */
    keyboard_btn_report_t report = {
        .key_data = kbd->key_data,
        .key_pressed_num = kbd->key_pressed_num,
        .key_release_num = key_release_num,
        .key_change_num = kbd->key_pressed_num - key_last_pressed_num,
        .key_release_data = kbd->key_release_data
    };
    CALL_EVENT_CB(KBD_EVENT_PRESSED);

    /*!< Check the combination event */
    if (key_change_inc_flag && kbd->cb_info[KBD_EVENT_COMBINATION]) {
        for (int i = 0; i < kbd->cb_size[KBD_EVENT_COMBINATION]; i++) {
            kbd_cb_info_t *cb_info = &kbd->cb_info[KBD_EVENT_COMBINATION][i];
            keyboard_btn_data_t *combination_key_data = cb_info->event_data.combination.key_data;
            uint8_t key_num = cb_info->event_data.combination.key_num;
            if (key_num == kbd->key_pressed_num) {
                uint8_t key_pressed = true;
                for (int j = 0; j < key_num; j++) {
                    if (combination_key_data[j].output_index != kbd->key_data[j].output_index ||
                            combination_key_data[j].input_index != kbd->key_data[j].input_index) {
                        key_pressed = false;
                        break;
                    }
                }

                if (key_pressed) {
                    cb_info->cb(kbd, report, cb_info->user_data);
                }
            }
        }
    }
}

// TODO: Add to kconfig
#define CONFIG_KEYBOARD_TEST_RUN_TIME 0

#if CONFIG_KEYBOARD_TEST_RUN_TIME
#include "esp_timer.h"
#endif

static void kbd_task(void *args)
{
    keyboard_btn_t *kbd = (keyboard_btn_t *)args;
    EventBits_t uxBits;
    while (1) {
        /*!< Waiting for the notification */
        uxBits = xEventGroupWaitBits(kbd->event_group, KBD_TIMER_NOTIFY | KBD_EXIT, pdTRUE, pdFALSE, portMAX_DELAY);
        if (uxBits & KBD_EXIT) {
            ESP_LOGI(TAG, "kbd task exit");
            break;
        }
        if (uxBits & KBD_TIMER_NOTIFY) {
            /*!< Keyboard handler */
#if CONFIG_KEYBOARD_TEST_RUN_TIME
            uint64_t start_time = esp_timer_get_time();
#endif
            if (kbd->enable_power_save) {
                kbd->can_enter_power_save = true;
            }
            kbd_handler(kbd);
            if (kbd->enable_power_save && kbd->key_pressed_num == 0 && kbd->can_enter_power_save == true) {
                /*!< Enter power save */
                ESP_LOGD(TAG, "Enter power save");
                kbd_gptimer_stop(kbd->gptimer_handle);
                kbd->gptimer_start = false;
                kbd_gpios_intr_control(kbd->input_gpios, kbd->input_gpio_num, true);
                kbd_gpios_set_level(kbd->output_gpios, kbd->output_gpio_num, kbd->active_level ? OUTPUT_MASK_HIGE : OUTPUT_MASK_LOW);
                kbd_gpios_set_hold_en(kbd->output_gpios, kbd->output_gpio_num);
            }

#if CONFIG_KEYBOARD_TEST_RUN_TIME
            uint64_t end_time = esp_timer_get_time();
            uint64_t duration = end_time - start_time;
            printf("Task duration: %llu microseconds\n", duration);
#endif
        }
    }
    xEventGroupSetBits(kbd->event_group, KBD_EXIT_OK);
    vTaskDelete(NULL);
}

static void IRAM_ATTR kbd_power_save_isr_handler(void* arg)
{
    keyboard_btn_t *kbd = (keyboard_btn_t *)arg;
    if (!kbd->gptimer_start) {
        kbd_gptimer_start(kbd->gptimer_handle);
        kbd->gptimer_start = true;
    }
    kbd_gpios_intr_control(kbd->input_gpios, kbd->input_gpio_num, false);
}

esp_err_t keyboard_button_create(keyboard_btn_config_t *kbd_cfg, keyboard_btn_handle_t *kbd_handle)
{
    ESP_LOGI(TAG, "KeyBoard Button Version: %d.%d.%d", KEYBOARD_BUTTON_VER_MAJOR, KEYBOARD_BUTTON_VER_MINOR, KEYBOARD_BUTTON_VER_PATCH);

    ESP_RETURN_ON_FALSE(kbd_cfg, ESP_ERR_INVALID_ARG, TAG, "Config cannot be NULL");
    ESP_RETURN_ON_FALSE(kbd_handle, ESP_ERR_INVALID_ARG, TAG, "keyboard handle cannot be NULL");
    ESP_RETURN_ON_FALSE(kbd_cfg->output_gpios, ESP_ERR_INVALID_ARG, TAG, "Output GPIOs cannot be NULL");
    ESP_RETURN_ON_FALSE(kbd_cfg->input_gpios, ESP_ERR_INVALID_ARG, TAG, "Input GPIOs cannot be NULL");
    ESP_RETURN_ON_FALSE(kbd_cfg->output_gpio_num, ESP_ERR_INVALID_ARG, TAG, "Output number cannot be 0");
    ESP_RETURN_ON_FALSE(kbd_cfg->input_gpio_num, ESP_ERR_INVALID_ARG, TAG, "Input number cannot be 0");

    esp_err_t ret = ESP_OK;

    keyboard_btn_t *kbd = calloc(1, sizeof(keyboard_btn_t));
    ESP_RETURN_ON_FALSE(kbd, ESP_ERR_NO_MEM, TAG, "Failed to allocate memory for keyboard");

    int *output_gpios = calloc(kbd_cfg->output_gpio_num, sizeof(int));
    ESP_GOTO_ON_FALSE(output_gpios, ESP_ERR_NO_MEM, exit, TAG, "Failed to allocate memory for output GPIOs");

    int *input_gpios = calloc(kbd_cfg->input_gpio_num, sizeof(int));
    ESP_GOTO_ON_FALSE(input_gpios, ESP_ERR_NO_MEM, exit, TAG, "Failed to allocate memory for input GPIOs");

    kbd->output_gpios = output_gpios;
    kbd->output_gpio_num = kbd_cfg->output_gpio_num;
    memcpy(kbd->output_gpios, kbd_cfg->output_gpios, kbd_cfg->output_gpio_num * sizeof(int));

    kbd->input_gpios = input_gpios;
    kbd->input_gpio_num = kbd_cfg->input_gpio_num;
    memcpy(kbd->input_gpios, kbd_cfg->input_gpios, kbd_cfg->input_gpio_num * sizeof(int));

    kbd->active_level = kbd_cfg->active_level ? 1 : 0;
    kbd->debounce_ticks = kbd_cfg->debounce_ticks;
    kbd->ticks_interval = kbd_cfg->ticks_interval;
    kbd->enable_power_save = kbd_cfg->enable_power_save;
    kbd->button_level = calloc(kbd->output_gpio_num * kbd->input_gpio_num, sizeof(uint8_t));
    ESP_GOTO_ON_FALSE(kbd->button_level, ESP_ERR_NO_MEM, exit, TAG, "Failed to allocate memory for button level");

    kbd->debounce_cnt = calloc(kbd->output_gpio_num * kbd->input_gpio_num, sizeof(uint8_t));
    ESP_GOTO_ON_FALSE(kbd->debounce_cnt, ESP_ERR_NO_MEM, exit, TAG, "Failed to allocate memory for debounce count");

    kbd->key_data = calloc(kbd->output_gpio_num * kbd->input_gpio_num, sizeof(keyboard_btn_data_t));
    ESP_GOTO_ON_FALSE(kbd->key_data, ESP_ERR_NO_MEM, exit, TAG, "Failed to allocate memory for key data");

    kbd->key_release_data = calloc(kbd->output_gpio_num * kbd->input_gpio_num, sizeof(keyboard_btn_data_t));
    ESP_GOTO_ON_FALSE(kbd->key_release_data, ESP_ERR_NO_MEM, exit, TAG, "Failed to allocate memory for key release data");

    kbd_gpio_config_t gpio_cfg = {0};
    gpio_cfg.gpios = kbd_cfg->input_gpios;
    gpio_cfg.gpio_num = kbd_cfg->input_gpio_num;
    gpio_cfg.gpio_mode = KBD_GPIO_MODE_INPUT;
    gpio_cfg.active_level = kbd_cfg->active_level;
    gpio_cfg.enable_power_save = kbd_cfg->enable_power_save;
    ret = kbd_gpio_init(&gpio_cfg);
    ESP_GOTO_ON_FALSE(ret == ESP_OK, ESP_FAIL, exit, TAG, "Failed to initialize input GPIOs");

    gpio_cfg.gpios = kbd_cfg->output_gpios;
    gpio_cfg.gpio_num = kbd_cfg->output_gpio_num;
    gpio_cfg.gpio_mode = KBD_GPIO_MODE_OUTPUT;
    ret = kbd_gpio_init(&gpio_cfg);
    ESP_GOTO_ON_FALSE(ret == ESP_OK, ESP_FAIL, exit, TAG, "Failed to initialize output GPIOs");

    kbd->event_group = xEventGroupCreate();
    ESP_GOTO_ON_FALSE(kbd->event_group, ESP_ERR_NO_MEM, exit, TAG, "Failed to create event group");

    /*!< Create a timer */
    kbd_gptimer_config_t gptimer_cfg = {
        .gptimer = &kbd->gptimer_handle,
        .cbs.on_alarm = kbd_gptimer_cb,
        .user_data = kbd,
        .alarm_count_us = kbd_cfg->ticks_interval,
    };

    uint32_t priority = kbd_cfg->priority;
    if (priority == 0) {
        priority = 5;
    }

    ret = kbd_gptimer_init(&gptimer_cfg);
    ESP_GOTO_ON_FALSE(ret == ESP_OK, ESP_FAIL, exit, TAG, "Failed to initialize gptimer");

    /*!< Start the timer */
    if (!kbd->gptimer_start) {
        ret = kbd_gptimer_start(kbd->gptimer_handle);
        ESP_GOTO_ON_FALSE(ret == ESP_OK, ESP_FAIL, exit, TAG, "Failed to start gptimer");
        kbd->gptimer_start = true;
    }

    if (kbd_cfg->enable_power_save) {
        kbd_gpios_set_intr(kbd->input_gpios, kbd->input_gpio_num, kbd->active_level == 0 ? GPIO_INTR_LOW_LEVEL : GPIO_INTR_HIGH_LEVEL, kbd_power_save_isr_handler, (void *)kbd);
    }

    /*!< Create a task */
    xTaskCreatePinnedToCore(kbd_task, "kbd_task", 4096, kbd, priority, NULL, kbd_cfg->core_id);

    *kbd_handle = kbd;

    return ESP_OK;

exit:
    if (kbd) {
        if (kbd->button_level) {
            free(kbd->button_level);
        }
        if (kbd->debounce_cnt) {
            free(kbd->debounce_cnt);
        }
        if (kbd->key_data) {
            free(kbd->key_data);
        }
        if (kbd->key_release_data) {
            free(kbd->key_release_data);
        }
        if (kbd->input_gpios) {
            free(kbd->input_gpios);
        }
        if (kbd->output_gpios) {
            free(kbd->output_gpios);
        }
        if (kbd->gptimer_handle) {
            kbd_gptimer_deinit(kbd->gptimer_handle);
        }
        if (kbd->event_group) {
            vEventGroupDelete(kbd->event_group);
        }
        free(kbd);
    }
    return ret;
}

esp_err_t keyboard_button_delete(keyboard_btn_handle_t kbd_handle)
{
    ESP_RETURN_ON_FALSE(kbd_handle, ESP_ERR_INVALID_ARG, TAG, "Keyboard handle cannot be NULL");

    keyboard_btn_t *kbd = (keyboard_btn_t *)kbd_handle;
    kbd_gptimer_deinit(kbd->gptimer_handle);
    xEventGroupSetBits(kbd->event_group, KBD_EXIT);
    xEventGroupWaitBits(kbd->event_group, KBD_EXIT_OK, pdTRUE, pdTRUE, portMAX_DELAY);

    kbd_gpio_deinit(kbd->input_gpios, kbd->input_gpio_num);
    kbd_gpio_deinit(kbd->output_gpios, kbd->output_gpio_num);
    if (kbd->button_level) {
        free(kbd->button_level);
    }
    if (kbd->debounce_cnt) {
        free(kbd->debounce_cnt);
    }
    if (kbd->key_data) {
        free(kbd->key_data);
    }
    if (kbd->key_release_data) {
        free(kbd->key_release_data);
    }
    if (kbd->input_gpios) {
        free(kbd->input_gpios);
    }
    if (kbd->output_gpios) {
        free(kbd->output_gpios);
    }
    for (int i = 0; i < KBD_EVENT_MAX; i++) {
        if (kbd->cb_info[i]) {
            for (int j = 0; j < kbd->cb_size[i]; j++) {
                if (kbd->cb_info[i][j].event_data.combination.key_data) {
                    free(kbd->cb_info[i][j].event_data.combination.key_data);
                }
            }
            free(kbd->cb_info[i]);
        }
    }
    free(kbd);
    return ESP_OK;
}

esp_err_t keyboard_button_register_cb(keyboard_btn_handle_t kbd_handle, keyboard_btn_cb_config_t cb_cfg, keyboard_btn_cb_handle_t *rtn_cb_hdl)
{
    ESP_RETURN_ON_FALSE(kbd_handle, ESP_ERR_INVALID_ARG, TAG, "Keyboard handle cannot be NULL");
    ESP_RETURN_ON_FALSE(cb_cfg.callback, ESP_ERR_INVALID_ARG, TAG, "Callback cannot be NULL");
    ESP_RETURN_ON_FALSE(cb_cfg.event < KBD_EVENT_MAX, ESP_ERR_INVALID_ARG, TAG, "Invalid event");

    keyboard_btn_t *kbd = (keyboard_btn_t *)kbd_handle;
    keyboard_btn_event_t event = cb_cfg.event;

    /*!< Expand the event dynamic array. */
    if (!kbd->cb_info[event]) {
        kbd->cb_info[event] = calloc(1, sizeof(kbd_cb_info_t));
        ESP_RETURN_ON_FALSE(kbd->cb_info[event], ESP_ERR_NO_MEM, TAG, "Failed to allocate memory for callback info");

    } else {
        kbd_cb_info_t *p = realloc(kbd->cb_info[event], sizeof(kbd_cb_info_t) * (kbd->cb_size[event] + 1));
        ESP_RETURN_ON_FALSE(p, ESP_ERR_NO_MEM, TAG, "Failed to allocate memory for callback info");
        kbd->cb_info[event] = p;
    }

    kbd_cb_info_t *cb_info = &kbd->cb_info[event][kbd->cb_size[event]];
    kbd->cb_size[event]++;
    cb_info->cb = cb_cfg.callback;
    cb_info->user_data = cb_cfg.user_data;

    /*!< For combination event */
    if (event == KBD_EVENT_COMBINATION) {
        cb_info->event_data.combination.key_num = cb_cfg.event_data.combination.key_num;
        cb_info->event_data.combination.key_data = calloc(cb_cfg.event_data.combination.key_num, sizeof(keyboard_btn_data_t));
        ESP_RETURN_ON_FALSE(cb_info->event_data.combination.key_data, ESP_ERR_NO_MEM, TAG, "Failed to allocate memory for combination key data");
        memcpy(cb_info->event_data.combination.key_data, cb_cfg.event_data.combination.key_data, cb_cfg.event_data.combination.key_num * sizeof(keyboard_btn_data_t));
    }

    if (rtn_cb_hdl) {
        *rtn_cb_hdl = cb_info;
    }

    return ESP_OK;
}
esp_err_t keyboard_button_unregister_cb(keyboard_btn_handle_t kbd_handle, keyboard_btn_event_t event, keyboard_btn_cb_handle_t rtn_cb_hdl)
{
    ESP_RETURN_ON_FALSE(kbd_handle, ESP_ERR_INVALID_ARG, TAG, "Keyboard handle cannot be NULL");
    ESP_RETURN_ON_FALSE(event < KBD_EVENT_MAX, ESP_ERR_INVALID_ARG, TAG, "Invalid event");

    keyboard_btn_t *kbd = (keyboard_btn_t *)kbd_handle;
    if (rtn_cb_hdl) {
        /*!< Find the cb */
        int check = -1;
        for (int i = 0; i < kbd->cb_size[event]; i++) {
            if (&kbd->cb_info[event][i] == (kbd_cb_info_t *)rtn_cb_hdl) {
                check = i;
                break;
            }
        }

        ESP_RETURN_ON_FALSE(check != -1, ESP_ERR_NOT_FOUND, TAG, "Callback not found");
        kbd_cb_info_t *cb_info = &kbd->cb_info[event][check];
        if (cb_info->event_data.combination.key_data) {
            free(cb_info->event_data.combination.key_data);
        }

        /*!< Move the last one to the current position */
        if (check != kbd->cb_size[event] - 1) {
            kbd->cb_info[event][check] = kbd->cb_info[event][kbd->cb_size[event] - 1];
        }

        if (kbd->cb_size[event] != 1) {
            kbd_cb_info_t *p = realloc(kbd->cb_info[event], sizeof(kbd_cb_info_t) * (kbd->cb_size[event] - 1));
            ESP_RETURN_ON_FALSE(p, ESP_ERR_NO_MEM, TAG, "Failed to allocate memory for callback info");
            kbd->cb_info[event] = p;
        } else {
            free(kbd->cb_info[event]);
            kbd->cb_info[event] = NULL;
        }

        kbd->cb_size[event]--;
    } else {
        for (int i = 0; i < kbd->cb_size[event]; i++) {
            kbd_cb_info_t *cb_info = &kbd->cb_info[event][i];
            if (cb_info->event_data.combination.key_data) {
                free(cb_info->event_data.combination.key_data);
            }
        }
        free(kbd->cb_info[event]);
        kbd->cb_info[event] = NULL;
        kbd->cb_size[event] = 0;
    }
    return ESP_OK;
}

esp_err_t keyboard_button_get_index_by_gpio(keyboard_btn_handle_t kbd_handle, uint32_t gpio_num, kbd_gpio_mode_t gpio_mode, uint32_t *index)
{
    ESP_RETURN_ON_FALSE(kbd_handle, ESP_ERR_INVALID_ARG, TAG, "Keyboard handle cannot be NULL");
    ESP_RETURN_ON_FALSE(index, ESP_ERR_INVALID_ARG, TAG, "Index cannot be NULL");

    keyboard_btn_t *kbd = (keyboard_btn_t *)kbd_handle;
    if (gpio_mode == KBD_GPIO_MODE_INPUT) {
        for (int i = 0; i < kbd->input_gpio_num; i++) {
            if (kbd->input_gpios[i] == gpio_num) {
                *index = i;
                return ESP_OK;
            }
        }
    } else if (gpio_mode == KBD_GPIO_MODE_OUTPUT) {
        for (int i = 0; i < kbd->output_gpio_num; i++) {
            if (kbd->output_gpios[i] == gpio_num) {
                *index = i;
                return ESP_OK;
            }
        }
    }
    return ESP_ERR_NOT_FOUND;
}

esp_err_t keyboard_button_get_gpio_by_index(keyboard_btn_handle_t kbd_handle, uint32_t index, kbd_gpio_mode_t gpio_mode, uint32_t *gpio_num)
{
    ESP_RETURN_ON_FALSE(kbd_handle, ESP_ERR_INVALID_ARG, TAG, "Keyboard handle cannot be NULL");
    ESP_RETURN_ON_FALSE(gpio_num, ESP_ERR_INVALID_ARG, TAG, "GPIO number cannot be NULL");

    keyboard_btn_t *kbd = (keyboard_btn_t *)kbd_handle;
    if (gpio_mode == KBD_GPIO_MODE_INPUT) {
        if (index < kbd->input_gpio_num) {
            *gpio_num = kbd->input_gpios[index];
            return ESP_OK;
        }
    } else if (gpio_mode == KBD_GPIO_MODE_OUTPUT) {
        if (index < kbd->output_gpio_num) {
            *gpio_num = kbd->output_gpios[index];
            return ESP_OK;
        }
    }
    return ESP_ERR_NOT_FOUND;
}
