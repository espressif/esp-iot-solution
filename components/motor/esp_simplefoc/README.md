[![Component Registry](https://components.espressif.com/components/espressif/esp_simplefoc/badge.svg)](https://components.espressif.com/components/espressif/esp_simplefoc)

## ESP-IDF SimpleFOC Component
 
Features:

1. Support voltage control.
2. Support Host computer control.
3. Support multiple motor control, up to 4, support manual selection of the drive mode or by the system.
4. Compatible with SimpleFOC examples.
5. Use IQMath to greatly accelerate Foc operations.


### Add component to your project

Please use the component manager command `add-dependency` to add the `esp_simplefoc` to your project's dependency, during the `CMake` step the component will be downloaded automatically

```
idf.py add-dependency "espressif/esp_simplefoc=*"
```

Alternatively, you can create `idf_component.yml`. More is in [Espressif's documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/tools/idf-component-manager.html).

### User guide

Please refer to：https://docs.espressif.com/projects/esp-iot-solution/zh_CN/latest/motor/foc/esp_simplefoc.html


### Usage Considerations

1. The configTICK_RATE_HZ of Freertos must be set to 1000. If it is not set, a motor exception may occur.

### License

esp_simplefoc is licensed under the Apache License, see license.txt for more information.