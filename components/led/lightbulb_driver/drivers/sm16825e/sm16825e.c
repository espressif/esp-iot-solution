/*
 * SPDX-FileCopyrightText: 2022-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-LICENSE-Identifier: Apache-2.0
 */

#include <string.h>

#include <esp_log.h>
#include <driver/spi_master.h>
#include <hal/spi_hal.h>
#include <driver/gpio.h>
#include "esp_heap_caps.h"
#include "esp_idf_version.h"
#include "driver_utils.h"
#include "sm16825e.h"

static const char *TAG = "sm16825e";

// RZ (Return-to-Zero) coding: 800Kbps rate with 1200ns code period
#define SM16825E_BIT_PERIOD_NS    1200           // 1200ns per bit (minimum code period from datasheet)

// Timing parameters from datasheet (in ns) - these are now actually used
#define SM16825E_T0H_MIN          200             // Minimum value of 0-bit high level time
#define SM16825E_T0H_TYP          300             // Typical value of 0-bit high-level time
#define SM16825E_T0H_MAX          400             // Maximum value of the 0-bit high level time
#define SM16825E_T0L_MIN          800             // Minimum value of 0-code low-level time
#define SM16825E_T1H_MIN          800             // Minimum value of 1-bit high level time
#define SM16825E_T1H_TYP          900             // Typical value of 1-bit high-level time
#define SM16825E_T1H_MAX          1000            // Maximum value of 1-bit high-level time
#define SM16825E_T1L_MIN          200             // Minimum value of 1-bit low-level time
#define SM16825E_RESET_MIN        200          // Minimum value of reset-code low-level time (200μs)

// Use typical timing values for actual implementation
#define SM16825E_T0H_ACTUAL       SM16825E_T0H_TYP    // Use typical value: 300ns
#define SM16825E_T0L_ACTUAL       (SM16825E_BIT_PERIOD_NS - SM16825E_T0H_ACTUAL)  // 1200-300=900ns
#define SM16825E_T1H_ACTUAL       SM16825E_T1H_TYP    // Use typical value: 900ns
#define SM16825E_T1L_ACTUAL       (SM16825E_BIT_PERIOD_NS - SM16825E_T1H_ACTUAL)  // 1200-900=300ns

// Calculate SPI speed based on required timing resolution
// Need at least 4 SPI bits per SM16825E bit for adequate timing resolution
// SPI speed = 4 * (1000000000ns / 1200ns) = 4 * 833333 = 3.33MHz
#define SM16825E_SPIBITS_PER_BIT  4
#define SM16825E_SPI_SPEED_HZ     (SM16825E_SPIBITS_PER_BIT * 1000000000UL / SM16825E_BIT_PERIOD_NS)  // 3.33MHz
#define SM16825E_SPI_BIT_NS       (SM16825E_BIT_PERIOD_NS / SM16825E_SPIBITS_PER_BIT)  // 300ns per SPI bit
#define SM16825E_LED_BUF          (SM16825E_TOTAL_BITS * SM16825E_SPIBITS_PER_BIT / 8)  // 112*4/8 = 56 bytes per chip

// Calculate SPI bit patterns based on actual timing parameters
// T0: T0H=300ns (1 SPI bit) + T0L=900ns (3 SPI bits) = 1200ns → pattern: 1000
// T1: T1H=900ns (3 SPI bits) + T1L=300ns (1 SPI bit) = 1200ns → pattern: 1110
#define SM16825E_T0H_SPI_BITS     (SM16825E_T0H_ACTUAL / SM16825E_SPI_BIT_NS)  // 300/300 = 1 bit
#define SM16825E_T0L_SPI_BITS     (SM16825E_T0L_ACTUAL / SM16825E_SPI_BIT_NS)  // 900/300 = 3 bits
#define SM16825E_T1H_SPI_BITS     (SM16825E_T1H_ACTUAL / SM16825E_SPI_BIT_NS)  // 900/300 = 3 bits
#define SM16825E_T1L_SPI_BITS     (SM16825E_T1L_ACTUAL / SM16825E_SPI_BIT_NS)  // 300/300 = 1 bit

