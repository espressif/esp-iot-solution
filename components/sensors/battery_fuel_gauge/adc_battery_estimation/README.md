## Battery capacity estimation based on ADC

`adc_battery_estimation` is a lithium battery capacity estimation component based on ADC, which converts battery voltage data collected by ADC into corresponding battery capacity according to the OCV-SOC model, and ensures the consistency of battery capacity data in both discharge and charge states. This component has the following features:

1. Provides basic battery level information while ensuring consistency in the estimated capacity
2. Supports both user-provided external ADC Handle or automatic creation by the component internally
3. Supports filtering of collected ADC data and estimated battery capacity
4. Provides a software-based charging state estimation method. If the user cannot provide a charging indicator pin and `BATTERY_STATE_SOFTWARE_ESTIMATION` is enabled in Kconfig, software charging state estimation will be activated

This component provides two OCV-SOC models, from [Ti](https://www.ti.com/lit/SLUAAR3) and [Analog Device](https://www.analog.com/en/resources/design-notes/characterizing-a-lithiumion-li-cell-for-use-with-an-opencircuitvoltage-ocv-based-fuel-gauge.html) respectively. Additionally, it supports user-defined custom OCV-SOC models.

![OCV-SOC](https://dl.espressif.com/AE/esp-iot-solution/OCV_SOC.png)

## Add component to your project

Please use the component manager command `add-dependency` to add the `adc_battery_estimation` to your project's dependency, during the `CMake` step the component will be downloaded automatically

```
idf.py add-dependency "espressif/adc_battery_estimation=*"
```

Alternatively, you can create `idf_component.yml`. More is in [Espressif's documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/tools/idf-component-manager.html).

## Example use

```c
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "adc_battery_estimation.h"

#define TEST_ADC_UNIT (ADC_UNIT_1)
#define TEST_ADC_BITWIDTH (ADC_BITWIDTH_DEFAULT)
#define TEST_ADC_ATTEN (ADC_ATTEN_DB_12)
#define TEST_ADC_CHANNEL (ADC_CHANNEL_1)
#define TEST_RESISTOR_UPPER (460)
#define TEST_RESISTOR_LOWER (460)
#define TEST_ESTIMATION_TIME (50)

void app_main(void)
{
    adc_battery_estimation_t config = {
        .internal = {
            .adc_unit = TEST_ADC_UNIT,
            .adc_bitwidth = TEST_ADC_BITWIDTH,
            .adc_atten = TEST_ADC_ATTEN,
        },
        .adc_channel = TEST_ADC_CHANNEL,
        .lower_resistor = TEST_RESISTOR_LOWER,
        .upper_resistor = TEST_RESISTOR_UPPER,
    };

    adc_battery_estimation_handle_t adc_battery_estimation_handle = adc_battery_estimation_create(&config);

    for (int i = 0; i < TEST_ESTIMATION_TIME; i++) {
        float capacity = 0;
        adc_battery_estimation_get_capacity(adc_battery_estimation_handle, &capacity);
        printf("Battery capacity: %.1f%%\n", capacity);
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    adc_battery_estimation_destroy(adc_battery_estimation_handle);
}
```
