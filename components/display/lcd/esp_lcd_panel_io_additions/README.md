[![Component Registry](https://components.espressif.com/components/espressif/esp_lcd_panel_io_additions/badge.svg)](https://components.espressif.com/components/espressif/esp_lcd_panel_io_additions)

# Component: ESP_LCD_PANEL_IO_ADDITIONS

This component supplements the [esp_lcd](https://github.com/espressif/esp-idf/blob/master/components/esp_lcd/include/esp_lcd_panel_io.h) component in ESP-IDF and offers additional functionality through `esp_lcd_panel_io_*()`. It provides the following functions:

* **esp_lcd_new_panel_io_3wire_spi()**: This function utilizes GPIO or IO expander to perform bit-banging for the 3-wire SPI interface (without D/C and MISO lines). It is specifically designed for the [3 Wire SPI + Parallel RGB Interface](https://focuslcds.com/3-wire-spi-parallel-rgb-interface-fan4213/).

## Add to project

Please use the component manager command `idf.py add-dependency` to add the `esp_lcd_panel_io_additions` to your project's dependency, during the `CMake` step the component will be downloaded automatically.

```
    idf.py add-dependency esp_lcd_panel_io_additions==1.0.0
```

Alternatively, you can create `idf_component.yml`. More is in [Espressif's documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/tools/idf-component-manager.html).

## Example use

### ESP_LCD_PANEL_IO_3WIRE_SPI

Initialization of the panel IO.

```
    spi_line_config_t spi_line = {
        .cs_io_type = IO_TYPE_EXPANDER,
        .cs_gpio_num = IO_EXPANDER_PIN_NUM_1,
        .scl_io_type = IO_TYPE_GPIO,
        .scl_gpio_num = GPIO_NUM_9,
        .sda_io_type = IO_TYPE_GPIO,
        .sda_gpio_num = GPIO_NUM_10,
        .io_expander = io_expander,     // Created by the user
    };
    esp_lcd_panel_io_3wire_spi_config_t io_config = {
        .line_config = spi_line,
        .expect_clk_speed = PANEL_IO_3WIRE_SPI_CLK_MAX,
        .spi_mode = 0,
        .lcd_cmd_bytes = 1,
        .lcd_param_bytes = 1,
        .flags = {
            .use_dc_bit = 1,
            .dc_zero_on_data = 0,
            .lsb_first = 0,
            .cs_high_active = 0,
            .del_keep_cs_inactive = 0,
        },
    };
    esp_lcd_panel_io_handle_t panel_io = NULL;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_3wire_spi(&io_config, &panel_io));
```

Write LCD command and parameters through the panel IO.

```
    esp_lcd_panel_io_tx_param(panel_io, lcd_cmd, lcd_param, bytes_of_lcd_param);
```

Here is an example of using it to initialize the RGB LCD panel.

```
typedef struct {
    uint8_t cmd;
    uint8_t data[12];
    uint8_t data_bytes; // Length of data in above data array; 0xFF = end of cmds.
} lcd_init_cmd_t;

static const lcd_init_cmd_t vendor_specific_init[] = {
    ...
    {0x62, {0x38, 0x0D, 0x71, 0xED, 0x70, 0x70, 0x38, 0x0F, 0x71, 0xEF, 0x70, 0x70}, 12},
    {0x63, {0x38, 0x11, 0x71, 0xF1, 0x70, 0x70, 0x38, 0x13, 0x71, 0xF3, 0x70, 0x70}, 12},
    ...
    {0, {0}, 0xff},
};

static void panel_init(esp_lcd_panel_io_handle_t panel_io)
{
    // vendor specific initialization, it can be different between manufacturers
    // should consult the LCD supplier for initialization sequence code
    int i = 0;
    while (vendor_specific_init[i].data_bytes != 0xff) {
        ESP_ERROR_CHECK(esp_lcd_panel_io_tx_param(panel_io, vendor_specific_init[i].cmd, vendor_specific_init[i].data,
                                                  vendor_specific_init[i].data_bytes));
        i++;
    }
}
```
