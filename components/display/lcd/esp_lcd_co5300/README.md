# ESP LCD CO5300

[![Component Registry](https://components.espressif.com/components/espressif/esp_lcd_co5300/badge.svg)](https://components.espressif.com/components/espressif/esp_lcd_co5300)

Implementation of the CO5300 LCD controller with esp_lcd component.

| LCD controller | Communication interface | Component name |                              Link to datasheet                               |
| :------------: | :---------------------: | :------------: | :--------------------------------------------------------------------------: |
|    CO5300     |    SPI/ QSPI / MIPI-DSI     | esp_lcd_co5300 | [PDF](https://dl.espressif.com/AE/esp-iot-solution/CO5300_Datasheet_V0.00.pdf) |

**Note**: MIPI-DSI interface only supports ESP-IDF v5.3 and above versions.

For more information on LCD, please refer to the [LCD documentation](https://docs.espressif.com/projects/esp-iot-solution/en/latest/display/lcd/index.html).

## Add to project

Packages from this repository are uploaded to [Espressif's component service](https://components.espressif.com/).
You can add them to your project via `idf.py add-dependancy`, e.g.

```
    idf.py add-dependency "espressif/esp_lcd_co5300"
```

Alternatively, you can create `idf_component.yml`. More is in [Espressif's documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/tools/idf-component-manager.html).

## Example use

### SPI Interface

```c
    ESP_LOGI(TAG, "Initialize SPI bus");
    const spi_bus_config_t bus_config = CO5300_PANEL_BUS_SPI_CONFIG(EXAMPLE_PIN_NUM_LCD_PCLK, EXAMPLE_PIN_NUM_LCD_MOSI,
                                                                     EXAMPLE_LCD_H_RES * 80 * sizeof(uint16_t));
    ESP_ERROR_CHECK(spi_bus_initialize(EXAMPLE_LCD_HOST, &bus_config, SPI_DMA_CH_AUTO));

    ESP_LOGI(TAG, "Install panel IO");
    esp_lcd_panel_io_handle_t io_handle = NULL;
    const esp_lcd_panel_io_spi_config_t io_config = CO5300_PANEL_IO_SPI_CONFIG(EXAMPLE_PIN_NUM_LCD_CS, EXAMPLE_PIN_NUM_LCD_DC,
                                                                                example_callback, &example_callback_ctx);
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)EXAMPLE_LCD_HOST, &io_config, &io_handle));

/**
 * Uncomment these lines if use custom initialization commands.
 * The array should be declared as "static const" and positioned outside the function.
 */
// static const co5300_lcd_init_cmd_t lcd_init_cmds[] = {
// // {cmd, { data }, data_size, delay_ms}
//  {0xFD, (uint8_t []){0x06, 0x08}, 2, 0},
//  {0x61, (uint8_t []){0x07, 0x07}, 2, 0},
//  {0x73, (uint8_t []){0x70}, 1, 0},
//  {0x73, (uint8_t []){0x00}, 1, 0},
//     ...
// };

    ESP_LOGI(TAG, "Install CO5300 panel driver");
    esp_lcd_panel_handle_t panel_handle = NULL;
    // co5300_vendor_config_t vendor_config = {  // Uncomment these lines if use custom initialization commands
    //     .init_cmds = lcd_init_cmds,
    //     .init_cmds_size = sizeof(lcd_init_cmds) / sizeof(co5300_lcd_init_cmd_t),
    // };
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = EXAMPLE_PIN_NUM_LCD_RST,      // Set to -1 if not use
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,     // Implemented by LCD command `36h`
        .bits_per_pixel = 16,                           // Implemented by LCD command `3Ah` (12/16/18)
        // .vendor_config = &vendor_config,            // Uncomment this line if use custom initialization commands
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_co5300(io_handle, &panel_config, &panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
    ESP_ERROR_CHECK(esp_lcd_panel_disp_off(panel_handle, false));
#else
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));
#endif
```

### QSPI Interface

```c
    ESP_LOGI(TAG, "Initialize QSPI bus");
    const spi_bus_config_t buscfg = CO5300_PANEL_BUS_QSPI_CONFIG(EXAMPLE_PIN_NUM_LCD_PCLK,
                                                                                 EXAMPLE_PIN_NUM_LCD_DATA0,
                                                                                 EXAMPLE_PIN_NUM_LCD_DATA1,
                                                                                 EXAMPLE_PIN_NUM_LCD_DATA2,
                                                                                 EXAMPLE_PIN_NUM_LCD_DATA3,
                                                                                 EXAMPLE_LCD_H_RES * 80 * sizeof(uint16_t));
    ESP_ERROR_CHECK(spi_bus_initialize(EXAMPLE_LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

    ESP_LOGI(TAG, "Install panel IO");
    esp_lcd_panel_io_handle_t io_handle = NULL;
    const esp_lcd_panel_io_spi_config_t io_config = CO5300_PANEL_IO_QSPI_CONFIG(EXAMPLE_PIN_NUM_LCD_CS, callback, &callback_data);
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)EXAMPLE_LCD_HOST, &io_config, &io_handle));

/**
 * Uncomment these line if use custom initialization commands.
 * The array should be declared as static const and positioned outside the function.
 */
// static const co5300_lcd_init_cmd_t lcd_init_cmds[] = {
// //  {cmd, { data }, data_size, delay_ms}
//    {0x44, (uint8_t []){0x00, 0xc8}, 2, 0},
//    {0x35, (uint8_t []){0x00}, 0, 0},
//    {0x53, (uint8_t []){0x20}, 1, 25},
//    {0x29, (uint8_t []){0x00}, 0, 120},
//     ...
// };

    ESP_LOGI(TAG, "Install CO5300 panel driver");
    esp_lcd_panel_handle_t panel_handle = NULL;
    const co5300_vendor_config_t vendor_config = {
        // .init_cmds = lcd_init_cmds,         // Uncomment these line if use custom initialization commands
        // .init_cmds_size = sizeof(lcd_init_cmds) / sizeof(co5300_lcd_init_cmd_t),
        .flags = {
            .use_qspi_interface = 1,
        },
    };
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = EXAMPLE_PIN_NUM_LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,               // Implemented by LCD command `36h`
        .bits_per_pixel = 16,                                     // Implemented by LCD command `3Ah`
        .vendor_config = &vendor_config,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_co5300(io_handle, &panel_config, &panel_handle));

    esp_lcd_panel_reset(panel_handle);
    esp_lcd_panel_init(panel_handle);
    esp_lcd_panel_disp_on_off(panel_handle, true);
```

### MIPI Interface

```c
/**
 * Uncomment these line if use custom initialization commands.
 * The array should be declared as static const and positioned outside the function.
 */
// static const co5300_lcd_init_cmd_t lcd_init_cmds[] = {
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
    esp_lcd_dsi_bus_config_t bus_config = CO5300_PANEL_BUS_DSI_1CH_CONFIG();
    ESP_ERROR_CHECK(esp_lcd_new_dsi_bus(&bus_config, &mipi_dsi_bus));

    ESP_LOGI(TAG, "Install panel IO");
    esp_lcd_dbi_io_config_t dbi_config = CO5300_PANEL_IO_DBI_CONFIG();
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_dbi(mipi_dsi_bus, &dbi_config, &mipi_dbi_io));

    ESP_LOGI(TAG, "Install LCD driver of co5300");
    esp_lcd_panel_handle_t panel_handle = NULL;
    esp_lcd_dpi_panel_config_t dpi_config = CO5300_466_466_PANEL_60HZ_DPI_CONFIG(EXAMPLE_MIPI_DPI_PX_FORMAT);
    co5300_vendor_config_t vendor_config = {
        // .init_cmds = lcd_init_cmds,      // Uncomment these line if use custom initialization commands
        // .init_cmds_size = sizeof(lcd_init_cmds) / sizeof(co5300_lcd_init_cmd_t),
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
    ESP_ERROR_CHECK(esp_lcd_new_panel_co5300(mipi_dbi_io, &panel_config, &panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
```
