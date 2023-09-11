# ChangeLog

## v1.0.0 - 2023-06-20

### Enhancements:

* esp_lcd_panel_io_3wire_spi:
    * Support bit-banging the 3-wire SPI LCD protocol (without D/C and MISO lines)
    * Support for GPIO and IO expander
    * Support for 8-bit/16-bit/24-bit/32-bit data write (without D/C bit)
    * Support for 9-bit/17-bit/25-bit/33-bit data write (including D/C bit)

## v1.0.1 - 2023-09-11

### Bug Fixes:

* esp_lcd_panel_io_3wire_spi:
    * fix test_apps build error