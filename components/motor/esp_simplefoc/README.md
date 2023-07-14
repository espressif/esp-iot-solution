[![Component Registry](https://components.espressif.com/components/espressif/esp_simplefoc/badge.svg)](https://components.espressif.com/components/espressif/esp_simplefoc)

## ESP32-IDF SimpleFOC Component
 
Features:

1. Support voltage control.
2. Support Host computer control.
3. Support multiple motor control, up to 4, support manual selection of the drive mode or by the system.
4. Compatible with SimpleFOC examples.
5. Use IQMath to greatly accelerate Foc operations.


### Usage Considerations
1. The configTICK_RATE_HZ of Freertos must be set to 1000. If it is not set, a motor exception may occur.

### License
esp_simplefoc is licensed under the Apache License, see license.txt for more information.