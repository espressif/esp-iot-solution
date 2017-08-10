#include "smart_config.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "unity.h"

#define SMART_CONFIG_TEST_EN 1
#if SMART_CONFIG_TEST_EN
void sc_check_status(void* arg)
{
    while(1) {
        printf("sc status: %d\n", sc_get_status());
        vTaskDelay(300/portTICK_PERIOD_MS);
        if(sc_get_status() == SC_STATUS_LINK_OVER) {
            break;
        }
    }
    vTaskDelete(NULL);
}

void sc_test()
{
    esp_err_t res;
    sc_setup(SC_TYPE_ESPTOUCH, WIFI_MODE_STA, 0);
    xTaskCreate(sc_check_status, "sc_check_status", 1024*2, NULL, 5, NULL);
    while (1) {
        res = sc_start(20000 / portTICK_PERIOD_MS);
        if (res == ESP_OK) {
            printf("connected\n");
            break;
        } else if (res == ESP_ERR_TIMEOUT) {
            printf("smart config timeout\n");
        } else if (res == ESP_FAIL) {
            printf("smart config stopped\n");
        }
    }

}

TEST_CASE("ESPTOUCH test", "[esptouch][iot]")
{
    sc_test();
}

#endif
