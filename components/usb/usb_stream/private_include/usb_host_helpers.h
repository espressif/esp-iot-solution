/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "hcd.h"
#include "usb/usb_types_stack.h"
#include "usb_private.h"

#define UVC_CHECK(a, str, ret) if(!(a)) { \
        ESP_LOGE(TAG,"%s:%d (%s):%s", __FILE__, __LINE__, __FUNCTION__, str); \
        return (ret); \
    }

#define UVC_CHECK_ABORT(a, str) if(!(a)) { \
        ESP_LOGE(TAG,"%s:%d (%s):%s", __FILE__, __LINE__, __FUNCTION__, str); \
        abort(); \
    }

#define UVC_CHECK_RETURN_VOID(a, str) if(!(a)) { \
        ESP_LOGE(TAG,"%s:%d (%s):%s", __FILE__, __LINE__, __FUNCTION__, str); \
        return; \
    }

#define UVC_CHECK_CONTINUE(a, str) if(!(a)) { \
        ESP_LOGE(TAG,"%s:%d (%s):%s", __FILE__, __LINE__, __FUNCTION__, str); \
    }

#define UVC_CHECK_GOTO(a, str, label) if(!(a)) { \
        ESP_LOGE(TAG,"%s:%d (%s):%s", __FILE__, __LINE__, __FUNCTION__, str); \
        goto label; \
    }

/* The usb host helper functions, only for usb_stream driver */
void _usb_urb_clear(urb_t *urb);
urb_t *_usb_urb_alloc(int num_isoc_packets, size_t packet_data_buffer_size, void *context);
void _usb_urb_free(urb_t *urb);
urb_t **_usb_urb_list_alloc(uint32_t urb_num, uint32_t num_isoc_packets, uint32_t bytes_per_packet);
void _usb_urb_list_clear(urb_t **urb_list, uint32_t urb_num, uint32_t packets_per_urb, uint32_t bytes_per_packet);
esp_err_t _usb_urb_list_enqueue(hcd_pipe_handle_t pipe_handle, urb_t **urb_list, uint32_t urb_num);
void _usb_urb_list_free(urb_t **urb_list, uint32_t urb_num);
hcd_port_event_t _usb_port_event_dflt_process(hcd_port_handle_t port_hdl, hcd_port_event_t event);
hcd_pipe_event_t _pipe_event_dflt_process(hcd_pipe_handle_t pipe_handle, const char *pipe_name, hcd_pipe_event_t pipe_event);;
hcd_port_handle_t _usb_port_init(hcd_port_callback_t callback, void *callback_arg);
esp_err_t _usb_port_deinit(hcd_port_handle_t port_hdl);
esp_err_t _usb_port_get_speed(hcd_port_handle_t port_hdl, usb_speed_t *port_speed);
hcd_pipe_handle_t _usb_pipe_init(hcd_port_handle_t port_hdl, usb_ep_desc_t *ep_desc, uint8_t dev_addr, usb_speed_t dev_speed, void *context, hcd_pipe_callback_t callback, void *callback_arg);
esp_err_t _usb_pipe_flush(hcd_pipe_handle_t pipe_hdl, size_t urb_num);
esp_err_t _usb_pipe_clear(hcd_pipe_handle_t pipe_hdl, size_t urb_num);
esp_err_t _usb_pipe_deinit(hcd_pipe_handle_t pipe_hdl, size_t urb_num);