// Generate bit patterns based on calculated timing
// For T0: 1 high bit + 3 low bits = 1000 (binary) = 0x08
// For T1: 3 high bits + 1 low bit = 1110 (binary) = 0x0E
#define SM16825E_BIT_0_PATTERN    ((0x0F << (SM16825E_SPIBITS_PER_BIT - SM16825E_T0H_SPI_BITS)) & 0x0F)  // 1000 = 0x08
#define SM16825E_BIT_1_PATTERN    ((0x0F << (SM16825E_SPIBITS_PER_BIT - SM16825E_T1H_SPI_BITS)) & 0x0F)  // 1110 = 0x0E

// Data frame structure
#define SM16825E_GRAY_BITS        16              // 16 bits per channel
#define SM16825E_CHANNELS         5               // RGBWY 5 channels
#define SM16825E_CONFIG_BITS      32              // 32 bits configuration data
#define SM16825E_TOTAL_BITS       112             // 80 + 32 = 112 bits total

// Invalid address constant for mapping
#define INVALID_ADDR        0xFF

typedef struct {
    spi_device_handle_t spi_handle;
    uint16_t led_num;
    uint8_t *buf;
    uint32_t buf_size;
    uint8_t current_gain[5];   // Current gain for RGBWY channels
    bool standby_enabled;      // Standby mode flag
    uint8_t mapping_addr[5];   // Channel to pin mapping
} sm16825e_handle_t;

static sm16825e_handle_t *s_sm16825e = NULL;

static esp_err_t _write(uint8_t *buf, size_t size)
{
    spi_transaction_t t = { 0 };
    esp_err_t ret;

    t.length = size * 8;
    t.tx_buffer = buf;
    t.rx_buffer = NULL;

    ret = spi_device_transmit(s_sm16825e->spi_handle, &t);

    return ret;
}

/**
 * @brief Encode a single bit using SM16825E RZ protocol timing
 *
 * @param bit_value 0 or 1 bit value to encode
 * @param buf Buffer to store encoded data
 * @param bit_index Bit index in the buffer (each bit uses 4 SPI bits)
 *
 * @note SM16825E RZ encoding per datasheet (1200ns code period):
 *       - '0' bit: T0H=300ns (typical), T0L=900ns → SPI pattern: 1000 (1 high, 3 low)
 *       - '1' bit: T1H=900ns (typical), T1L=300ns → SPI pattern: 1110 (3 high, 1 low)
 *       - SPI speed calculated as: 4 × (1000000000ns ÷ 1200ns) = 3.33MHz
 *       - Each SPI bit = 1200ns ÷ 4 = 300ns, 4 SPI bits = 1200ns total
 *       - 800Kbps effective rate with 1200ns minimum code period
 *       - Timing values now dynamically calculated from datasheet parameters
 */
static esp_err_t encode_bit(uint8_t bit_value, uint8_t *buf, uint32_t bit_index)
{
    uint32_t spi_bit_start = bit_index * SM16825E_SPIBITS_PER_BIT;  // Each SM16825E bit uses 4 SPI bits
    uint8_t pattern = bit_value ? SM16825E_BIT_1_PATTERN : SM16825E_BIT_0_PATTERN;

    // Write the 4-bit pattern into the buffer
    for (int i = 0; i < SM16825E_SPIBITS_PER_BIT; i++) {
        uint32_t spi_bit_pos = spi_bit_start + i;
        uint32_t byte_pos = spi_bit_pos / 8;
        uint32_t bit_pos = 7 - (spi_bit_pos % 8);

        if (byte_pos >= SM16825E_LED_BUF) {
            ESP_LOGE(TAG, "Buffer overflow!");
            return ESP_FAIL;
        }

        if (pattern & (1 << (SM16825E_SPIBITS_PER_BIT - 1 - i))) {  // Check bit from MSB
            buf[byte_pos] |= (1 << bit_pos);
        }
    }
    return ESP_OK;
}

