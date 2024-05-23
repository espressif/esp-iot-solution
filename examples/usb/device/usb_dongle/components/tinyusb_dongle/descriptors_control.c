/*
 * SPDX-FileCopyrightText: 2020-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "esp_log.h"
#include "esp_check.h"
#include "esp_err.h"
#include "descriptors_control.h"
#include "usb_descriptors.h"

#define MAX_DESC_BUF_SIZE 32               // Max length of string descriptor (can be extended, USB supports lengths up to 255 bytes)

static const char *TAG = "tusb_desc";

// =============================================================================
// STRUCTS
// =============================================================================

/**
 * @brief Descriptor pointers for tinyusb descriptor requests callbacks
 *
 */
typedef struct {
    const tusb_desc_device_t *dev;      /*!< Pointer to device descriptor */
    union {
        const uint8_t *cfg;             /*!< Pointer to FullSpeed configuration descriptor when device one-speed only */
        const uint8_t *fs_cfg;          /*!< Pointer to FullSpeed configuration descriptor when device support HighSpeed */
    };
#if (TUD_OPT_HIGH_SPEED)
    const uint8_t *hs_cfg;              /*!< Pointer to HighSpeed configuration descriptor */
    const tusb_desc_device_qualifier_t *qualifier;            /*!< Pointer to Qualifier descriptor */
    uint8_t *other_speed;               /*!< Pointer for other speed configuration descriptor */
#endif // TUD_OPT_HIGH_SPEED
    const char *str[USB_STRING_DESCRIPTOR_ARRAY_SIZE];  /*!< Pointer to array of UTF-8 strings */
    int str_count;                      /*!< Number of descriptors in str */
} tinyusb_descriptor_config_t;

static tinyusb_descriptor_config_t s_desc_cfg;

// =============================================================================
// CALLBACKS
// =============================================================================

/**
 * @brief Invoked when received GET DEVICE DESCRIPTOR.
 * Descriptor contents must exist long enough for transfer to complete
 *
 * @return Pointer to device descriptor
 */
uint8_t const *tud_descriptor_device_cb(void)
{
    assert(s_desc_cfg.dev);
    return (uint8_t const *)s_desc_cfg.dev;
}

/**
 * @brief Invoked when received GET CONFIGURATION DESCRIPTOR.
 * Descriptor contents must exist long enough for transfer to complete
 *
 * @param[in] index Index of required configuration
 * @return Pointer to configuration descriptor
 */
uint8_t const *tud_descriptor_configuration_cb(uint8_t index)
{
    (void)index; // Unused, this driver supports only 1 configuration
    assert(s_desc_cfg.cfg);

#if (TUD_OPT_HIGH_SPEED)
    // HINT: cfg and fs_cfg are union, no need to assert(fs_cfg)
    assert(s_desc_cfg.hs_cfg);
    // Return configuration descriptor based on Host speed
    return (TUSB_SPEED_HIGH == tud_speed_get())
           ? s_desc_cfg.hs_cfg
           : s_desc_cfg.fs_cfg;
#else
    return s_desc_cfg.cfg;
#endif // TUD_OPT_HIGH_SPEED
}

#if (TUD_OPT_HIGH_SPEED)
/**
 * @brief Invoked when received GET DEVICE QUALIFIER DESCRIPTOR request
 * Descriptor contents must exist long enough for transfer to complete
 * If not highspeed capable stall this request
 */
uint8_t const *tud_descriptor_device_qualifier_cb(void)
{
    assert(s_desc_cfg.qualifier);
    return (uint8_t const *)s_desc_cfg.qualifier;
}

/**
 * @brief Invoked when received GET OTHER SPEED CONFIGURATION DESCRIPTOR request
 * Descriptor contents must exist long enough for transfer to complete
 * Configuration descriptor in the other speed e.g if high speed then this is for full speed and vice versa
 */
uint8_t const *tud_descriptor_other_speed_configuration_cb(uint8_t index)
{
    assert(s_desc_cfg.other_speed);

    const uint8_t *other_speed = (TUSB_SPEED_HIGH == tud_speed_get())
                                 ? s_desc_cfg.fs_cfg
                                 : s_desc_cfg.hs_cfg;

    memcpy(s_desc_cfg.other_speed,
           other_speed,
           ((tusb_desc_configuration_t *)other_speed)->wTotalLength);

    ((tusb_desc_configuration_t *)s_desc_cfg.other_speed)->bDescriptorType = TUSB_DESC_OTHER_SPEED_CONFIG;
    return s_desc_cfg.other_speed;
}
#endif // TUD_OPT_HIGH_SPEED

