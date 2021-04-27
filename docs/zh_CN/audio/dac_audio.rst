DAC 音频
=============
:link_to_translation:`en:[English]`

ESP32 拥有两个独立的 DAC 通道，并可直接使用 I2S 通过 DMA 播放音频。这里在 ESP-IDF 的基础上简化了 API，并对数据进行了重新编码以支持更多类型的采样位宽。

API 参考
-------------

.. include:: /_build/inc/dac_audio.inc
