/* esp32 deep_sleep test
 *
 *  We do this test case using ESP32_DeepSleep_Demo_Board_V1 board.
 *  You can refer to the README.md document in the upper director.
 *
*/

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_spi_flash.h"
#include "esp_sleep.h"
#include "esp32/ulp.h"
#include "driver/adc.h"
#include "soc/sens_reg.h"
#include "driver/rtc_io.h"
#include "driver/gpio.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/rtc.h"
#include "time.h"
#include "driver/touch_pad.h"
#include "unity.h"
#include "iot_touchpad.h"

static const char *TAG = "ut_deep_sleep";
#define TEST_LOG(msg) ESP_LOGI(TAG, "\n\n>>>   %s   <<<\n\n",msg)

//set rtc timer as wakeup source
static void timer_wake_init(void)
{
    const int time_wakeup_sec = 10;
    esp_sleep_enable_timer_wakeup(time_wakeup_sec * 1000000);
}

//set touch_pad 7 as wakeup source
static void touch_pad_wakeup_init(void)
{
    iot_tp_create(TOUCH_PAD_NUM7, 0.20);
    touch_pad_set_meas_time(0xffff, 0xffff);
    esp_sleep_enable_touchpad_wakeup();
}

//set wakeup by ext1
static void ext1_wakeup_init(void)
{
    const uint64_t pin_mask = (1ULL << 39) | (1ULL << 36) | (1ULL << 35) | (1ULL << 34);
    esp_sleep_enable_ext1_wakeup(pin_mask, ESP_EXT1_WAKEUP_ALL_LOW);
}

//set wakeup by ext0
static void ext0_wakeup_init(void)
{
    const uint32_t pin_num = 39;
    esp_sleep_enable_ext0_wakeup(pin_num, 0);
}

//get what waked the system up.
static void get_wakup_cause(void)
{
    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    switch (cause) {
        case ESP_SLEEP_WAKEUP_EXT0:
            printf("\nwaked up by EXT0\n");
            break;
        case ESP_SLEEP_WAKEUP_EXT1:
            printf("\nwaked up by EXT1\n");
            break;
        case ESP_SLEEP_WAKEUP_TIMER:
            printf("\nwaked up by TIMER\n");
            break;
        case ESP_SLEEP_WAKEUP_TOUCHPAD:
            printf("\nwaked up by TOUCHPAD\n");
            break;
        default:
            printf("\nfirst power on\n");
            break;
    }
}

//start deep_sleep 3 seconds later.
static void deep_sleep_start(void)
{
    int time_count = 3;
    printf("System entering deep_sleep mode 3 seconds later\n");
    while (time_count > 0) {
        printf("time remain %d s\n", time_count);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        time_count--;
    }
    TEST_LOG("entering deep_sleep mode");
    esp_deep_sleep_start();
}

TEST_CASE("Deep_sleep get wake_up cause test", "[deep_sleep][iot]")
{
    TEST_LOG("wake_up cause test, please do deep_sleep test first");
    get_wakup_cause();
}

TEST_CASE("Deep_sleep EXT0 wakeup test", "[deep_sleep][iot]")
{
    TEST_LOG("ext0 wake_up test");
    printf("During deep_sleep, press key 's_vn' and give a low level signal to wake_up system\n");
    ext0_wakeup_init();
    deep_sleep_start();
}

TEST_CASE("Deep_sleep EXT1 wakeup test", "[deep_sleep][iot]")
{
    TEST_LOG("ext1 wake_up test");
    printf("During deep_sleep, set key 's_VP' 's_VN' 'IO_34' 'IO_35' all low to wake_up system\n");
    ext1_wakeup_init();
    deep_sleep_start();
}

TEST_CASE("Deep_sleep touch_pad wakeup test", "[deep_sleep][iot]")
{
    TEST_LOG("touch_pad wake_up test");
    printf("During deep_sleep, touch 'TP3' pad to wake_up system\n");
    touch_pad_wakeup_init();
    deep_sleep_start();
}

TEST_CASE("Deep_sleep time wakeup test", "[deep_sleep][iot]")
{
    TEST_LOG("timer wake_up test");
    printf("After entering deep_sleep mode, the System will be awakend 10 seconds later.\n");
    timer_wake_init();
    deep_sleep_start();
}
