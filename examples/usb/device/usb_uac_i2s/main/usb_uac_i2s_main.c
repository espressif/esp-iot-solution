/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include "driver/gpio.h"
#include "driver/i2s_pdm.h"
#include "driver/i2s_std.h"
#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include "usb_device_uac.h"
#include <math.h>

#define SPEAKER_I2S_DOUT CONFIG_SPEAKER_I2S_DOUT
#define SPEAKER_I2S_BCLK CONFIG_SPEAKER_I2S_BCLK
#define SPEAKER_I2S_LRC CONFIG_SPEAKER_I2S_LRC
#ifdef CONFIG_SPEAKER_SD_MODE_ENABLE
#define SPEAKER_SD_MODE CONFIG_SPEAKER_SD_MODE
#endif

#define MIC_I2S_CLK CONFIG_MIC_I2S_CLK
#define MIC_I2S_LR CONFIG_MIC_I2S_LR
#define MIC_I2S_DATA CONFIG_MIC_I2S_DATA

static i2s_chan_handle_t rx;
static i2s_chan_handle_t tx;

static bool is_muted = false;
/* default to 0dB (full volume) */
static uint32_t gain = 100;

static esp_err_t usb_uac_device_output_cb(uint8_t *buf, size_t len, void *arg) {
  if (!tx) {
    return ESP_FAIL;
  }
  /* process the samples */
  int16_t *samples = (int16_t *)buf;
  for (size_t i = 0; i < len / 2; i++) {
    /* if muted - set the sample to 0 so no sound is played */
    if (is_muted) {
      samples[i] = 0;
      continue;
    }
    /* apply the gain */
    int32_t sample = samples[i];
    sample = (sample * gain) / 100;
    /* clip the sample to the range of -32768 to 32767 (16 bits) */
    if (sample > 32767) {
      sample = 32767;
    } else if (sample < -32768) {
      sample = -32768;
    }
    samples[i] = (int16_t)sample;
  }
  /* write the samples to the I2S TX channel */
  size_t total_bytes_written = 0;
  while (total_bytes_written < len) {
    size_t bytes_written = 0;
    i2s_channel_write(tx, (uint8_t *)buf + total_bytes_written,
                      len - total_bytes_written, &bytes_written, portMAX_DELAY);
    total_bytes_written += bytes_written;
  }
  if (total_bytes_written < len) {
    return ESP_FAIL;
  }
  return ESP_OK;
}

static esp_err_t usb_uac_device_input_cb(uint8_t *buf, size_t len,
                                         size_t *bytes_read, void *arg) {
  if (!rx) {
    return ESP_FAIL;
  }
  /* read directly from the I2S RX channel */
  return i2s_channel_read(rx, buf, len, bytes_read, portMAX_DELAY);
}

static void usb_uac_device_set_mute_cb(uint32_t mute, void *arg) {
  is_muted = mute;
}

static void usb_uac_device_set_volume_cb_db(int32_t volume_db, void *arg) {
  /* convert from dB to a percentage */
  gain = pow(10, volume_db / 20.0f) * 100.0f;
}

static void usb_uac_device_init(void) {
  uac_device_config_t config = {
      .output_cb = usb_uac_device_output_cb,
      .input_cb = usb_uac_device_input_cb,
      .set_mute_cb = usb_uac_device_set_mute_cb,
      .set_volume_db_cb = usb_uac_device_set_volume_cb_db,
      .cb_ctx = NULL,
  };
  /* Init UAC device, UAC related configurations can be set by the menuconfig */
  ESP_ERROR_CHECK(uac_device_init(&config));
}

void init_pdm_rx(void) {
  i2s_chan_config_t chan_cfg =
      I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
  i2s_new_channel(&chan_cfg, NULL, &rx);

  i2s_pdm_rx_config_t pdm_cfg = {
      .clk_cfg = I2S_PDM_RX_CLK_DEFAULT_CONFIG(CONFIG_UAC_SAMPLE_RATE),
      .slot_cfg = I2S_PDM_RX_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT,
                                                 I2S_SLOT_MODE_MONO),
      .gpio_cfg =
          {
              .clk = MIC_I2S_CLK, // PDM clock
              // QUESTION - what about the LR clock pin? No longer relevant? Do
              // we tie it high or low?
              .din = MIC_I2S_DATA, // PDM data
              .invert_flags = {.clk_inv = false},
          },
  };
  pdm_cfg.slot_cfg.slot_mode = I2S_SLOT_MODE_MONO; // single mic

  i2s_channel_init_pdm_rx_mode(rx, &pdm_cfg);
  i2s_channel_enable(rx);
}

static void init_pcm_tx(void) {
  i2s_chan_config_t chan_cfg =
      I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_1, I2S_ROLE_MASTER);
  ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx, NULL));

  i2s_std_config_t std_cfg = {
      .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(CONFIG_UAC_SAMPLE_RATE),
      .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT,
                                                  I2S_SLOT_MODE_MONO),
      .gpio_cfg =
          {
              .mclk = I2S_GPIO_UNUSED, // set this if your amp needs MCLK
              .bclk = SPEAKER_I2S_BCLK,
              .ws = SPEAKER_I2S_LRC,
              .dout = SPEAKER_I2S_DOUT,
              .din = I2S_GPIO_UNUSED,
              .invert_flags =
                  {
                      .mclk_inv = false,
                      .bclk_inv = false,
                      .ws_inv = false, // if L/R are swapped or silent, try true
                  },
          },
  };

  ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx, &std_cfg));
  ESP_ERROR_CHECK(i2s_channel_enable(tx));
}

void app_main(void) {
  /* set the LR pin to low */
  gpio_reset_pin(MIC_I2S_LR);
  gpio_set_direction(MIC_I2S_LR, GPIO_MODE_OUTPUT);
  gpio_set_level(MIC_I2S_LR, 0);

  #ifdef CONFIG_SPEAKER_SD_MODE_ENABLE
  // enable the amplifier
  gpio_reset_pin(SPEAKER_SD_MODE);
  gpio_set_direction(SPEAKER_SD_MODE, GPIO_MODE_OUTPUT);
  gpio_set_level(SPEAKER_SD_MODE, 1);
  #endif

  // init the I2S peripherals
  init_pdm_rx();
  init_pcm_tx();

  // init the USB audio device
  usb_uac_device_init();

  // Nothing to do here - the USB audio device will take care of everything
}
