OpenAI
=============
:link_to_translation:`en:[English]`

`openai` 组件是 `OpenAI-ESP32 <https://github.com/me-no-dev/OpenAI-ESP32>`_ Arduino 库的移植版本，它简化了在 `ESP-IDF` 上调用 OpenAI API 的过程。该库广泛支持大部分 OpenAI API 的功能，但不包括 `files` 和 `fine-tunes`。更多详情，请参考 `OpenAI <https://platform.openai.com/docs/api-reference>`_ 的 API 参考文档。

FAQ
------

使用 `audioTranscription->file` 报错 `Failed to allocate request buffer!`
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

该错误表明动态内存分配失败，通常是由于可用内存不足导致的。所需的内存大小取决于当前剩余的 RAM 或 PSRAM 大小。

**解决方法**

1. **开启 PSRAM**：如果设备支持 PSRAM，可以开启 PSRAM ，以增加可用内存，并打开宏 `CONFIG_SPIRAM_USE_MALLOC` 以使用 PSRAM。
2. **减少音频的数据量**:减少音频的数据量，例如减少音频的采样率、音频的长度等。

API 参考
------------

.. include-build-file:: inc/OpenAI.inc