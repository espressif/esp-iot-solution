/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif
#define MAX_HTTP_OUTPUT_BUFFER      (16 * 1024)


typedef struct {
    char district_id[20];     // Unique identifier for the district
    char city_name[10];       // City name for weather information retrieval
    char district_name[10];   // District name for weather information retrieval
    char text[10];            // Weather description (e.g., sunny, cloudy)
    char wind_class[10];      // Classification of wind speed
    char wind_dir[15];        // Wind direction (e.g., north, south)
    char uptime[15];          // Timestamp of the last weather data update
    char week[10];            // Day of the week (e.g., Monday, Tuesday)
    int temp_high;            // High temperature of the day in Celsius
    int temp_low;             // Low temperature of the day in Celsius
} http_weather_data_t;

/**
 * @brief Retrieve weather data for the configured district.
 *
 * The response is bounded by MAX_HTTP_OUTPUT_BUFFER. Malformed JSON,
 * oversized fields, non-success HTTP responses, and transport failures
 * return an error without modifying the destination structure.
 *
 * @param[in,out] weather_data Destination for a successful response.
 *
 * @return ESP_OK on success, or an error code describing the failure.
 */
esp_err_t get_http_weather_data_with_url(http_weather_data_t *weather_data);

#ifdef __cplusplus
}
#endif
