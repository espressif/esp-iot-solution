# ESP LCD Touch ST7123 Controller

[![Component Registry](https://components.espressif.com/components/espressif/esp_lcd_touch_st7123/badge.svg)](https://components.espressif.com/components/espressif/esp_lcd_touch_st7123)

Implementation of the ST7123 touch controller with [esp_lcd_touch](https://github.com/espressif/esp-bsp/tree/master/components/lcd_touch/esp_lcd_touch) component.

| Touch controller | Communication interface |    Component name     |                                Link to datasheet                                |
| :--------------: | :---------------------: | :-------------------: | :-----------------------------------------------------------------------------: |
|     ST7123      |           I2C           | esp_lcd_touch_st7123 | [PDF](https://dl.espressif.com/AE/esp-iot-solution/ST7123+TDDI+Interface+Protocol+V01.11.pdf) |

## Add to project

Packages from this repository are uploaded to [Espressif's component service](https://components.espressif.com/).
You can add them to your project via `idf.py add-dependancy`, e.g.
```
    idf.py add-dependency "espressif/esp_lcd_touch_st7123"
```

Alternatively, you can create `idf_component.yml`. More is in [Espressif's documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/tools/idf-component-manager.html).

## Example use

I2C initialization of the touch component.

```
    esp_lcd_panel_io_i2c_config_t io_config = ESP_LCD_TOUCH_IO_I2C_ST7123_CONFIG();

    esp_lcd_touch_config_t tp_cfg = {
        .x_max = CONFIG_LCD_H_RES,
        .y_max = CONFIG_LCD_V_RES,
        .rst_gpio_num = -1,
        .int_gpio_num = -1,
        .levels = {
            .reset = 0,
            .interrupt = 0,
        },
        .flags = {
            .swap_xy = 0,
            .mirror_x = 0,
            .mirror_y = 0,
        },
    };

    esp_lcd_touch_handle_t tp;
    esp_lcd_touch_new_i2c_st7123(io_handle, &tp_cfg, &tp);
```

Read data from the touch controller and store it in RAM memory. It should be called regularly in poll.

```
    esp_lcd_touch_read_data(tp);
```

Get one X and Y coordinates with strength of touch.

```
    uint16_t touch_x[1];
    uint16_t touch_y[1];
    uint16_t touch_strength[1];
    uint8_t touch_cnt = 0;

    esp_lcd_touch_point_data_t points[1] = {0};
    esp_lcd_touch_get_data(tp, points, &touch_cnt, 1);
    if (touch_cnt > 0) {
        ESP_LOGI(TAG, "Touch position: [%d, %d], strength %d, count %d", points[0].x, points[0].y, points[0].strength, touch_cnt);
    }
```
