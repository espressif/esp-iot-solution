# ESP LCD ST77922

[![Component Registry](https://components.espressif.com/components/espressif/esp_lcd_st77922/badge.svg)](https://components.espressif.com/components/espressif/esp_lcd_st77922)

Implementation of the ST77922 LCD controller with [esp_lcd](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/lcd.html) component.

| LCD controller | Communication interface | Component name |                               Link to datasheet                               |
| :------------: | :---------------------: | :------------: | :---------------------------------------------------------------------------: |
|     ST77922     |        SPI/QSPI/MIPI-DSI         | esp_lcd_st77922 | [PDF](https://dl.espressif.com/AE/esp-iot-solution/ST77922_SPEC_V0.1.pdf) |

**Note**: MIPI-DSI interface only supports ESP-IDF v5.3 and above versions.

For more information on LCD, please refer to the [LCD documentation](https://docs.espressif.com/projects/esp-iot-solution/en/latest/display/lcd/index.html).

## Add to project

Packages from this repository are uploaded to [Espressif's component service](https://components.espressif.com/).
You can add them to your project via `idf.py add-dependancy`, e.g.
```
    idf.py add-dependency "espressif/esp_lcd_st77922"
```

Alternatively, you can create `idf_component.yml`. More is in [Espressif's documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/tools/idf-component-manager.html).

## Initialization Code

### SPI Interface

```c
    ESP_LOGI(TAG, "Initialize SPI bus");
    const spi_bus_config_t buscfg = ST77922_PANEL_BUS_SPI_CONFIG(EXAMPLE_PIN_NUM_LCD_PCLK, EXAMPLE_PIN_NUM_LCD_DATA0, EXAMPLE_LCD_H_RES * 80 * sizeof(uint16_t));
    ESP_ERROR_CHECK(spi_bus_initialize(EXAMPLE_LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

    ESP_LOGI(TAG, "Install panel IO");
    esp_lcd_panel_io_handle_t io_handle = NULL;
    const esp_lcd_panel_io_spi_config_t io_config = ST77922_PANEL_IO_SPI_CONFIG(EXAMPLE_PIN_NUM_LCD_CS, EXAMPLE_PIN_NUM_LCD_DC,
                                                                               callback, &callback_data);
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)EXAMPLE_LCD_HOST, &io_config, &io_handle));

/**
 * Uncomment these line if use custom initialization commands.
 * The array should be declared as static const and positioned outside the function.
 */
// static const st77922_lcd_init_cmd_t lcd_init_cmds[] = {
// //  {cmd, { data }, data_size, delay_ms}
// {0x28, (uint8_t []){0x00}, 0, 0},
// {0x10, (uint8_t []){0x00}, 0, 0},
// {0x2A, (uint8_t []){0x00, 0x00, 0x02, 0x13}, 4, 0},
// {0x2B, (uint8_t []){0x00, 0x00, 0x01, 0x2B}, 4, 0},
//     ...
// };

    ESP_LOGI(TAG, "Install st77922 panel driver");
    esp_lcd_panel_handle_t panel_handle = NULL;
    const st77922_vendor_config_t vendor_config = {
        // .init_cmds = lcd_init_cmds,         // Uncomment these line if use custom initialization commands
        // .init_cmds_size = sizeof(lcd_init_cmds) / sizeof(st77922_lcd_init_cmd_t),
        .flags = {
            .use_qspi_interface = 0,
        },
    };
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = EXAMPLE_PIN_NUM_LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,     // Implemented by LCD command `36h`
        .bits_per_pixel = EXAMPLE_LCD_BIT_PER_PIXEL,    // Implemented by LCD command `3Ah` (16/18/24)
        .vendor_config = &vendor_config,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st77922(io_handle, &panel_config, &panel_handle));
    esp_lcd_panel_reset(panel_handle);
    esp_lcd_panel_init(panel_handle);
    esp_lcd_panel_disp_on_off(panel_handle, true);
```

### QSPI Interface

```c
    ESP_LOGI(TAG, "Initialize QSPI bus");
    const spi_bus_config_t buscfg = ST77922_PANEL_BUS_QSPI_CONFIG(EXAMPLE_PIN_NUM_LCD_PCLK,
                                                                                 EXAMPLE_PIN_NUM_LCD_DATA0,
                                                                                 EXAMPLE_PIN_NUM_LCD_DATA1,
                                                                                 EXAMPLE_PIN_NUM_LCD_DATA2,
                                                                                 EXAMPLE_PIN_NUM_LCD_DATA3,
                                                                                 EXAMPLE_LCD_H_RES * 80 * sizeof(uint16_t));
    ESP_ERROR_CHECK(spi_bus_initialize(EXAMPLE_LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

    ESP_LOGI(TAG, "Install panel IO");
    esp_lcd_panel_io_handle_t io_handle = NULL;
    const esp_lcd_panel_io_spi_config_t io_config = ST77922_PANEL_IO_QSPI_CONFIG(EXAMPLE_PIN_NUM_LCD_CS, callback, &callback_data);
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)EXAMPLE_LCD_HOST, &io_config, &io_handle));

/**
 * Uncomment these line if use custom initialization commands.
 * The array should be declared as static const and positioned outside the function.
 */
// static const st77922_lcd_init_cmd_t lcd_init_cmds[] = {
// //  {cmd, { data }, data_size, delay_ms}
// {0x28, (uint8_t []){0x00}, 0, 0},
// {0x10, (uint8_t []){0x00}, 0, 0},
// {0x2A, (uint8_t []){0x00, 0x00, 0x02, 0x13}, 4, 0},
// {0x2B, (uint8_t []){0x00, 0x00, 0x01, 0x2B}, 4, 0},
//     ...
// };

    ESP_LOGI(TAG, "Install st77922 panel driver");
    esp_lcd_panel_handle_t panel_handle = NULL;
    const st77922_vendor_config_t vendor_config = {
        // .init_cmds = lcd_init_cmds,         // Uncomment these line if use custom initialization commands
        // .init_cmds_size = sizeof(lcd_init_cmds) / sizeof(st77922_lcd_init_cmd_t),
        .flags = {
            .use_qspi_interface = 1,
        },
    };
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = EXAMPLE_PIN_NUM_LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,     // Implemented by LCD command `36h`
        .bits_per_pixel = EXAMPLE_LCD_BIT_PER_PIXEL,    // Implemented by LCD command `3Ah` (16/18/24)
        .vendor_config = &vendor_config,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st77922(io_handle, &panel_config, &panel_handle));

    esp_lcd_panel_reset(panel_handle);
    esp_lcd_panel_init(panel_handle);
    esp_lcd_panel_disp_on_off(panel_handle, true);
```
#### Notes

* When using `esp_panel_lcd_draw_bitmap()` to refresh the screen, ensure that both `x_start` and `x_end` are divisible by `4`. This is a requirement of ST77922. For LVGL, register the following function into `rounder_cb` of `lv_disp_drv_t` to round the coordinates.

```c
void lvgl_port_rounder_callback(struct _lv_disp_drv_t * disp_drv, lv_area_t * area)
{
    uint16_t x1 = area->x1;
    uint16_t x2 = area->x2;

    // round the start of coordinate down to the nearest 4M number
    area->x1 = (x1 >> 2) << 2;

    // round the end of coordinate up to the nearest 4N+3 number
    area->x2 = ((x2 >> 2) << 2) + 3;
}
```

### RGB Interface

```c
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
        esp_lcd_panel_io_3wire_spi_config_t io_config = ST77922_PANEL_IO_3WIRE_SPI_CONFIG(line_config, 0);
        EXAMPLE_ESP_OK(esp_lcd_new_panel_io_3wire_spi(&io_config, &io_handle));

/**
 * Uncomment these line if use custom initialization commands.
 * The array should be declared as static const and positioned outside the function.
 */
// static const st77922_lcd_init_cmd_t lcd_init_cmds[] = {
// //  {cmd, { data }, data_size, delay_ms}
// {0x28, (uint8_t []){0x00}, 0, 0},
// {0x10, (uint8_t []){0x00}, 0, 0},
// {0x2A, (uint8_t []){0x00, 0x00, 0x02, 0x13}, 4, 0},
// {0x2B, (uint8_t []){0x00, 0x00, 0x01, 0x2B}, 4, 0},
//     ...
// };

        ESP_LOGI(TAG, "Install st77922 panel driver");
        esp_lcd_rgb_panel_config_t rgb_config = {
            .clk_src = LCD_CLK_SRC_DEFAULT,
            .psram_trans_align = 64,
            .data_width = 8,
            .bits_per_pixel = 24,
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
            .timings = ST77922_480_480_PANEL_60HZ_RGB_TIMING(),
            .flags.fb_in_psram = 1,
            .bounce_buffer_size_px = EXAMPLE_LCD_H_RES * 10,
        };
        rgb_config.timings.h_res = EXAMPLE_LCD_H_RES;
        rgb_config.timings.v_res = EXAMPLE_LCD_V_RES;
        const st77922_vendor_config_t vendor_config = {
            .rgb_config = &rgb_config,
            .flags = {
                .use_rgb_interface = 1,
                .enable_io_multiplex = 0,         /**
                                                * Set to 1 if panel IO is no longer needed after LCD initialization.
                                                * If the panel IO pins are sharing other pins of the RGB interface to save GPIOs,
                                                * Please set it to 1 to release the pins.
                                                */
                .mirror_by_cmd = 1,             // Set to 0 if `enable_io_multiplex` is enabled
            },
        };
        const esp_lcd_panel_dev_config_t panel_config = {
            .reset_gpio_num = EXAMPLE_PIN_NUM_LCD_RST,
            .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
            .bits_per_pixel = EXAMPLE_LCD_BIT_PER_PIXEL,
            .vendor_config = &vendor_config,
        };
        EXAMPLE_ESP_OK(esp_lcd_new_panel_st77922(io_handle, &panel_config, &panel_handle));
        EXAMPLE_ESP_OK(esp_lcd_panel_reset(panel_handle));
        EXAMPLE_ESP_OK(esp_lcd_panel_init(panel_handle));
        EXAMPLE_ESP_OK(esp_lcd_panel_disp_on_off(panel_handle, true));
```
### MIPI Interface

```c
/**
 * Uncomment these line if use custom initialization commands.
 * The array should be declared as static const and positioned outside the function.
 */
// static const st77922_lcd_init_cmd_t lcd_init_cmds[] = {
// //   cmd   data        data_size  delay_ms
//    {0xFF, (uint8_t []){0x77, 0x01, 0x00, 0x00, 0x13}, 5, 0},
//    {0xEF, (uint8_t []){0x08}, 1, 0},
//    {0xFF, (uint8_t []){0x77, 0x01, 0x00, 0x00, 0x10}, 5, 0},
//    {0xC0, (uint8_t []){0x3B, 0x00}, 2, 0},
//     ...
// };
    ESP_LOGI(TAG, "MIPI DSI PHY Powered on");
    esp_ldo_channel_config_t ldo_mipi_phy_config = {
        .chan_id = EXAMPLE_MIPI_DSI_PHY_PWR_LDO_CHAN,
        .voltage_mv = EXAMPLE_MIPI_DSI_PHY_PWR_LDO_VOLTAGE_MV,
    };
    ESP_ERROR_CHECK(esp_ldo_acquire_channel(&ldo_mipi_phy_config, &ldo_mipi_phy));

    ESP_LOGI(TAG, "Initialize MIPI DSI bus");
    esp_lcd_dsi_bus_config_t bus_config = ST77922_PANEL_BUS_DSI_1CH_CONFIG();
    ESP_ERROR_CHECK(esp_lcd_new_dsi_bus(&bus_config, &mipi_dsi_bus));

    ESP_LOGI(TAG, "Install panel IO");
    esp_lcd_dbi_io_config_t dbi_config = ST77922_PANEL_IO_DBI_CONFIG();
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_dbi(mipi_dsi_bus, &dbi_config, &mipi_dbi_io));

    ESP_LOGI(TAG, "Install LCD driver of st77922");
    esp_lcd_panel_handle_t panel_handle = NULL;
    esp_lcd_dpi_panel_config_t dpi_config = ST77922_480_360_PANEL_60HZ_DPI_CONFIG(EXAMPLE_MIPI_DPI_PX_FORMAT);
    const st77922_vendor_config_t vendor_config = {
        // .init_cmds = lcd_init_cmds,      // Uncomment these line if use custom initialization commands
        // .init_cmds_size = sizeof(lcd_init_cmds) / sizeof(st77922_lcd_init_cmd_t),
        .flags.use_mipi_interface = 1,
        .mipi_config = {
            .dsi_bus = mipi_dsi_bus,
            .dpi_config = &dpi_config,
        },
    };
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = EXAMPLE_PIN_NUM_LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = EXAMPLE_LCD_BIT_PER_PIXEL,
        .vendor_config = &vendor_config,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st77922(mipi_dbi_io, &panel_config, &panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));
```