/**
 * @brief Generate 16-bit gray data for a channel
 *
 * @param value 16-bit input value (0-65535)
 * @param buf Buffer to store encoded data
 * @param start_bit_index Starting bit index (each bit uses 4 SPI bits)
 *
 * @note SM16825E supports 65536 levels (16 bits), we map 8-bit to 16-bit
 */
static esp_err_t set_gray_data(uint16_t value, uint8_t *buf, uint32_t start_bit_index)
{
    uint16_t gray_value = value;

    // Encode 16 bits of gray data (MSB first as per datasheet)
    for (int i = 0; i < 16; i++) {
        uint8_t bit = (gray_value >> (15 - i)) & 0x01;
        esp_err_t err = encode_bit(bit, buf, start_bit_index + i);
        if (err != ESP_OK) {
            return err;
        }
    }
    return ESP_OK;
}

/**
 * @brief Generate configuration data for SM16825E
 *
 * @param buf Buffer to store configuration data
 * @param start_bit_index Starting bit index for config data (each bit uses 4 SPI bits)
 *
 * @note Configuration includes:
 *       - 5 bits current gain for each channel (RGBWY) = 25 bits
 *       - 2 bits standby enable (2b'10 for standby) = 2 bits
 *       - 5 bits reserved (recommended all 1s) = 5 bits
 *       Total: 32 bits
 */
static esp_err_t set_config_data(uint8_t *buf, uint32_t start_bit_index)
{
    uint32_t bit_index = start_bit_index;

    for (int ch = 0; ch < 5; ch++) {
        uint8_t gain = s_sm16825e->current_gain[ch];
        for (int i = 0; i < 5; i++) {
            uint8_t bit = (gain >> (4 - i)) & 0x01;
            esp_err_t err = encode_bit(bit, buf, bit_index++);
            if (err != ESP_OK) {
                return err;
            }
        }
    }

    // Set standby enable bits (2 bits)
    if (s_sm16825e->standby_enabled) {
        // 2b'10 for standby mode
        esp_err_t err = encode_bit(1, buf, bit_index++);
        if (err != ESP_OK) {
            return err;
        }
        err = encode_bit(0, buf, bit_index++);
        if (err != ESP_OK) {
            return err;
        }
    } else {
        // 2b'00 for normal mode
        esp_err_t err = encode_bit(0, buf, bit_index++);
        if (err != ESP_OK) {
            return err;
        }
        err = encode_bit(0, buf, bit_index++);
        if (err != ESP_OK) {
            return err;
        }
    }

    // Set reserved bits (5 bits, recommended all 1b's)
    for (int i = 0; i < 5; i++) {
        esp_err_t err = encode_bit(1, buf, bit_index++); // All 1b's
        if (err != ESP_OK) {
            return err;
        }
    }
    return ESP_OK;
}

/**
 * @brief Generate complete data frame for SM16825E
 *
 * @param index LED chip index
 * @param red Red channel value (0-255)
 * @param green Green channel value (0-255)
 * @param blue Blue channel value (0-255)
 * @param cold_white White channel value (0-255)
 * @param warm_yellow Yellow channel value (0-255)
 * @param out_buf Output buffer
 *
 * @note Data frame: 80 bits (5×16 bits gray) + 32 bits (config) = 112 bits total
 *       Each bit encoded as 4 SPI bits → 112 × 4 = 448 SPI bits = 56 bytes
 *       RZ coding: 800Kbps rate, 1200ns code period per datasheet
 *       Frame duration: 112 × 1200ns = 134.4μs per chip
 */
