#pragma once

#include <driver/adc.h>
#include <esp_err.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Simple ADC read wrapper for ESP-IDF
static inline int espidf_analogRead(int pin, int bit_width)
{
    adc1_channel_t channel;
    // Map pin to ADC1 channel (example for GPIO36-39,32-35 on ESP32)
    switch(pin) {
        case 36: channel = ADC1_CHANNEL_0; break;
        case 37: channel = ADC1_CHANNEL_1; break;
        case 38: channel = ADC1_CHANNEL_2; break;
        case 39: channel = ADC1_CHANNEL_3; break;
        case 32: channel = ADC1_CHANNEL_4; break;
        case 33: channel = ADC1_CHANNEL_5; break;
        case 34: channel = ADC1_CHANNEL_6; break;
        case 35: channel = ADC1_CHANNEL_7; break;
        default: return -1; // unsupported pin
    }
    adc1_config_width(bit_width == 12 ? ADC_WIDTH_BIT_12 : (bit_width == 11 ? ADC_WIDTH_BIT_11 : (bit_width == 10 ? ADC_WIDTH_BIT_10 : ADC_WIDTH_BIT_9)));
    adc1_config_channel_atten(channel, ADC_ATTEN_DB_11); // 0-3.6V
    return adc1_get_raw(channel);
}

#ifdef __cplusplus
}
#endif
