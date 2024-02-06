/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "lv_demos.h"

#include "esp_lcd_st77903_rgb.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_io_additions.h"
#include "esp_lcd_panel_io_interface.h"

#include "lv_port.h"

#define LOG_SYSTEM_INFO    (0)

#if LOG_SYSTEM_INFO
static esp_err_t print_real_time_stats(TickType_t xTicksToWait);
#endif /* LOG_SYSTEM_INFO */

static const char *TAG = "example";

void app_main()
{
    esp_lcd_panel_handle_t panel_handle = NULL;

    ESP_LOGI(TAG, "Turn off LCD backlight");
    gpio_config_t bk_gpio_config = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << EXAMPLE_PIN_NUM_BK_LIGHT
    };
    ESP_ERROR_CHECK(gpio_config(&bk_gpio_config));

    ESP_LOGI(TAG, "Install 3-wire SPI panel IO");
    spi_line_config_t line_config = {
        .cs_io_type = IO_TYPE_GPIO,
        .cs_expander_pin = EXAMPLE_PIN_NUM_LCD_SPI_CS,
        .scl_io_type = IO_TYPE_GPIO,
        .scl_expander_pin = EXAMPLE_PIN_NUM_LCD_SPI_SCK,
        .sda_io_type = IO_TYPE_GPIO,
        .sda_expander_pin = EXAMPLE_PIN_NUM_LCD_SPI_SDO,
        .io_expander = NULL,
    };
    esp_lcd_panel_io_3wire_spi_config_t io_config = ST77903_RGB_PANEL_IO_3WIRE_SPI_CONFIG(line_config, 0);
    esp_lcd_panel_io_handle_t io_handle = NULL;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_3wire_spi(&io_config, &io_handle));

    ESP_LOGI(TAG, "Install st77903 panel driver");
    const esp_lcd_rgb_panel_config_t rgb_config = {
        .clk_src = LCD_CLK_SRC_DEFAULT,
        .psram_trans_align = 64,
        .data_width = EXAMPLE_LCD_RGB_DATA_WIDTH,
        .bits_per_pixel = EXAMPLE_RGB_BIT_PER_PIXEL,
        .de_gpio_num = EXAMPLE_PIN_NUM_LCD_RGB_DE,
        .pclk_gpio_num = EXAMPLE_PIN_NUM_LCD_RGB_PCLK,
        .vsync_gpio_num = EXAMPLE_PIN_NUM_LCD_RGB_VSYNC,
        .hsync_gpio_num = EXAMPLE_PIN_NUM_LCD_RGB_HSYNC,
        .disp_gpio_num = EXAMPLE_PIN_NUM_LCD_RGB_DISP,
        .data_gpio_nums = {
            EXAMPLE_PIN_NUM_LCD_RGB_DATA0,
            EXAMPLE_PIN_NUM_LCD_RGB_DATA1,
            EXAMPLE_PIN_NUM_LCD_RGB_DATA2,
            EXAMPLE_PIN_NUM_LCD_RGB_DATA3,
            EXAMPLE_PIN_NUM_LCD_RGB_DATA4,
            EXAMPLE_PIN_NUM_LCD_RGB_DATA5,
            EXAMPLE_PIN_NUM_LCD_RGB_DATA6,
            EXAMPLE_PIN_NUM_LCD_RGB_DATA7,
        },
        .timings = ST77903_RGB_320_480_PANEL_48HZ_RGB_TIMING(),
        .flags.fb_in_psram = 1,
    };
    st77903_vendor_config_t vendor_config = {
        .rgb_config = &rgb_config,
        .flags = {
            .auto_del_panel_io = 1,
        },
    };
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = EXAMPLE_PIN_NUM_LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = EXAMPLE_LCD_BIT_PER_PIXEL,
        .vendor_config = &vendor_config,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st77903_rgb(io_handle, &panel_config, &panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_handle, true, false));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));

    lv_port_init(panel_handle);

    ESP_LOGI(TAG, "Turn on LCD backlight");
    gpio_set_level(EXAMPLE_PIN_NUM_BK_LIGHT, EXAMPLE_LCD_BK_LIGHT_ON_LEVEL);

    ESP_LOGI(TAG, "Display LVGL demos");
    // Lock the mutex due to the LVGL APIs are not thread-safe
    if (lv_port_lock(-1)) {
        // lv_demo_stress();
        // lv_demo_benchmark();
        lv_demo_music();

        // Release the mutex
        lv_port_unlock();
    }

#if LOG_SYSTEM_INFO
    static char buffer[2048];
    while (1) {
        /**
         * It's not recommended to frequently use functions like `heap_caps_get_free_size()` to obtain memory information
         * in practical applications, especially when the application extensively uses `malloc()` to dynamically allocate
         * a significant number of memory blocks. The frequent interrupt disabling may potentially lead to issues with other functionalities.
         */
        sprintf(buffer, "\t  Biggest /     Free /    Total\n"
                " SRAM : [%8d / %8d / %8d]\n"
                "PSRAM : [%8d / %8d / %8d]\n",
                heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
                heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                heap_caps_get_total_size(MALLOC_CAP_INTERNAL),
                heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM),
                heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
                heap_caps_get_total_size(MALLOC_CAP_SPIRAM));
        printf("------------ Memory ------------\n");
        printf("%s\n", buffer);

        vTaskList(buffer);
        printf("------------ Task State ------------\n");
        printf("Name\t\tStatus\tPrio\tHWM\tTask\tCore\n");
        printf("******************************************************\n");
        printf("%s\n", buffer);

        ESP_ERROR_CHECK(print_real_time_stats(pdMS_TO_TICKS(2000)));
        printf("\n");
    }