static esp_err_t generate_data(uint32_t index, uint16_t red, uint16_t green, uint16_t blue,
                               uint16_t cold_white, uint16_t warm_yellow, uint8_t *out_buf)
{
    uint8_t *chip_buf = out_buf + (index * SM16825E_LED_BUF);
    memset(chip_buf, 0, SM16825E_LED_BUF);

    uint32_t bit_index = 0;

    // Create array of channel values in logical order (R,G,B,W,Y)
    uint16_t channel_values[5] = {red, green, blue, cold_white, warm_yellow};

    // Set gray data for each channel (16 bits each, MSB first)
    // get mapping address from mapping_addr array, and set gray data to the corresponding physical pin
    for (int logical_ch = 0; logical_ch < 5; logical_ch++) {
        uint8_t physical_pin = s_sm16825e->mapping_addr[logical_ch];
        if (physical_pin != INVALID_ADDR) {
            esp_err_t err = set_gray_data(channel_values[logical_ch], chip_buf, bit_index + (physical_pin * 16));
            if (err != ESP_OK) {
                return err;
            }
        }
    }
    bit_index += 80; // 5 channels * 16 bits each

    // Set configuration data (32 bits)
    return set_config_data(chip_buf, bit_index);
}

static void cleanup(void)
{
    if (s_sm16825e && s_sm16825e->spi_handle) {
        spi_bus_remove_device(s_sm16825e->spi_handle);
        s_sm16825e->spi_handle = NULL;
    }

    if (s_sm16825e && s_sm16825e->buf) {
        free(s_sm16825e->buf);
        s_sm16825e->buf = NULL;
    }

    if (s_sm16825e) {
        free(s_sm16825e);
        s_sm16825e = NULL;
    }
}

esp_err_t sm16825e_init(driver_sm16825e_t *config, void(*hook_func)(void *))
{
    esp_err_t err = ESP_OK;
    DRIVER_CHECK(config, "config is null", return ESP_ERR_INVALID_ARG);
    DRIVER_CHECK(!s_sm16825e, "already init done", return ESP_ERR_INVALID_ARG);

    s_sm16825e = calloc(1, sizeof(sm16825e_handle_t));
    DRIVER_CHECK(s_sm16825e, "alloc fail", return ESP_ERR_NO_MEM);

    s_sm16825e->led_num = config->led_num;
    s_sm16825e->buf_size = s_sm16825e->led_num * SM16825E_LED_BUF;

    // Initialize customer current gains
    sm16825e_set_channel_current(SM16825E_CHANNEL_R, config->current[SM16825E_CHANNEL_R]);
    sm16825e_set_channel_current(SM16825E_CHANNEL_G, config->current[SM16825E_CHANNEL_G]);
    sm16825e_set_channel_current(SM16825E_CHANNEL_B, config->current[SM16825E_CHANNEL_B]);
    sm16825e_set_channel_current(SM16825E_CHANNEL_W, config->current[SM16825E_CHANNEL_W]);
    sm16825e_set_channel_current(SM16825E_CHANNEL_Y, config->current[SM16825E_CHANNEL_Y]);

    s_sm16825e->standby_enabled = false;

    // Initialize mapping array with default 1:1 mapping (R->OUTR, G->OUTG, B->OUTB, W->OUTW, Y->OUTY)
    memset(s_sm16825e->mapping_addr, INVALID_ADDR, 5);
    s_sm16825e->mapping_addr[SM16825E_CHANNEL_R] = SM16825E_PIN_OUTR;
    s_sm16825e->mapping_addr[SM16825E_CHANNEL_G] = SM16825E_PIN_OUTG;
    s_sm16825e->mapping_addr[SM16825E_CHANNEL_B] = SM16825E_PIN_OUTB;
    s_sm16825e->mapping_addr[SM16825E_CHANNEL_W] = SM16825E_PIN_OUTW;
    s_sm16825e->mapping_addr[SM16825E_CHANNEL_Y] = SM16825E_PIN_OUTY;

    s_sm16825e->buf = heap_caps_malloc(s_sm16825e->buf_size, MALLOC_CAP_DMA);
    DRIVER_CHECK(s_sm16825e->buf, "buf alloc fail", return ESP_ERR_NO_MEM);
    memset(s_sm16825e->buf, 0, s_sm16825e->buf_size);

    // Set GPIO drive capability for high-speed operation
    err = gpio_set_drive_capability(config->ctrl_io, GPIO_DRIVE_CAP_3);
    DRIVER_CHECK(err == ESP_OK, "set drive capability fail", goto EXIT);

    // Configure SPI bus for RZ protocol
    spi_bus_config_t buscfg = {
        .mosi_io_num = config->ctrl_io,
        .miso_io_num = -1,
        .sclk_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = s_sm16825e->buf_size,
    };

    spi_device_interface_config_t devcfg = {
        .command_bits = 0,
        .address_bits = 0,
        .dummy_bits = 0,
        .clock_speed_hz = SM16825E_SPI_SPEED_HZ,
        .duty_cycle_pos = 128,  // 50% duty cycle for RZ protocol
        .mode = 0,
        .spics_io_num = -1,
        .queue_size = 1,
    };

#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 3, 0))
    err = spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
#else
    int dma_chan = SPI2_HOST;
    err = spi_bus_initialize(SPI2_HOST, &buscfg, dma_chan);
#endif
    DRIVER_CHECK(err == ESP_OK, "spi_bus_initialize error", goto EXIT);

#if !CONFIG_IDF_TARGET_ESP32
    spi_dev_t *hw = spi_periph_signal[SPI2_HOST].hw;
    hw->ctrl.d_pol = 0;
#endif

    err = spi_bus_add_device(SPI2_HOST, &devcfg, &s_sm16825e->spi_handle);
    DRIVER_CHECK(err == ESP_OK, "spi_bus_add_device error", goto EXIT);

    // Send initial data
    err = _write(s_sm16825e->buf, s_sm16825e->buf_size);
    DRIVER_CHECK(err == ESP_OK, "set init data fail", goto EXIT);

    return ESP_OK;

EXIT:
    cleanup();
    return err;
}

