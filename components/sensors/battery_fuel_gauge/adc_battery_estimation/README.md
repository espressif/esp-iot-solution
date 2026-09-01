## Battery capacity estimation based on ADC

`adc_battery_estimation` is a lithium battery capacity estimation component based on ADC, which converts battery voltage data collected by ADC into corresponding battery capacity according to the OCV-SOC model, and ensures the consistency of battery capacity data in both discharge and charge states. This component has the following features:

1. Provides basic battery level information while ensuring consistency in the estimated capacity
2. Supports both user-provided external ADC Handle or automatic creation by the component internally
3. Supports filtering of collected ADC data and estimated battery capacity, and exposes the filtered battery voltage
4. Provides a software-based charging state estimation method. If the user cannot provide a charging indicator pin and `BATTERY_STATE_SOFTWARE_ESTIMATION` is enabled in Kconfig, software charging state estimation will be activated
5. Supports a standby indicator pin, so that a battery which is full but still connected to the charger keeps reporting 100%

This component provides two OCV-SOC models, from [Ti](https://www.ti.com/lit/SLUAAR3) and [Analog Device](https://www.analog.com/en/resources/design-notes/characterizing-a-lithiumion-li-cell-for-use-with-an-opencircuitvoltage-ocv-based-fuel-gauge.html) respectively. Additionally, it supports user-defined custom OCV-SOC models.

![OCV-SOC](https://dl.espressif.com/AE/esp-iot-solution/OCV_SOC.png)

## Add component to your project

Please use the component manager command `add-dependency` to add the `adc_battery_estimation` to your project's dependency, during the `CMake` step the component will be downloaded automatically

```
idf.py add-dependency "espressif/adc_battery_estimation=*"
```

Alternatively, you can create `idf_component.yml`. More is in [Espressif's documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/tools/idf-component-manager.html).

## Charging and standby state

The component cannot tell from the battery voltage alone whether the battery is being charged, because the charge current raises the terminal voltage well above the open-circuit voltage the OCV-SOC model expects. It relies on the status pins of the charger IC instead, reported through two optional callbacks.

`charging_detect_cb` reports an ongoing charge, and is typically wired to the `CHRG` pin of a charger such as the TP4056. `standby_detect_cb` reports that the charge has been terminated because the battery is full, while the charger is still connected, and is typically wired to the `STDBY` pin of the same charger. Both pins are open drain and active low, so they need a pull-up.

Standby takes priority over charging. While it is asserted the capacity is reported as 100% and the ADC is not sampled at all, which is what keeps a docked device from slowly counting down as the terminal voltage relaxes from 4.2 V to the open-circuit voltage of a full cell. `adc_battery_estimation_get_charging_state()` reports `false` in this state, since the charge really has stopped, use `adc_battery_estimation_get_standby_state()` to tell standby apart from a real discharge.

Make sure the pin driving `standby_detect_cb` really means "charge complete". Some chargers also assert their status pins when no battery is connected or when the input supply is removed, which would be reported as a full battery.

If no charging indicator pin is available at all, enable `BATTERY_STATE_SOFTWARE_ESTIMATION` in Kconfig to let the component guess the charging state from the trend of the sampled voltage. There is no software equivalent for standby.

## Example use

```c
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "adc_battery_estimation.h"

#define TEST_ADC_UNIT (ADC_UNIT_1)
#define TEST_ADC_BITWIDTH (ADC_BITWIDTH_DEFAULT)
#define TEST_ADC_ATTEN (ADC_ATTEN_DB_12)
#define TEST_ADC_CHANNEL (ADC_CHANNEL_1)
#define TEST_CHARGE_GPIO_NUM (GPIO_NUM_0)
#define TEST_STANDBY_GPIO_NUM (GPIO_NUM_4)
#define TEST_RESISTOR_UPPER (460)
#define TEST_RESISTOR_LOWER (460)
#define TEST_ESTIMATION_TIME (50)

/* Both pins are open drain and active low */
static bool battery_charging_detect(void *user_data)
{
    return gpio_get_level(TEST_CHARGE_GPIO_NUM) == 0;
}

static bool battery_standby_detect(void *user_data)
{
    return gpio_get_level(TEST_STANDBY_GPIO_NUM) == 0;
}

void app_main(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << TEST_CHARGE_GPIO_NUM) | (1ULL << TEST_STANDBY_GPIO_NUM),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    adc_battery_estimation_t config = {
        .internal = {
            .adc_unit = TEST_ADC_UNIT,
            .adc_bitwidth = TEST_ADC_BITWIDTH,
            .adc_atten = TEST_ADC_ATTEN,
        },
        .adc_channel = TEST_ADC_CHANNEL,
        .lower_resistor = TEST_RESISTOR_LOWER,
        .upper_resistor = TEST_RESISTOR_UPPER,
        .charging_detect_cb = battery_charging_detect,
        .standby_detect_cb = battery_standby_detect,
    };

    adc_battery_estimation_handle_t adc_battery_estimation_handle = adc_battery_estimation_create(&config);

    for (int i = 0; i < TEST_ESTIMATION_TIME; i++) {
        float capacity = 0;
        float voltage = 0;
        bool is_charging = false;
        bool is_standby = false;

        adc_battery_estimation_get_capacity(adc_battery_estimation_handle, &capacity);
        adc_battery_estimation_get_voltage(adc_battery_estimation_handle, &voltage);
        adc_battery_estimation_get_charging_state(adc_battery_estimation_handle, &is_charging);
        adc_battery_estimation_get_standby_state(adc_battery_estimation_handle, &is_standby);

        printf("Battery capacity: %.1f%%, voltage: %.3fV, state: %s\n", capacity, voltage,
               is_standby ? "Standby" : (is_charging ? "Charging" : "Discharging"));
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    adc_battery_estimation_destroy(adc_battery_estimation_handle);
}
```
