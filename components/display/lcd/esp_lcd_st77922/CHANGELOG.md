# ChangeLog

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

## v1.0.4 - 2025-09-15

### Changes:

* Updated phy_clk_src default value from MIPI_DSI_PHY_CLK_SRC_DEFAULT to 0

## v1.0.3 - 2025-6-23

### bugfix：

*  Fixed the log TAG issue

## v1.0.2 - 2024-11-10

### bugfix:

* Increased the default Lane bit rate value

## v1.0.1 - 2024-11-7

* Fix default initialization parameters and front-back window parameters.

## v1.0.0 - 2024-8-7

* Support MIPI-DSI interface

## v0.1.0 - 2024-5-15

### Enhancements:

* Support RGB interface

## v0.0.3 - 2024-3-27

### Enhancements:

### bugfix

* Fix the logic when checking conflicting commands between initialization sequence and driver
* Fix the wrong command structure in the README.md

## v0.0.2 - 2024-2-19

### Enhancements:

* Add notes of X coordinate limitation in README.md

## v0.0.1 - 2024-1-22

### Enhancements:

* Implement the driver for the ST77922 LCD controller
* Support SPI and QSPI interface
