ADC 麦克风
==============
:link_to_translation:`en:[English]`

使用 ADC 采集模拟麦克风数据，无需使用外部的音频 codec 芯片，适用于对采样率要求不太高且成本敏感的应用。在代码结构上采用 `esp_codec_dev <https://components.espressif.com/components/espressif/esp_codec_dev>`__ 的工程模式。

特性
------

- 支持多路 ADC 音频采样。
- 支持最大 12 位的 ADC 采样分辨率。

.. note:: 多路采样支持的最大分辨率为，ADC 最大采样率（S3 为 83.3K） / 采样通道数。

参考电路
----------


外部配置 ADC Continuous
--------------------------

可在外部初始化好 adc continuous mode, 并将 ``adc_continuous_handle_t`` 传入。

.. code::c
    adc_continuous_handle_t handle;
    adc_continuous_handle_cfg_t adc_config = {
        .max_store_buf_size = adc_cfg->max_store_buf_size,
        .conv_frame_size = adc_cfg->conv_frame_size,
        .flags.flush_pool = true,
    };

    adc_continuous_new_handle(&adc_config, &handle);

与 ADC oneshot 模式一起使用
------------------------------

- 如果 ADC oneshot 与 ADC condition 使用不同的 ADC Unit，那么不会有冲突。

- 如果使用相同的 ADC Unit, 请调用 ``esp_codec_dev_close()`` 关闭 ``adc_mic`` 时调用 ``adc_oneshot_read``。

.. code:: C

    adc_continuous_start(handle);
    adc_continuous_read();
    adc_continuous_stop();
    adc_oneshot_read();

示例代码
----------

.. code:: C

    audio_codec_adc_cfg_t cfg = DEFAULT_AUDIO_CODEC_ADC_MONO_CFG(ADC_CHANNEL_0, 16000);
    const audio_codec_data_if_t *adc_if = audio_codec_new_adc_data(&cfg);

    esp_codec_dev_cfg_t codec_dev_cfg = {
        .dev_type = ESP_CODEC_DEV_TYPE_IN,
        .data_if = adc_if,
    };
    esp_codec_dev_handle_t dev = esp_codec_dev_new(&codec_dev_cfg);
    esp_codec_dev_sample_info_t fs = {
        .sample_rate = 16000,
        .channel = 1,
        .bits_per_sample = 16,
    };
    esp_codec_dev_open(dev, &fs);
    uint16_t *audio_buffer = malloc(16000 * sizeof(uint16_t));
    assert(audio_buffer);
    while (1) {
        int ret = esp_codec_dev_read(dev, audio_buffer, sizeof(uint16_t) * 16000);
        ESP_LOGI(TAG, "esp_codec_dev_read ret: %d\n", ret);
    }


API 参考
-------------

.. include-build-file:: inc/adc_mic.inc
