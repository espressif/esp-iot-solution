**Don't Need Patch in This version, but some IDF files need to be modified**

## Setup `release/v4.4` ESP-IDF

* Please Install latest `release/v4.4` branch [ESP-IDF](https://github.com/espressif/esp-idf/tree/release/v4.4), you can refer [ESP-IDF(release/v4.4) Installation Step by Step](https://docs.espressif.com/projects/esp-idf/en/release-v4.4/esp32s3/get-started/index.html#installation-step-by-step)


## Modify some IDF Files

1. **Fix ESP32-S2 PRE-ECO Chip Bug**, add `ets_delay_us(10)` before `spi_hal_user_start(hal)` in function `spi_new_trans` of file `esp-idf/components/driver/spi_master.c`:

   ```c
   ets_delay_us(10);
   //Kick off transfer
   spi_hal_user_start(hal);
   ```