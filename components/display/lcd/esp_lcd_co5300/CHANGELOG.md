# ChangeLog

## v2.1.0 - 2026-06-29

### Enhancements:

* Added rotation support for the CO5300 driver through `esp_lcd_panel_mirror()` and `esp_lcd_panel_swap_xy()` on SPI and QSPI paths
* Added SPI/QSPI rotation test coverage
* Bumped component version to 2.1.0

## v2.0.3 - 2025-12-15

### Changes:

* Start from esp-idf v6.0, DMA2D can only be enable by calling `esp_lcd_dpi_panel_enable_dma2d`

## v2.0.2 - 2025-12-10

### Bugfix:

* Fix draw_bitmap not propagating tx_color errors, preventing system deadlock on SPI transmission failures

## v2.0.1 - 2025-11-12

* Updated MIPI-DSI structs for IDF6

## v2.0.0 - 2025-10-29

### Changes:

* Compatible with ESP-IDF v6.0
* Added brightness adjustment interface

## v1.0.1 - 2025-09-15

### Changes:

* Updated phy_clk_src default value from MIPI_DSI_PHY_CLK_SRC_DEFAULT to 0

## v1.0.0 - 2025-07-16

### Enhancements:

* Implement the driver for the CO5300 LCD controller
* Support SPI/QSPI interface
* Support RGB interface
