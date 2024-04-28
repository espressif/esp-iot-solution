/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_io.h"
#include "unity.h"
#include "unity_test_runner.h"
#include "unity_test_utils_memory.h"
#include "esp_heap_caps.h"
#include "esp_lcd_usb_display.h"
#include "esp_lcd_panel_ops.h"

#define TEST_DISP_BIT_PER_PIXEL     (24)

#define TEST_DISP_H_RES             CONFIG_UVC_CAM1_FRAMESIZE_WIDTH
#define TEST_DISP_V_RES             CONFIG_UVC_CAM1_FRAMESIZE_HEIGT

#define TEST_DISPLAY_BUF_NUMS       (1)
#define TEST_DISP_SIZE              (TEST_DISP_V_RES * TEST_DISP_H_RES * TEST_DISP_BIT_PER_PIXEL / 8)

#define TEST_JPEG_SUB_SAMPLE        (JPEG_DOWN_SAMPLING_YUV420)
#define TEST_JPEG_ENC_QUALITY       (80)
#define TEST_JPEG_TASK_PRIORITY     (4)
#define TEST_JPEG_TASK_CORE         (-1)

#define TEST_DELAY_TIME_MS          (3000)

static char *TAG = "usb_display_test";

static void test_draw_bitmap(esp_lcd_panel_handle_t panel_handle)
{
    uint16_t row_line = TEST_DISP_V_RES / TEST_DISP_BIT_PER_PIXEL;
    uint8_t byte_per_pixel = TEST_DISP_BIT_PER_PIXEL / 8;
    uint8_t *color = (uint8_t *)heap_caps_calloc(1, row_line * TEST_DISP_H_RES * byte_per_pixel, MALLOC_CAP_DMA);
    TEST_ASSERT_NOT_NULL(color);

    for (int j = 0; j < TEST_DISP_BIT_PER_PIXEL; j++) {
        for (int i = 0; i < row_line * TEST_DISP_H_RES; i++) {
            for (int k = 0; k < byte_per_pixel; k++) {
                color[i * byte_per_pixel + k] = (BIT(j) >> (k * 8)) & 0xff;
            }
        }
        TEST_ESP_OK(esp_lcd_panel_draw_bitmap(panel_handle, 0, j * row_line, TEST_DISP_H_RES, (j + 1) * row_line, color));
    }
    free(color);
}

TEST_CASE("draw color bar with usb uvc", "[usb display][uvc]")
{
    ESP_LOGI(TAG, "Install LCD data panel");
    esp_lcd_panel_handle_t display_panel;
    usb_display_vendor_config_t vendor_config = DEFAULT_USB_DISPLAY_VENDOR_CONFIG(TEST_DISP_H_RES, TEST_DISP_V_RES, TEST_DISP_BIT_PER_PIXEL, display_panel);

    ESP_ERROR_CHECK(esp_lcd_new_panel_usb_display(&vendor_config, &display_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(display_panel));

    test_draw_bitmap(display_panel);
    vTaskDelay(pdMS_TO_TICKS(TEST_DELAY_TIME_MS));
}

void app_main(void)
{
    /*
            __    ___      ___ _____  __    ___  __    _
      /\ /\/ _\  / __\    /   \\_   \/ _\  / _ \/ /   /_\ /\_/\
     / / \ \ \  /__\//   / /\ / / /\/\ \  / /_)/ /   //_\\\_ _/
     \ \_/ /\ \/ \/  \  / /_//\/ /_  _\ \/ ___/ /___/  _  \/ \
      \___/\__/\_____/ /___,'\____/  \__/\/   \____/\_/ \_/\_/
    */

    printf("        __    ___      ___ _____  __    ___  __    _        \n");
    printf("  /\\ /\\/ _\\  / __\\    /   \\_   \\/ _\\  / _ \\/ /   /_\\ /\\_/\\ \n");
    printf(" / / \\ \\ \\  /__\\//   / /\\ / / /\\/\\ \\  / /_)/ /   //_\\\\_ _/ \n");
    printf(" \\ \\_/ /\\ \\/ \\/  \\  / /_//\\/ /_  _\\ \\/ ___/ /___/  _  \\/ \\  \n");
    printf("  \\___/\\__/\\_____/ /___,'\\____/  \\__/\\/   \\____/\\_/ \\_/\\_/  \n");
    unity_run_menu();
}
