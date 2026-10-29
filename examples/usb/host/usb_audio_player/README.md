| Supported Targets | ESP32-P4 | ESP32-S2 | ESP32-S3 | ESP32-S31 |
| ----------------- | -------- | -------- | -------- | --------- |

## USB Host Audio Player Example

This example demonstrates how to use `usb_host_uac` component to play an MP3 file from SPIFFS to a USB Audio Class speaker device.

### Current Behavior and Limits

- This example targets **USB speaker playback** only
- Microphone devices may be detected, but microphone streaming is not handled in this example
- The playback source is a file in SPIFFS, not a USB audio source
- If the connected speaker does not support the default 48 kHz / 16-bit / 2-channel format, the example selects a supported PCM format from the device descriptors and converts the decoded audio to that format automatically

### MP3 File

To test your own audio file:

1. Put the MP3 file in the project's `spiffs` directory
2. Update `MP3_FILE_NAME` in `main/usb_audio_player_main.c`

The URI used by the player is built from:

- SPIFFS base path: `/spiffs`
- Default file name: `/new_epic.mp3`

> The default MP3 file is `spiffs/new_epic.mp3`, a 48 kHz 16-bit stereo track from 500Audio: https://500audio.com/track/new-epic_22682

### Hardware Requirements

- An ESP32 board with USB OTG host capability.
- A proper USB connection and 5V power for the USB device when required.

Connection example:

    ```
    ┌─────────────┐          ┌─────────────────┐
    │             ┼──────────┼5V               │
    │ USB Speaker ┼──────────┼GND              │
    │             │          │    ESP32-xx     │
    │             │          │                 │
    │             ┼──────────┼USB D+           │
    │             ┼──────────┼USB D-           │
    │             │          │                 │
    └─────────────┘          │                 │
                             └─────────────────┘
    ```

### Build and Flash

```bash
idf.py set-target TARGET
idf.py -p PORT flash monitor
```

Replace `PORT` and `TARGET` with the serial port of your board.

(To exit the serial monitor, type `Ctrl-]`.)

See the Getting Started Guide for all the steps to configure and use the ESP-IDF to build projects.