/**
 * @brief Invoked when received GET STRING DESCRIPTOR request
 *
 * @param[in] index   Index of required descriptor
 * @param[in] langid  Language of the descriptor
 * @return Pointer to UTF-16 string descriptor
 */
uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid)
{
    (void) langid; // Unused, this driver supports only one language in string descriptors
    assert(s_desc_cfg.str);
    uint8_t chr_count;
    static uint16_t _desc_str[MAX_DESC_BUF_SIZE];

    if (index == 0) {
        memcpy(&_desc_str[1], s_desc_cfg.str[0], 2);
        chr_count = 1;
    } else {
        if (index >= USB_STRING_DESCRIPTOR_ARRAY_SIZE) {
            ESP_LOGW(TAG, "String index (%u) is out of bounds, check your string descriptor", index);
            return NULL;
        }

        if (s_desc_cfg.str[index] == NULL) {
            ESP_LOGW(TAG, "String index (%u) points to NULL, check your string descriptor", index);
            return NULL;
        }

        const char *str = s_desc_cfg.str[index];
        chr_count = strnlen(str, MAX_DESC_BUF_SIZE - 1); // Buffer len - header

        // Convert ASCII string into UTF-16
        for (uint8_t i = 0; i < chr_count; i++) {
            _desc_str[1 + i] = str[i];
        }
    }

    // First byte is length in bytes (including header), second byte is descriptor type (TUSB_DESC_STRING)
    _desc_str[0] = (TUSB_DESC_STRING << 8) | (2 * chr_count + 2);

    return _desc_str;
}