esp_err_t sm16825e_deinit(void)
{
    DRIVER_CHECK(s_sm16825e, "not init", return ESP_ERR_INVALID_ARG);

    cleanup();
    ESP_LOGD(TAG, "SM16825E deinitialized");
    return ESP_OK;
}

esp_err_t sm16825e_regist_channel(sm16825e_channel_t channel, sm16825e_out_pin_t pin)
{
    DRIVER_CHECK(s_sm16825e, "not init", return ESP_ERR_INVALID_STATE);
    DRIVER_CHECK(channel < SM16825E_CHANNEL_MAX, "check channel fail", return ESP_ERR_INVALID_ARG);
    DRIVER_CHECK(pin < SM16825E_PIN_OUT_MAX, "check out pin fail", return ESP_ERR_INVALID_ARG);

    s_sm16825e->mapping_addr[channel] = pin;
    ESP_LOGD(TAG, "Channel %d mapped to pin %d", channel, pin);

    return ESP_OK;
}

/**
 * @brief Set RGBWY channels with individual control
 *
 * @param value_r Red channel value (0-255)
 * @param value_g Green channel value (0-255)
 * @param value_b Blue channel value (0-255)
 * @param value_w White channel value (0-255)
 * @param value_y Yellow channel value (0-255)
 * @return esp_err_t
 */
