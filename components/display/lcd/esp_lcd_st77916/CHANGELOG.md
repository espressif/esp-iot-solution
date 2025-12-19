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
* Add MIPI-DSI interface support

## v1.0.1 - 2025-01-13

### bugfix:

* Fix issues of QSPI interface

## v1.0.0 - 2024-08-12

### Enhancements:

* Component version maintenance, code improvement, and documentation enhancement

## v0.0.1 - 2023-12-01

### Enhancements:

* Implement the driver for the ST77916 LCD controller
* Support SPI and QSPI interface

## v0.0.2 - 2023-12-15

### Enhancements:

* Fix issues of QSPI interface
