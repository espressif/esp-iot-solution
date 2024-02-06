# ESP LCD ST77922

[![Component Registry](https://components.espressif.com/components/espressif/esp_lcd_st77922/badge.svg)](https://components.espressif.com/components/espressif/esp_lcd_st77922)

Implementation of the ST77922 LCD controller with [esp_lcd](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/lcd.html) component.

| LCD controller | Communication interface | Component name |                               Link to datasheet                               |
| :------------: | :---------------------: | :------------: | :---------------------------------------------------------------------------: |
|     ST77922     |        SPI/QSPI         | esp_lcd_st77922 | [PDF](https://dl.espressif.com/AE/esp-iot-solution/ST77922_SPEC_V0.1.pdf) |

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
    const spi_bus_config_t bus_config = ST77922_PANEL_BUS_SPI_CONFIG(EXAMPLE_PIN_NUM_LCD_PCLK, EXAMPLE_PIN_NUM_LCD_DATA0, EXAMPLE_LCD_H_RES * 80 * sizeof(uint16_t));
    ESP_ERROR_CHECK(spi_bus_initialize(EXAMPLE_LCD_HOST, &bus_config, SPI_DMA_CH_AUTO));

    ESP_LOGI(TAG, "Install panel IO");
    esp_lcd_panel_io_handle_t io_handle = NULL;
    const esp_lcd_panel_io_spi_config_t io_config = ST77922_PANEL_IO_SPI_CONFIG(EXAMPLE_PIN_NUM_LCD_CS, EXAMPLE_PIN_NUM_LCD_DC,
                                                                               callback, &callback_data);
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)EXAMPLE_LCD_HOST, &io_config, &io_handle));

/**
 * Uncomment these line if use custom initialization commands.
 * The array should be declared as static const and positioned outside the function.
 */
// static const ST77922_lcd_init_cmd_t lcd_init_cmds[] = {
// //  {cmd, { data }, data_size, delay_ms}
// {0x28, (uint8_t []){0x00}, 0, 0},
// {0x10, (uint8_t []){0x00}, 0, 0},
// {0x2A, (uint8_t []){0x00, 0x00, 0x02, 0x13}, 4, 0},
// {0x2B, (uint8_t []){0x00, 0x00, 0x01, 0x2B}, 4, 0},
//     ...
// };

    ESP_LOGI(TAG, "Install ST77922 panel driver");
    esp_lcd_panel_handle_t panel_handle = NULL;
    ST77922_vendor_config_t vendor_config = {
        // .init_cmds = lcd_init_cmds,         // Uncomment these line if use custom initialization commands
        // .init_cmds_size = sizeof(lcd_init_cmds) / sizeof(ST77922_lcd_init_cmd_t),
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
    const esp_lcd_panel_io_spi_config_t io_config = ST77922_PANEL_BUS_QSPI_CONFIG(EXAMPLE_PIN_NUM_LCD_PCLK,
                                                                                 EXAMPLE_PIN_NUM_LCD_DATA0,
                                                                                 EXAMPLE_PIN_NUM_LCD_DATA1,
                                                                                 EXAMPLE_PIN_NUM_LCD_DATA2,
                                                                                 EXAMPLE_PIN_NUM_LCD_DATA3
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
// static const ST77922_lcd_init_cmd_t lcd_init_cmds[] = {
// //  {cmd, { data }, data_size, delay_ms}
// {0x28, (uint8_t []){0x00}, 0, 0},
// {0x10, (uint8_t []){0x00}, 0, 0},
// {0x2A, (uint8_t []){0x00, 0x00, 0x02, 0x13}, 4, 0},
// {0x2B, (uint8_t []){0x00, 0x00, 0x01, 0x2B}, 4, 0},
//     ...
// };

    ESP_LOGI(TAG, "Install ST77922 panel driver");
    esp_lcd_panel_handle_t panel_handle = NULL;
    ST77922_vendor_config_t vendor_config = {
        // .init_cmds = lcd_init_cmds,         // Uncomment these line if use custom initialization commands
        // .init_cmds_size = sizeof(lcd_init_cmds) / sizeof(ST77922_lcd_init_cmd_t),
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
