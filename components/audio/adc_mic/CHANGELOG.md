# ChangeLog

## v0.2.1 - 2025-08-08

### Fixes:

* Fixed a crash when calling `esp_codec_dev_open` and `esp_codec_dev_close` across tasks.

### Note:

* This version creates an internal task, whose priority and task size can be adjusted via `menuconfig`. Correspondingly, the memory overhead will increase by the configured amount.

## v0.2.0 - 2024-05-22

### Enhancements:

* Add config ADC_MIC_APPLY_GAIN to apply gain to the ADC mic.

## v0.1.0 - 2024-05-22

### Enhancements:

* Initial version.
