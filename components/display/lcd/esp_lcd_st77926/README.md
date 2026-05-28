# ESP LCD ST77926

[![Component Registry](https://components.espressif.com/components/espressif/esp_lcd_st77926/badge.svg)](https://components.espressif.com/components/espressif/esp_lcd_st77926)

Implementation of the ST77926 LCD controller with esp_lcd component.

| LCD controller | Communication interface | Component name  | Link to datasheet |
| :------------: | :---------------------: | :-------------: | :---------------: |
|    ST77926     |          QSPI           | esp_lcd_st77926 | [PDF](https://dl.espressif.com/AE/esp-iot-solution/ST77926_SPEC_V1.0.pdf) |

**Note**: The QSPI write path requires the flush area `x_start` and width to be 4-pixel aligned.

For more information on LCD, please refer to the [LCD documentation](https://docs.espressif.com/projects/esp-iot-solution/en/latest/display/lcd/index.html).

## Add to project

Packages from this repository are uploaded to [Espressif's component service](https://components.espressif.com/).
You can add them to your project via `idf.py add-dependency`, e.g.

```
    idf.py add-dependency "espressif/esp_lcd_st77926"
```

## Example use

```c
    spi_bus_config_t buscfg = ST77926_PANEL_BUS_QSPI_CONFIG(EXAMPLE_PIN_NUM_LCD_PCLK,
                                                            EXAMPLE_PIN_NUM_LCD_DATA0,
                                                            EXAMPLE_PIN_NUM_LCD_DATA1,
                                                            EXAMPLE_PIN_NUM_LCD_DATA2,
                                                            EXAMPLE_PIN_NUM_LCD_DATA3,
                                                            EXAMPLE_LCD_H_RES * 80 * EXAMPLE_LCD_BIT_PER_PIXEL / 8);
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));

    esp_lcd_panel_io_spi_config_t io_config = ST77926_PANEL_IO_QSPI_CONFIG(EXAMPLE_PIN_NUM_LCD_CS, NULL, NULL);
    esp_lcd_panel_io_handle_t io_handle = NULL;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST, &io_config, &io_handle));

    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = EXAMPLE_PIN_NUM_LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = EXAMPLE_LCD_BIT_PER_PIXEL,
    };
    esp_lcd_panel_handle_t panel_handle = NULL;
    ESP_ERROR_CHECK(esp_lcd_new_panel_st77926(io_handle, &panel_config, &panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));
```