// =============================================================================
// Driver functions
// =============================================================================
esp_err_t tinyusb_set_descriptors(const tinyusb_config_t *config)
{
    esp_err_t ret = ESP_FAIL;
    assert(config);
    const char **pstr_desc;
    // Flush descriptors control struct
    memset(&s_desc_cfg, 0x00, sizeof(tinyusb_descriptor_config_t));
    // Parse configuration and save descriptors's pointer
    // Select Device Descriptor
    if (config->device_descriptor == NULL) {
        ESP_LOGW(TAG, "No Device descriptor provided, using default.");
        s_desc_cfg.dev = &descriptor_dev_default;
    } else {
        s_desc_cfg.dev = config->device_descriptor;
    }

    // Select FullSpeed configuration descriptor
    if (config->configuration_descriptor == NULL) {
        ESP_LOGW(TAG, "No FullSpeed configuration descriptor provided, using default.");
        s_desc_cfg.cfg = descriptor_fs_cfg_default;
    } else {
        s_desc_cfg.cfg = config->configuration_descriptor;
    }

#if (TUD_OPT_HIGH_SPEED)
    // High Speed
    if (config->hs_configuration_descriptor == NULL) {
        ESP_LOGW(TAG, "No HighSpeed configuration descriptor provided, using default.");
        s_desc_cfg.hs_cfg = descriptor_hs_cfg_default;
    } else {
        s_desc_cfg.hs_cfg = config->hs_configuration_descriptor;
    }

    // HS and FS cfg desc should be equal length
    ESP_GOTO_ON_FALSE(((tusb_desc_configuration_t *)s_desc_cfg.hs_cfg)->wTotalLength ==
                      ((tusb_desc_configuration_t *)s_desc_cfg.fs_cfg)->wTotalLength,
                      ESP_ERR_INVALID_ARG, fail, TAG, "HighSpeed and FullSpeed configuration descriptors must be same length");

    // Qualifier Descriptor
    if (config->qualifier_descriptor == NULL) {
        ESP_GOTO_ON_FALSE((s_desc_cfg.dev == &descriptor_dev_default), ESP_ERR_INVALID_ARG, fail, TAG, "Qualifier descriptor must be present (Device Descriptor not default).");
        // Get default qualifier if device descriptor is default
        ESP_LOGW(TAG, "No Qulifier descriptor provided, using default.");
        s_desc_cfg.qualifier = &descriptor_qualifier_default;
    } else {
        s_desc_cfg.qualifier = config->qualifier_descriptor;
    }

    // Other Speed buffer allocate
    s_desc_cfg.other_speed = calloc(1, ((tusb_desc_configuration_t *)s_desc_cfg.hs_cfg)->wTotalLength);
    ESP_GOTO_ON_FALSE(s_desc_cfg.other_speed, ESP_ERR_NO_MEM, fail, TAG, "Other speed memory allocation error");
#endif // TUD_OPT_HIGH_SPEED

    // Select String Descriptors and count them
    if (config->string_descriptor == NULL) {
        ESP_LOGW(TAG, "No String descriptors provided, using default.");
        pstr_desc = descriptor_str_default;
        while (descriptor_str_default[++s_desc_cfg.str_count] != NULL);
    } else {
        pstr_desc = config->string_descriptor;
        s_desc_cfg.str_count = (config->string_descriptor_count != 0)
                               ? config->string_descriptor_count
                               : 8; // '8' is for backward compatibility with esp_tinyusb v1.0.0. Do NOT remove!
    }

    ESP_GOTO_ON_FALSE(s_desc_cfg.str_count <= USB_STRING_DESCRIPTOR_ARRAY_SIZE, ESP_ERR_NOT_SUPPORTED, fail, TAG, "String descriptors exceed limit");
    memcpy(s_desc_cfg.str, pstr_desc, s_desc_cfg.str_count * sizeof(pstr_desc[0]));

    ESP_LOGI(TAG, "\n"
             "┌─────────────────────────────────┐\n"
             "│  USB Device Descriptor Summary  │\n"
             "├───────────────────┬─────────────┤\n"
             "│bDeviceClass       │ %-4u        │\n"
             "├───────────────────┼─────────────┤\n"
             "│bDeviceSubClass    │ %-4u        │\n"
             "├───────────────────┼─────────────┤\n"
             "│bDeviceProtocol    │ %-4u        │\n"
             "├───────────────────┼─────────────┤\n"
             "│bMaxPacketSize0    │ %-4u        │\n"
             "├───────────────────┼─────────────┤\n"
             "│idVendor           │ %-#10x  │\n"
             "├───────────────────┼─────────────┤\n"
             "│idProduct          │ %-#10x  │\n"
             "├───────────────────┼─────────────┤\n"
             "│bcdDevice          │ %-#10x  │\n"
             "├───────────────────┼─────────────┤\n"
             "│iManufacturer      │ %-#10x  │\n"
             "├───────────────────┼─────────────┤\n"
             "│iProduct           │ %-#10x  │\n"
             "├───────────────────┼─────────────┤\n"
             "│iSerialNumber      │ %-#10x  │\n"
             "├───────────────────┼─────────────┤\n"
             "│bNumConfigurations │ %-#10x  │\n"
             "└───────────────────┴─────────────┘",
             s_desc_cfg.dev->bDeviceClass, s_desc_cfg.dev->bDeviceSubClass,
             s_desc_cfg.dev->bDeviceProtocol, s_desc_cfg.dev->bMaxPacketSize0,
             s_desc_cfg.dev->idVendor, s_desc_cfg.dev->idProduct, s_desc_cfg.dev->bcdDevice,
             s_desc_cfg.dev->iManufacturer, s_desc_cfg.dev->iProduct, s_desc_cfg.dev->iSerialNumber,
             s_desc_cfg.dev->bNumConfigurations);

    return ESP_OK;

fail:
#if (TUD_OPT_HIGH_SPEED)
    free(s_desc_cfg.other_speed);
#endif // TUD_OPT_HIGH_SPEED
    return ret;
}

void tinyusb_set_str_descriptor(const char *str, int str_idx)
{
    assert(str_idx < USB_STRING_DESCRIPTOR_ARRAY_SIZE);
    s_desc_cfg.str[str_idx] = str;
}

void tinyusb_free_descriptors(void)
{
#if (TUD_OPT_HIGH_SPEED)
    assert(s_desc_cfg.other_speed);
    free(s_desc_cfg.other_speed);
#endif // TUD_OPT_HIGH_SPEED
}
