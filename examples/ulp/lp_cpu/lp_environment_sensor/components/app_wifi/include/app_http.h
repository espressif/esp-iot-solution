/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_HTTP_OUTPUT_BUFFER      2048

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
 * @brief Retrieve weather data via HTTP request using the provided URL
 *
 * This function sends an HTTP request to the specified URL to retrieve weather data and stores the
 * data in the provided `weather_data` structure.
 *
 * @param[in] weather_data Pointer to an `http_weather_data_t` structure where the retrieved weather data will be stored
 *
 * @return
 * - ESP_OK: Successfully retrieved and stored weather data
 * - ESP_FAIL on error
 */
esp_err_t get_http_weather_data_with_url(http_weather_data_t *weather_data);

#ifdef __cplusplus
}
#endif
