# ChangeLog

## v0.3.0 - 2026-06-12

### Enhancement ###

* Move all file I/O into a background task, using a ringbuffer.
* Flush all buffers and fsync() files when esp-restart() is called.
* Handle read errors in esp_log_router_dump_file_messages()
* KConfig options to disable the ringbuffer, the per-file batch-buffers, or both.
* KConfig options to allocate batch-buffers and/or ringbuffer in PSRAM.
* KConfig option to statically-allocate ringbuffer.

### Feature ###
* new functions: esp_log_router_status_file(), esp_log_router_sync_file
* esp_log_router_dump_file_messages() now returns ESP_ERR_NOT_FOUND instead of ESP_OK if the file is not currently a log target.

### Bug fix ###

* Fix buffer timeout flush being delayed until the next call to a log function.
* Fix esp_log_router_dump_file_messages() splitting lines longer than its buffer.
* Fix esp_log_router_dump_file_messages() not printing recent messages.

## v0.2.0 - 2026-05-15

### Feature ###

* Append an indicator string to truncated messages

### Bug fix ###

* Prevent buffer overrun when messages are truncated.
* Stop wasting a byte at the end of the buffer.

## v0.1.1 - 2025-08-25

### Bug fix ###

* Do not enter critical section until the mutex is taken.
* Return ESP_ERR_NO_MEM if creating the mutex fails.
* 
## v0.1.0 - 2025-7-1

First release version.
