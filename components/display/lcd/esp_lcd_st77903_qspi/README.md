# ESP LCD ST77903 (QSPI)

Implementation of the ST77903 QSPI LCD controller with [esp_lcd](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/lcd.html) component.

| LCD controller | Communication interface |    Component name    |                                                                            Link to datasheet                                                                            |
| :------------: | :---------------------: | :------------------: | :---------------------------------------------------------------------------------------------------------------------------------------------------------------------: |
|    ST77903     |          QSPI           | esp_lcd_ST77903_qspi | [PDF1](https://dl.espressif.com/AE/esp-iot-solution/ST77903_SPEC_P0.5.pdf), [PDF2](https://dl.espressif.com/AE/esp-iot-solution/ST77903_Customer_Application_Notes.pdf) |

For more information on LCD, please refer to the [LCD documentation](https://docs.espressif.com/projects/esp-iot-solution/en/latest/display/lcd/index.html).

## Initialization Code

```c
/**
 * Uncomment these line if use custom initialization commands.
 * The array should be declared as static const and positioned outside the function.
 */
// static const st77903_lcd_init_cmd_t lcd_init_cmds[] = {
// //  {cmd, { data }, data_size, delay_ms}
//    {0xf0, (uint8_t []){0xc3}, 1, 0},
//    {0xf0, (uint8_t []){0x96}, 1, 0},
//    {0xf0, (uint8_t []){0xa5}, 1, 0},
//     ...
// };

    ESP_LOGI(TAG, "Install st77903 panel driver");
    esp_lcd_panel_handle_t panel_handle = NULL;
    st77903_qspi_config_t qspi_config = ST77903_QSPI_CONFIG_DEFAULT(EXAMPLE_LCD_HOST,
                                                                    EXAMPLE_PIN_NUM_LCD_QSPI_CS,
                                                                    EXAMPLE_PIN_NUM_LCD_QSPI_PCLK,
                                                                    EXAMPLE_PIN_NUM_LCD_QSPI_DATA0,
                                                                    EXAMPLE_PIN_NUM_LCD_QSPI_DATA1,
                                                                    EXAMPLE_PIN_NUM_LCD_QSPI_DATA2,
                                                                    EXAMPLE_PIN_NUM_LCD_QSPI_DATA3,
                                                                    1,
                                                                    EXAMPLE_LCD_QSPI_H_RES,
                                                                    EXAMPLE_LCD_QSPI_V_RES);
    st77903_vendor_config_t vendor_config = {
        .qspi_config = &qspi_config,
        // .init_cmds = lcd_init_cmds,         // Uncomment these line if use custom initialization commands
        // .init_cmds_size = sizeof(lcd_init_cmds) / sizeof(st77903_lcd_init_cmd_t),
        .flags = {
            .mirror_by_cmd = 1,     // Implemented by LCD command `36h`
        },
    };
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = EXAMPLE_PIN_NUM_LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,     // Implemented by LCD command `36h`
        .bits_per_pixel = EXAMPLE_LCD_BIT_PER_PIXEL,    // Implemented by LCD command `3Ah` (16/18/24)
        .vendor_config = &vendor_config,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st77903(&panel_config, &panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_handle, true, false));   // This function can only be called when the refresh task is not running
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));     // This function can control the display on/off and the refresh task run/stop
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));                  // Start the refresh task
```