#endif
}

#if LOG_SYSTEM_INFO
#define ARRAY_SIZE_OFFSET   5   //Increase this if print_real_time_stats returns ESP_ERR_INVALID_SIZE

/**
 * @brief   Function to print the CPU usage of tasks over a given duration.
 *
 * This function will measure and print the CPU usage of tasks over a specified
 * number of ticks (i.e. real time stats). This is implemented by simply calling
 * uxTaskGetSystemState() twice separated by a delay, then calculating the
 * differences of task run times before and after the delay.
 *
 * @note    If any tasks are added or removed during the delay, the stats of
 *          those tasks will not be printed.
 * @note    This function should be called from a high priority task to minimize
 *          inaccuracies with delays.
 * @note    When running in dual core mode, each core will correspond to 50% of
 *          the run time.
 *
 * @param   xTicksToWait    Period of stats measurement
 *
 * @return
 *  - ESP_OK                Success
 *  - ESP_ERR_NO_MEM        Insufficient memory to allocated internal arrays
 *  - ESP_ERR_INVALID_SIZE  Insufficient array size for uxTaskGetSystemState. Trying increasing ARRAY_SIZE_OFFSET
 *  - ESP_ERR_INVALID_STATE Delay duration too short
 */
static esp_err_t print_real_time_stats(TickType_t xTicksToWait)
{
    TaskStatus_t *start_array = NULL, *end_array = NULL;
    UBaseType_t start_array_size, end_array_size;
    uint32_t start_run_time, end_run_time;
    esp_err_t ret;

    // Allocate array to store current task states
    start_array_size = uxTaskGetNumberOfTasks() + ARRAY_SIZE_OFFSET;
    start_array = malloc(sizeof(TaskStatus_t) * start_array_size);
    if (start_array == NULL) {
        ret = ESP_ERR_NO_MEM;
        goto exit;
    }
    // Get current task states
    start_array_size = uxTaskGetSystemState(start_array, start_array_size, &start_run_time);
    if (start_array_size == 0) {
        ret = ESP_ERR_INVALID_SIZE;
        goto exit;
    }

    vTaskDelay(xTicksToWait);

    // Allocate array to store tasks states post delay
    end_array_size = uxTaskGetNumberOfTasks() + ARRAY_SIZE_OFFSET;
    end_array = malloc(sizeof(TaskStatus_t) * end_array_size);
    if (end_array == NULL) {
        ret = ESP_ERR_NO_MEM;
        goto exit;
    }
    // Get post delay task states
    end_array_size = uxTaskGetSystemState(end_array, end_array_size, &end_run_time);
    if (end_array_size == 0) {
        ret = ESP_ERR_INVALID_SIZE;
        goto exit;
    }

    // Calculate total_elapsed_time in units of run time stats clock period.
    uint32_t total_elapsed_time = (end_run_time - start_run_time);
    if (total_elapsed_time == 0) {
        ret = ESP_ERR_INVALID_STATE;
        goto exit;
    }

    printf("------------ Task Run Time ------------\n");
    printf("| Task | Run Time | Percentage\n");
    // Match each task in start_array to those in the end_array
    for (int i = 0; i < start_array_size; i++) {
        int k = -1;
        for (int j = 0; j < end_array_size; j++) {
            if (start_array[i].xHandle == end_array[j].xHandle) {
                k = j;
                //Mark that task have been matched by overwriting their handles
                start_array[i].xHandle = NULL;
                end_array[j].xHandle = NULL;
                break;
            }
        }
        // Check if matching task found
        if (k >= 0) {
            uint32_t task_elapsed_time = end_array[k].ulRunTimeCounter - start_array[i].ulRunTimeCounter;
            uint32_t percentage_time = (task_elapsed_time * 100UL) / (total_elapsed_time * portNUM_PROCESSORS);
            printf("| %s | %ld | %ld%%\n", start_array[i].pcTaskName, task_elapsed_time, percentage_time);
        }
    }

    // Print unmatched tasks
    for (int i = 0; i < start_array_size; i++) {
        if (start_array[i].xHandle != NULL) {
            printf("| %s | Deleted\n", start_array[i].pcTaskName);
        }
    }
    for (int i = 0; i < end_array_size; i++) {
        if (end_array[i].xHandle != NULL) {
            printf("| %s | Created\n", end_array[i].pcTaskName);
        }
    }
    ret = ESP_OK;

exit:    // Common return path
    free(start_array);
    free(end_array);
    return ret;
}
#endif /* LOG_SYSTEM_INFO */