esp_err_t sm16825e_set_rgbwy_channel(uint16_t value_r, uint16_t value_g, uint16_t value_b,
                                     uint16_t value_w, uint16_t value_y)
{
    DRIVER_CHECK(s_sm16825e, "sm16825e_init() must be called first", return ESP_ERR_INVALID_STATE);

    if (s_sm16825e->standby_enabled) {
        sm16825e_set_standby(false);

        // According to the chip manual, the first frame after sleep is invalid for wake-up purposes.
        // So we need to send a frame with all channels set to 0 to wake up the chip.
        for (int i = 0; i < s_sm16825e->led_num; i++) {
            esp_err_t err = generate_data(i, 0, 0, 0, 0, 0, s_sm16825e->buf);
            if (err != ESP_OK) {
                return err;
            }
        }
        esp_err_t err = _write(s_sm16825e->buf, s_sm16825e->buf_size);
        if (err != ESP_OK) {
            return err;
        }

        // Insert a reset gap without blocking the scheduler by sending a zero-filled SPI burst
        uint8_t reset_gap[128] = { 0 };
        err = _write(reset_gap, sizeof(reset_gap));
        if (err != ESP_OK) {
            return err;
        }
    }

    for (int i = 0; i < s_sm16825e->led_num; i++) {
        esp_err_t err = generate_data(i, value_r, value_g, value_b, value_w, value_y, s_sm16825e->buf);
        if (err != ESP_OK) {
            return err;
        }
    }

    return _write(s_sm16825e->buf, s_sm16825e->buf_size);
}

/**
 * @brief Set current gain for specific channel
 *
 * @param channel Channel index (0=R, 1=G, 2=B, 3=W, 4=Y)
 * @param current_ma Current value in mA (10.2mA-300mA)
 * @return esp_err_t
 */
esp_err_t sm16825e_set_channel_current(uint8_t channel, uint16_t current_ma)
{
    DRIVER_CHECK(s_sm16825e, "sm16825e_init() must be called first", return ESP_ERR_INVALID_STATE);
    DRIVER_CHECK(channel < 5, "invalid channel", return ESP_ERR_INVALID_ARG);
    DRIVER_CHECK(current_ma >= 10 && current_ma <= 300, "invalid current value, must be 10-300mA", return ESP_ERR_INVALID_ARG);

    uint8_t gain = (uint8_t)((current_ma - 10) * 31 / 290);

    // Ensure gain is within valid range
    if (gain > 31) {
        gain = 31;
    }

    s_sm16825e->current_gain[channel] = gain;
    ESP_LOGD(TAG, "Channel %d current set to %dmA", channel, current_ma);

    return ESP_OK;
}

/**
 * @brief Enable or disable standby mode
 *
 * @param enable true to enable standby, false to disable
 * @return esp_err_t
 *
 * @note Standby mode reduces power consumption to <2mW
 */
esp_err_t sm16825e_set_standby(bool enable)
{
    DRIVER_CHECK(s_sm16825e, "sm16825e_init() must be called first", return ESP_ERR_INVALID_STATE);

    if (enable) {
        s_sm16825e->standby_enabled = true;
        esp_err_t err = sm16825e_set_shutdown();
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to shutdown outputs before entering standby");
            return err;
        }

        ESP_LOGD(TAG, "Standby mode enabled - all outputs shut down");
    } else {
        s_sm16825e->standby_enabled = false;
        ESP_LOGD(TAG, "Standby mode disabled - ready for normal operation");
    }

    return ESP_OK;
}

/**
 * @brief Shutdown all channels (set to 0)
 *
 * @return esp_err_t
 */
esp_err_t sm16825e_set_shutdown(void)
{
    DRIVER_CHECK(s_sm16825e, "sm16825e_init() must be called first", return ESP_ERR_INVALID_STATE);

    // If in standby, keep standby bit and send all-zero frames without waking
    if (s_sm16825e->standby_enabled) {
        for (int i = 0; i < s_sm16825e->led_num; i++) {
            esp_err_t err = generate_data(i, 0, 0, 0, 0, 0, s_sm16825e->buf);
            if (err != ESP_OK) {
                return err;
            }
        }
        return _write(s_sm16825e->buf, s_sm16825e->buf_size);
    }

    // Normal mode: set all channels to 0 to turn off LEDs
    return sm16825e_set_rgbwy_channel(0, 0, 0, 0, 0);
}
