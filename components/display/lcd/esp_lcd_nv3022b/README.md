# ESP LCD NV3022B

[![Component Registry](https://components.espressif.com/components/espressif/esp_lcd_nv3022b/badge.svg)](https://components.espressif.com/components/espressif/esp_lcd_nv3022b)

Implementation of the NV3022B LCD controller with esp_lcd component.

| LCD controller | Communication interface | Component name | Link to datasheet |
| :------------: | :---------------------: | :------------: | :---------------: |
| NV3022B        | SPI                     | esp_lcd_nv3022b     | [Specification](https://dl.espressif.com/AE/esp-iot-solution/NV3022B-Datasheet-V10.pdf) |

## Add to project

Packages from this repository are uploaded to [Espressif's component service](https://components.espressif.com/).
You can add them to your project via `idf.py add-dependancy`, e.g.
```
    idf.py add-dependency "espressif/esp_lcd_nv3022b"
```

Alternatively, you can create `idf_component.yml`. More is in [Espressif's documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/tools/idf-component-manager.html).

For more information on LCD, please refer to the [LCD documentation](https://docs.espressif.com/projects/esp-iot-solution/en/latest/display/lcd/index.html).

## Example use

```c
    ESP_LOGI(TAG, "Initialize SPI bus");
    const spi_bus_config_t bus_config = NV3022B_PANEL_BUS_SPI_CONFIG(EXAMPLE_PIN_NUM_LCD_PCLK, EXAMPLE_PIN_NUM_LCD_MOSI,
                                                                     EXAMPLE_LCD_H_RES * 80 * sizeof(uint16_t));
    ESP_ERROR_CHECK(spi_bus_initialize(EXAMPLE_LCD_HOST, &bus_config, SPI_DMA_CH_AUTO));

    ESP_LOGI(TAG, "Install panel IO");
    esp_lcd_panel_io_handle_t io_handle = NULL;
    const esp_lcd_panel_io_spi_config_t io_config = NV3022B_PANEL_IO_SPI_CONFIG(EXAMPLE_PIN_NUM_LCD_CS, EXAMPLE_PIN_NUM_LCD_DC,
                                                                                example_callback, &example_callback_ctx);
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)EXAMPLE_LCD_HOST, &io_config, &io_handle));

/**
 * Uncomment these lines if use custom initialization commands.
 * The array should be declared as "static const" and positioned outside the function.
 */
// static const nv3022b_lcd_init_cmd_t lcd_init_cmds[] = {
// // {cmd, { data }, data_size, delay_ms}
//  {0xFD, (uint8_t []){0x06, 0x08}, 2, 0},
//  {0x61, (uint8_t []){0x07, 0x07}, 2, 0},
//  {0x73, (uint8_t []){0x70}, 1, 0},
//  {0x73, (uint8_t []){0x00}, 1, 0},
//     ...
// };

    ESP_LOGI(TAG, "Install NV3022B panel driver");
    esp_lcd_panel_handle_t panel_handle = NULL;
    // nv3022b_vendor_config_t vendor_config = {  // Uncomment these lines if use custom initialization commands
    //     .init_cmds = lcd_init_cmds,
    //     .init_cmds_size = sizeof(lcd_init_cmds) / sizeof(nv3022b_lcd_init_cmd_t),
    // };
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = EXAMPLE_PIN_NUM_LCD_RST,      // Set to -1 if not use
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,     // Implemented by LCD command `36h`
        .bits_per_pixel = 16,                           // Implemented by LCD command `3Ah` (12/16/18)
        // .vendor_config = &vendor_config,            // Uncomment this line if use custom initialization commands
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_nv3022b(io_handle, &panel_config, &panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
    ESP_ERROR_CHECK(esp_lcd_panel_disp_off(panel_handle, false));
#else
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));
#endif
```
