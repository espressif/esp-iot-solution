DAC 音频
=============

ESP32 拥有两个独立的 DAC 通道，并可直接使用 I2S 通过 DMA 播放音频。这里在 esp-idf 的基础上简化了 API，并对数据进行了重新编码已支持更多类型的采样位宽。

API Reference
-------------

.. include:: /_build/inc/dac_audio.inc
