# Modem AT Command Parser

## Overview

This is an AT command parser component for communicating with 4G modules, developed based on the ESP-IDF framework. The component sends AT commands through your own interface and parses module responses, supporting timeout control, suitable for 4G Modem communication scenarios in IoT devices.

### Key Features

- Standard AT command sending and response parsing
- Built-in timeout control
- Extensible response handling callbacks

## Usage

```c
at_handle_t handle = NULL;

static esp_err_t your_send_function(const char *cmd, size_t length, void *usr_data)
{
    //send command with your own interface
}

// your data received callback
static void data_recv_data_cb(void)
{
    char *buffer;
    size_t buffer_remain;
    modem_at_get_response_buffer(handle, &buffer, &buffer_remain);
    if (buffer_remain < length) {
        length = buffer_remain;
        ESP_LOGE(TAG, "data size is too big, truncated to %d", length);
    }
    data_read_bytes((uint8_t *)buffer, &length); // read data from your interface
    // Parse the AT command response
    modem_at_write_response_done(handle, length);
}

void at_init()
{
    modem_at_config_t cfg = {
        .send_buffer_length = 256,
        .recv_buffer_length = 256,
        .io = {
            .send_cmd = your_send_function,
            .usr_data = your_context
        }
    };
    handle = modem_at_parser_create(&cfg);

    modem_at_start(handle);
    at_cmd_at(handle); // test AT command

    // free the AT command parser
    modem_at_parser_destroy(at_handle);
}
```
