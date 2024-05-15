OpenAI
=============
:link_to_translation:`en:[English]`

The `openai` component is a porting version of  `OpenAI-ESP32 <https://github.com/me-no-dev/OpenAI-ESP32>`_ arduino library, which simplifies the process of invoking the OpenAI API on `ESP-IDF`. This library provides extensive support for the majority of OpenAI API functionalities, except for `files` and `fine-tunes`. For more details, please refer to `OpenAI <https://platform.openai.com/docs/api-reference>`_  API REFERENCE.

FAQ
-----

Error `Failed to allocate request buffer!` when using `audioTranscription->file`
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

This error indicates that dynamic memory allocation failed, usually due to insufficient available memory. The required memory size depends on the current available RAM or PSRAM size.

**Solutions**

1. **Enable PSRAM**: If the device supports PSRAM, enable PSRAM to increase available memory and enable the `CONFIG_SPIRAM_USE_MALLOC` macro to use PSRAM.

2. **Reduce the audio data size**: For example, reduce the audio sampling rate or the length of the audio.


API Reference
-------------

.. include-build-file:: inc/OpenAI.inc