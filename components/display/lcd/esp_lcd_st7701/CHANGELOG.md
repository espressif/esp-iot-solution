# ChangeLog

## v1.1.4 - 2025-09-15

### Changes:

* Updated phy_clk_src default value from MIPI_DSI_PHY_CLK_SRC_DEFAULT to 0

## v1.1.3 - 2025-6-23

### bugfix：

*  Fixed the log TAG issue

## v1.1.2 - 2025-04-03

### bugfix:

* Fixed the issue where the ST7701 RGB interface mirror and swap were ineffective

## v1.1.1 - 2024-11-10

### bugfix:

* Modified the order of reading the ID register

## v1.1.0 - 2024-05-06

### Enhancements:

* Support MIPI-DSI interface

## v1.0.1 - 2024-03-25

### bugfix

* Fix the `mirror` function

## v1.0.0 - 2023-11-03

### bugfix

* Fix the incompatible dependent version of ESP-IDF
* Fix missing dependency of `cmake_utilities` component
* Check conflicting commands between initialization sequence and driver

## v0.0.2 - 2023-10-26

### Enhancements:

* Support to mirror by command

## v0.0.1 - 2023-09-12

### Enhancements:

* Implement the driver for the ST7701(S) LCD controller
* Support RGB interface
