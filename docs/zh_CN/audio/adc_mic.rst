ADC 麦克风
==============
:link_to_translation:`en:[English]`

使用 ADC 采集模拟麦克风数据，无需使用外部的音频 codec 芯片，适用于对采样精度要求不太高且成本敏感的应用。在代码结构上采用 `esp_codec_dev <https://components.espressif.com/components/espressif/esp_codec_dev>`__ 的工程模式。

特性
------

- 支持多路 ADC 音频采样。
- 支持最大 12 位的 ADC 采样分辨率。

.. note:: 多路采样支持的最大分辨率为，ADC 最大采样率（S3 为 83.3K） / 采样通道数。

参考电路
----------

外部参考电路如下，可以选择跨阻放大器或者同相/反相放大电路对 MIC 输入信号进行放大。在大多数情况下，使用跨阻放大器可以取得更佳的信噪比，在本示例中 R29 用于为 MIC 添加直流偏置，使用通用运放 LMV321 作为运算放大器，通过 R26 与 C19 构成反馈电路，使运放工作在跨阻放大模式，其中阻容根据 MIC 的灵敏度进行调整。R31 与 R30 用于设定中位电压，以避免交越失真，R27 与 R32 用于提供直流偏置，避免 ADC 采集时失真。关于前端 MIC 放大器的更多设计参考信息，请参考 `Single-Supply, Electret Microphone Pre-Amplifier Reference Design <https://www.ti.com/lit/ug/tidu765/tidu765.pdf>`__。

.. note::
   - 偏置：ADC 端的中点偏置是必须的（如 Vref/2），避免 ADC 采集失真，默认情况下 ESP32-C3 的 Vref 约为 0.9V。 
   - 电源：MIC 供电最好使用 LDO 进行稳压，避免电源波动影响 ADC 采样。如果不使用外部 LDO，请在电源侧进行单独的 RC 滤波（在本示例中 R28 与 C23 用于滤波）， MIC 推荐与运放独立供电。
   - 推荐选择灵敏度大于 -46dB 的 MIC，避免放大后的信噪比过低。并根据 MIC 的灵敏度调整放大倍数。

.. figure:: ../../_static/audio/adc_mic_hardware_ref_design.png
    :align: center
    :alt: ADC mic hardware reference design

    ADC mic hardware reference design

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
