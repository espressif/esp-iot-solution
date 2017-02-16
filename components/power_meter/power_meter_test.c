#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "power_meter.h"
#include "esp_log.h"

#define PM_CF_IO_NUM    25
#define PM_CFI_IO_NUM   26

#define PM_CF_PCNT_UNIT     PCNT_UNIT_0
#define PM_CFI_PCNT_UNIT    PCNT_UNIT_1

#define PM_POWER_PARAM  1293699
#define PM_VOLTAGE_PARAM    102961
#define PM_CURRENT_PARAM    13670

static const char* TAG = "powermeter_test";

static void read_value(void* arg)
{
    pm_handle_t pm_handle = arg;
    while (1) {
        powermeter_change_mode(pm_handle, PM_SINGLE_VOLTAGE);
        vTaskDelay(5000 / portTICK_RATE_MS);
        ESP_LOGI(TAG, "value of power:%d", powermeter_read(pm_handle, PM_POWER));
        ESP_LOGI(TAG, "value of voltage:%d", powermeter_read(pm_handle, PM_VOLTAGE));
        ESP_LOGI(TAG, "value of current:%d", powermeter_read(pm_handle, PM_CURRENT));
        powermeter_change_mode(pm_handle, PM_SINGLE_CURRENT);
        vTaskDelay(5000 / portTICK_RATE_MS);
        ESP_LOGI(TAG, "value of power:%d", powermeter_read(pm_handle, PM_POWER));
        ESP_LOGI(TAG, "value of voltage:%d", powermeter_read(pm_handle, PM_VOLTAGE));
        ESP_LOGI(TAG, "value of current:%d", powermeter_read(pm_handle, PM_CURRENT));
    }
    vTaskDelete(NULL);
}

void power_meter_test()
{
    pm_config_t pm_conf = {
        .power_io_num = PM_CF_IO_NUM,
        .power_pcnt_unit = PCNT_UNIT_0,
        .power_ref_param = PM_POWER_PARAM,
        .voltage_io_num = PM_CFI_IO_NUM,
        .voltage_pcnt_unit = PCNT_UNIT_1,
        .voltage_ref_param = PM_VOLTAGE_PARAM,
        .current_io_num = PM_CFI_IO_NUM,
        .current_pcnt_unit = PCNT_UNIT_1,
        .current_ref_param = PM_CURRENT_PARAM,
        .sel_io_num = 17,
        .sel_level = 0,
        .pm_mode = PM_SINGLE_VOLTAGE
    };
    pm_handle_t pm_handle = powermeter_create(pm_conf);
    xTaskCreate(read_value, "read_value", 2048, pm_handle, 5, NULL);
}
