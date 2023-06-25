/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <inttypes.h>

#include "driver/i2c.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_lcd_panel_io_interface.h"
#include "unity.h"
#include "unity_test_runner.h"

#include "esp_io_expander_tca9554.h"
#include "esp_lcd_panel_io_additions.h"

static char *TAG = "panel_io_additions_test";

#define TEST_I2C_MASTER_NUM         (I2C_NUM_0)
#define TEST_I2C_CLK_SPEED          (400 * 1000)
#define TEST_I2C_SCL_GPIO_NUM       (GPIO_NUM_18)
#define TEST_I2C_SDA_GPIO_NUM       (GPIO_NUM_8)

#define TEST_IO_EXPANDER_TC9554_ADDRESS     (ESP_IO_EXPANDER_I2C_TCA9554_ADDRESS_000)

#define TEST_SPI_LINE_CS_GPIO_NUM   (GPIO_NUM_10)
#define TEST_SPI_LINE_SCL_GPIO_NUM  (GPIO_NUM_11)
#define TEST_SPI_LINE_SDA_GPIO_NUM  (GPIO_NUM_12)
#define TEST_SPI_LINE_CS_EX_PIN     (IO_EXPANDER_PIN_NUM_1)
#define TEST_SPI_LINE_SCL_EX_PIN    (IO_EXPANDER_PIN_NUM_2)
#define TEST_SPI_LINE_SDA_EX_PIN    (IO_EXPANDER_PIN_NUM_3)

#define TEST_SET_SPI_LINE_GPIO(line)                    \
    do {                                                \
        line.cs_io_type = IO_TYPE_GPIO;                 \
        line.cs_gpio_num = TEST_SPI_LINE_CS_GPIO_NUM;   \
        line.scl_io_type = IO_TYPE_GPIO;                \
        line.scl_gpio_num = TEST_SPI_LINE_SCL_GPIO_NUM; \
        line.sda_io_type = IO_TYPE_GPIO;                \
        line.sda_gpio_num = TEST_SPI_LINE_SDA_GPIO_NUM; \
        line.io_expander = NULL;                        \
    } while (0)

#define TEST_SET_SPI_LINE_EXPANDER(line, handle)          \
    do {                                                  \
        line.cs_io_type = IO_TYPE_EXPANDER;               \
        line.cs_expander_pin = TEST_SPI_LINE_CS_EX_PIN;   \
        line.scl_io_type = IO_TYPE_EXPANDER;              \
        line.scl_expander_pin = TEST_SPI_LINE_SCL_EX_PIN; \
        line.sda_io_type = IO_TYPE_EXPANDER;              \
        line.sda_expander_pin = TEST_SPI_LINE_SDA_EX_PIN; \
        line.io_expander = handle;                        \
    } while (0)

typedef enum {
    SPI_LINE_TYPE_GPIO = 0,
    SPI_LINE_TYPE_EXPANDER,
} test_3wire_spi_line_type_t;

static uint32_t test_cmd = 0x11223344;
static uint8_t test_param[] = {0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc};

static esp_lcd_panel_io_handle_t test_3wire_spi_common_init(test_3wire_spi_line_type_t line_type, esp_io_expander_handle_t ex_handle,
                                                            bool use_dc_bit, int bytes_per_cmd_param)
{
    spi_line_config_t spi_line;

    if (line_type == SPI_LINE_TYPE_GPIO) {
        TEST_SET_SPI_LINE_GPIO(spi_line);
    } else {
        TEST_ASSERT(ex_handle != NULL);
        TEST_SET_SPI_LINE_EXPANDER(spi_line, ex_handle);
    }
    esp_lcd_panel_io_3wire_spi_config_t config = {
        .line_config = spi_line,
        .expect_clk_speed = PANEL_IO_3WIRE_SPI_CLK_MAX,
        .spi_mode = 0,
        .lcd_cmd_bytes = bytes_per_cmd_param,
        .lcd_param_bytes = bytes_per_cmd_param,
        .flags = {
            .use_dc_bit = use_dc_bit,
            .dc_zero_on_data = 0,
            .lsb_first = 0,
            .cs_high_active = 0,
            .del_keep_cs_inactive = 0,
        },
    };
    esp_lcd_panel_io_handle_t io = NULL;
    TEST_ESP_OK(esp_lcd_new_panel_io_3wire_spi(&config, &io));

    return io;
}

