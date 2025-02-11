# ChangeLog

## v1.0.2 - 2025-2-11

### Bug Fixes:

* Fix OpenAI_Request not handling chunked data correctly [!467](https://github.com/espressif/esp-iot-solution/pull/467)

## v1.0.1 - 2024-11-5

### Bug Fixes:

* Fix incorrect free of cjson object's payload memory

## v1.0.0 - 2024-5-15

### Break Changes:

* Changed C++ reserved keyword 'delete' to 'deleteResponse'

## v0.3.1 - 2023-12-29

### Enhancements

* Fix infinite loop of TTS read
* Update Error message

## v0.3.0 - 2023-12-4

### Enhancements

* Support OpenAI text to speech api

## v0.2.0 - 2023-11-15

### Enhancements

* Support config the OpenAI Base URL through API `OpenAIChangeBaseURL` or `menuconfig`. Thanks [@Sam McLeod](https://github.com/sammcj)
* Using MP3 format audio file in unit test

## v0.1.2 - 2023-8-4

### Docs

* Update OpenAI.h for build online docs

## v0.1.1 - 2023-6-29

### Enhancements

* Support http chunked transfer.

## v0.1.0 - 2023-6-14

### Enhancements

* Init version