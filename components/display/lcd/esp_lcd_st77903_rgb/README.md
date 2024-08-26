# ESP LCD ST77903 RGB

Implementation of the ST77903 RGB LCD controller with [esp_lcd](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/lcd.html) component.

| LCD controller | Communication interface | Component name |                                                                            Link to datasheet                                                                             |
| :------------: | :---------------------: | :------------: | :----------------------------------------------------------------------------------------------------------------------------------------------------------------------: |
|     ST77903     |        RGB         | esp_lcd_ST77903_rgb | [PDF1](https://dl.espressif.com/AE/esp-iot-solution/ST77903_SPEC_P0.5.pdf)|

For more information on LCD, please refer to the [LCD documentation](https://docs.espressif.com/projects/esp-iot-solution/en/latest/display/lcd/index.html).

## Initialization Code

For most RGB LCDs, they typically use a "3-Wire SPI + Parallel RGB" interface. The "3-Wire SPI" interface is used for transmitting command data and the "Parallel RGB" interface is used for sending pixel data.

It's recommended to use the [esp_lcd_panel_io_additions](https://components.espressif.com/components/espressif/esp_lcd_panel_io_additions) component to bit-bang the "3-Wire SPI" interface through **GPIO** or an **IO expander** (like [TCA9554](https://components.espressif.com/components/espressif/esp_io_expander_tca9554)). To do this, please first add this component to your project manually. Then, refer to the following code to initialize the ST77903 controller.

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

    ESP_LOGI(TAG, "Install 3-wire SPI panel IO");
    spi_line_config_t line_config = {
        .cs_io_type = IO_TYPE_GPIO,     // Set to `IO_TYPE_EXPANDER` if using GPIO, same to below
        .cs_expander_pin = EXAMPLE_LCD_IO_SPI_CS,
        .scl_io_type = IO_TYPE_GPIO,
        .scl_gpio_num = EXAMPLE_LCD_IO_SPI_SCK,
        .sda_io_type = IO_TYPE_GPIO,
        .sda_gpio_num = EXAMPLE_LCD_IO_SPI_SDO,
        .io_expander = NULL,     // Set handle if using IO expander
    };
    esp_lcd_panel_io_3wire_spi_config_t io_config = ST77903_RGB_PANEL_IO_3WIRE_SPI_CONFIG(line_config, 0);
    esp_lcd_panel_io_handle_t io_handle = NULL;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_3wire_spi(&io_config, &io_handle));

    ESP_LOGI(TAG, "Install st77903 panel driver");
    esp_lcd_panel_handle_t panel_handle = NULL;
    const esp_lcd_rgb_panel_config_t rgb_config = {
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
        .timings = ST77903_RGB_320_480_PANEL_48HZ_RGB_TIMING(),
        .flags.fb_in_psram = 1,
    };
    st77903_vendor_config_t vendor_config = {
        .rgb_config = &rgb_config,
        // .init_cmds = lcd_init_cmds,         // Uncomment these line if use custom initialization commands
        // .init_cmds_size = sizeof(lcd_init_cmds) / sizeof(st77903_lcd_init_cmd_t),
        .flags = {
            .mirror_by_cmd = 1,             // Only work when `enable_io_multiplex` is set to 0
            .enable_io_multiplex = 0,       /**
                                             * Send initialization commands and delete the panel IO instance during creation if set to 1.
                                             * This flag is only used when `use_rgb_interface` is set to 1.
                                             * If the panel IO pins are sharing other pins of the RGB interface to save GPIOs,
                                             * Please set it to 1 to release the panel IO and its pins (except CS signal).
                                             */
        },
    };
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = EXAMPLE_PIN_NUM_LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,     // Implemented by LCD command `36h`
        .bits_per_pixel = EXAMPLE_LCD_BIT_PER_PIXEL,    // Implemented by LCD command `3Ah` (16/18/24)
        .vendor_config = &vendor_config,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st77903(io_handle, &panel_config, &panel_handle));    /**
                                                                                             * Only create RGB when `enable_io_multiplex` is set to 0,
                                                                                             * or initialize ST77903 meanwhile
                                                                                             */
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));     // Only reset RGB when `enable_io_multiplex` is set to 1, or reset ST77903 meanwhile
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));      // Only initialize RGB when `enable_io_multiplex` is set to 1, or initialize ST77903 meanwhile
```