TEST_CASE("test 3-wire spi use GPIO to write data", "[3wire_spi][gpio]")
{
    esp_lcd_panel_io_handle_t io_handle = NULL;
    uint32_t cmd = 0;
    uint32_t param_1 = 0;
    uint32_t param_2 = 0;
    for (int i = 1; i <= sizeof(uint32_t); i++) {
        cmd = test_cmd & (0xffffffff >> (32 - 8 * i));
        param_1 = 0;
        param_2 = 0;
        int j = 0;
        for (; j < i; j++) {
            param_1 >>= 8;
            param_1 |= (uint32_t)test_param[j] << 24;
            param_2 >>= 8;
            param_2 |= (uint32_t)test_param[i + j] << 24;
        }
        for (; j < sizeof(uint32_t); j++) {
            param_1 >>= 8;
            param_2 >>= 8;
        }

        ESP_LOGI(TAG, "test %d-bit command and parameters (no D/C bit)", 8 * i);
        ESP_LOGI(TAG, "send command[0x%"PRIx32"], parameters[0x%"PRIx32"][0x%"PRIx32"]", cmd, param_1, param_2);
        io_handle = NULL;
        io_handle = test_3wire_spi_common_init(SPI_LINE_TYPE_GPIO, NULL, false, i);
        TEST_ASSERT(io_handle != NULL);
        TEST_ESP_OK(esp_lcd_panel_io_tx_param(io_handle, test_cmd, test_param, i * 2));
        TEST_ESP_OK(esp_lcd_panel_io_del(io_handle));

        ESP_LOGI(TAG, "test %d-bit command and parameters (with D/C bit)", 8 * i + 1);
        ESP_LOGI(TAG, "send command[0x%"PRIx32"], parameters[0x1%"PRIx32"][0x1%"PRIx32"]", cmd, param_1, param_2);
        io_handle = NULL;
        io_handle = test_3wire_spi_common_init(SPI_LINE_TYPE_GPIO, NULL, true, i);
        TEST_ASSERT(io_handle != NULL);
        TEST_ESP_OK(esp_lcd_panel_io_tx_param(io_handle, test_cmd, test_param, i * 2));
        TEST_ESP_OK(esp_lcd_panel_io_del(io_handle));
    }
}

TEST_CASE("test 3-wire spi use IO expander to write data", "[3wire_spi][expander]")
{
    const i2c_config_t i2c_conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = TEST_I2C_SDA_GPIO_NUM,
        .sda_pullup_en = GPIO_PULLUP_DISABLE,
        .scl_io_num = TEST_I2C_SCL_GPIO_NUM,
        .scl_pullup_en = GPIO_PULLUP_DISABLE,
        .master.clk_speed = TEST_I2C_CLK_SPEED,
    };
    TEST_ESP_OK(i2c_param_config(TEST_I2C_MASTER_NUM, &i2c_conf));
    TEST_ESP_OK(i2c_driver_install(TEST_I2C_MASTER_NUM, i2c_conf.mode, 0, 0, 0));
    esp_io_expander_handle_t expander_handle = NULL;
    TEST_ESP_OK(esp_io_expander_new_i2c_tca9554(TEST_I2C_MASTER_NUM, TEST_IO_EXPANDER_TC9554_ADDRESS, &expander_handle));

    esp_lcd_panel_io_handle_t io_handle = NULL;
    uint32_t cmd = 0;
    uint32_t param_1 = 0;
    uint32_t param_2 = 0;
    for (int i = 1; i <= sizeof(uint32_t); i++) {
        cmd = test_cmd & (0xffffffff >> (32 - 8 * i));
        param_1 = 0;
        param_2 = 0;
        int j = 0;
        for (; j < i; j++) {
            param_1 >>= 8;
            param_1 |= (uint32_t)test_param[j] << 24;
            param_2 >>= 8;
            param_2 |= (uint32_t)test_param[i + j] << 24;
        }
        for (; j < sizeof(uint32_t); j++) {
            param_1 >>= 8;
            param_2 >>= 8;
        }

        ESP_LOGI(TAG, "test %d-bit command and parameters (no D/C bit)", 8 * i);
        ESP_LOGI(TAG, "send command[0x%"PRIx32"], parameters[0x%"PRIx32"][0x%"PRIx32"]", cmd, param_1, param_2);
        io_handle = NULL;
        io_handle = test_3wire_spi_common_init(SPI_LINE_TYPE_EXPANDER, expander_handle, false, i);
        TEST_ASSERT(io_handle != NULL);
        TEST_ESP_OK(esp_lcd_panel_io_tx_param(io_handle, test_cmd, test_param, i * 2));
        TEST_ESP_OK(esp_lcd_panel_io_del(io_handle));

        ESP_LOGI(TAG, "test %d-bit command and parameters (with D/C bit)", 8 * i + 1);
        ESP_LOGI(TAG, "send command[0x%"PRIx32"], parameters[0x1%"PRIx32"][0x1%"PRIx32"]", cmd, param_1, param_2);
        io_handle = NULL;
        io_handle = test_3wire_spi_common_init(SPI_LINE_TYPE_EXPANDER, expander_handle, true, i);
        TEST_ASSERT(io_handle != NULL);
        TEST_ESP_OK(esp_lcd_panel_io_tx_param(io_handle, test_cmd, test_param, i * 2));
        TEST_ESP_OK(esp_lcd_panel_io_del(io_handle));
    }

    TEST_ESP_OK(esp_io_expander_del(expander_handle));
    TEST_ESP_OK(i2c_driver_delete(TEST_I2C_MASTER_NUM));
}

// Some resources are lazy allocated in the LCD driver, the threadhold is left for that case
#define TEST_MEMORY_LEAK_THRESHOLD (-300)

static size_t before_free_8bit;
static size_t before_free_32bit;

static void check_leak(size_t before_free, size_t after_free, const char *type)
{
    ssize_t delta = after_free - before_free;
    printf("MALLOC_CAP_%s: Before %u bytes free, After %u bytes free (delta %d)\n", type, before_free, after_free, delta);
    TEST_ASSERT_MESSAGE(delta >= TEST_MEMORY_LEAK_THRESHOLD, "memory leak");
}

void setUp(void)
{
    before_free_8bit = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    before_free_32bit = heap_caps_get_free_size(MALLOC_CAP_32BIT);
}

void tearDown(void)
{
    size_t after_free_8bit = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    size_t after_free_32bit = heap_caps_get_free_size(MALLOC_CAP_32BIT);
    check_leak(before_free_8bit, after_free_8bit, "8BIT");
    check_leak(before_free_32bit, after_free_32bit, "32BIT");
}

void app_main(void)
{
    //    ___                 _    _____  ___     _       _     _ _ _   _                   _            _
    //   / _ \__ _ _ __   ___| |   \_   \/___\   /_\   __| | __| (_) |_(_) ___  _ __  ___  | |_ ___  ___| |_
    //  / /_)/ _` | '_ \ / _ \ |    / /\//  //  //_\\ / _` |/ _` | | __| |/ _ \| '_ \/ __| | __/ _ \/ __| __|
    // / ___/ (_| | | | |  __/ | /\/ /_/ \_//  /  _  \ (_| | (_| | | |_| | (_) | | | \__ \ | ||  __/\__ \ |_
    // \/    \__,_|_| |_|\___|_| \____/\___/   \_/ \_/\__,_|\__,_|_|\__|_|\___/|_| |_|___/  \__\___||___/\__|
    printf("   ___                 _    _____  ___     _       _     _ _ _   _                   _            _\r\n");
    printf("  / _ \\__ _ _ __   ___| |   \\_   \\/___\\   /_\\   __| | __| (_) |_(_) ___  _ __  ___  | |_ ___  ___| |_\r\n");
    printf(" / /_)/ _` | '_ \\ / _ \\ |    / /\\//  //  //_\\\\ / _` |/ _` | | __| |/ _ \\| '_ \\/ __| | __/ _ \\/ __| __|\r\n");
    printf("/ ___/ (_| | | | |  __/ | /\\/ /_/ \\_//  /  _  \\ (_| | (_| | | |_| | (_) | | | \\__ \\ | ||  __/\\__ \\ |_\r\n");
    printf("\\/    \\__,_|_| |_|\\___|_| \\____/\\___/   \\_/ \\_/\\__,_|\\__,_|_|\\__|_|\\___/|_| |_|___/  \\__\\___||___/\\__|\r\n");
    unity_run_menu();
}
