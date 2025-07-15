/*
 * SPDX-FileCopyrightText: 2022-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <inttypes.h>
#include <string.h>
#include "sdkconfig.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_idf_version.h"
#include "esp_attr.h"
#include "esp_private/usb_phy.h"
#include "usb_host_helpers.h"
#include "esp_intr_alloc.h"
#include "esp_bit_defs.h"

#define USB_PORT_NUM 1  //Default port number
static const char *TAG = "USB_STREAM";
/*------------------------------------------------ USB URB Code ----------------------------------------------------*/
void _usb_urb_clear(urb_t *urb)
{
    UVC_CHECK_RETURN_VOID(NULL != urb, "urb = NULL");
    usb_transfer_dummy_t *transfer_dummy = (usb_transfer_dummy_t *)&urb->transfer;
    uint8_t *data_buffer = transfer_dummy->data_buffer;
    size_t data_buffer_size = transfer_dummy->data_buffer_size;
    int num_isoc_packets = transfer_dummy->num_isoc_packets;
    void *context = transfer_dummy->context;
    memset(urb, 0, sizeof(urb_t) + (num_isoc_packets * sizeof(usb_isoc_packet_desc_t)));
    transfer_dummy->data_buffer = data_buffer;
    transfer_dummy->data_buffer_size = data_buffer_size;
    transfer_dummy->num_isoc_packets = num_isoc_packets;
    transfer_dummy->context = context;
}

urb_t *_usb_urb_alloc(int num_isoc_packets, size_t packet_data_buffer_size, void *context)
{
    uint8_t *data_buffer = NULL;
    urb_t *urb = heap_caps_calloc(1, sizeof(urb_t) + (num_isoc_packets * sizeof(usb_isoc_packet_desc_t)), MALLOC_CAP_DMA);
    UVC_CHECK_GOTO(NULL != urb, "urb alloc failed", _alloc_failed);
    //Allocate data buffer for each URB and assign them
    if (num_isoc_packets) {
        /* ISOC urb: num_isoc_packets must be 0 for isoc urb */
        data_buffer = heap_caps_calloc(num_isoc_packets, packet_data_buffer_size, MALLOC_CAP_DMA);
    } else {
        /* no ISOC urb */
        data_buffer = heap_caps_calloc(1, packet_data_buffer_size, MALLOC_CAP_DMA);
    }
    UVC_CHECK_GOTO(NULL != data_buffer, "urb data_buffer alloc failed", _alloc_failed);
    //Initialize URB and underlying transfer structure. Need to cast to dummy due to const fields
    size_t data_buffer_size = packet_data_buffer_size * (num_isoc_packets == 0 ? 1 : num_isoc_packets);
    usb_transfer_dummy_t *transfer_dummy = (usb_transfer_dummy_t *)&urb->transfer;
    transfer_dummy->data_buffer = data_buffer;
    transfer_dummy->data_buffer_size = data_buffer_size;
    transfer_dummy->num_isoc_packets = num_isoc_packets;
    transfer_dummy->context = context;
    ESP_LOGV(TAG, "urb(%p), alloc size %d = %d * %d", urb, data_buffer_size, packet_data_buffer_size, num_isoc_packets == 0 ? 1 : num_isoc_packets);
    return urb;
_alloc_failed:
    free(urb);
    free(data_buffer);
    return NULL;
}

void _usb_urb_free(urb_t *urb)
{
    UVC_CHECK_RETURN_VOID(NULL != urb, "urb = NULL");
    //Free data buffers of URB
    if (urb->transfer.data_buffer) {
        heap_caps_free(urb->transfer.data_buffer);
    }
    //Free the URB
    heap_caps_free(urb);
    ESP_LOGD(TAG, "urb free(%p)", urb);
}

urb_t **_usb_urb_list_alloc(uint32_t urb_num, uint32_t num_isoc_packets, uint32_t bytes_per_packet)
{
    ESP_LOGD(TAG, "urb list alloc urb_num = %"PRId32", isoc packets = %"PRId32 ", bytes_per_packet = %"PRId32, urb_num, num_isoc_packets, bytes_per_packet);
    urb_t **urb_list = heap_caps_calloc(urb_num, sizeof(urb_t *), MALLOC_CAP_DMA);
    UVC_CHECK(urb_list != NULL, "p_urb alloc failed", NULL);
    for (int i = 0; i < urb_num; i++) {
        urb_list[i] = _usb_urb_alloc(num_isoc_packets, bytes_per_packet, NULL);
        UVC_CHECK_GOTO(urb_list[i] != NULL, "stream urb alloc failed", failed_);
        urb_list[i]->transfer.num_bytes = (num_isoc_packets == 0 ? 1 : num_isoc_packets) * bytes_per_packet;
        for (size_t j = 0; j < num_isoc_packets; j++) {
            //We need to initialize each individual isoc packet descriptor of the URB
            urb_list[i]->transfer.isoc_packet_desc[j].num_bytes = bytes_per_packet;
        }
    }
    return urb_list;
failed_:
    for (size_t i = 0; i < urb_num; i++) {
        _usb_urb_free(urb_list[i]);
        urb_list[i] = NULL;
    }
    free(urb_list);
    return NULL;
}

void _usb_urb_list_clear(urb_t **urb_list, uint32_t urb_num, uint32_t packets_per_urb, uint32_t bytes_per_packet)
{
    if (urb_list) {
        for (size_t i = 0; i < urb_num; i++) {
            _usb_urb_clear(urb_list[i]);
            usb_transfer_dummy_t *transfer_dummy = (usb_transfer_dummy_t *)&urb_list[i]->transfer;
            transfer_dummy->num_isoc_packets = packets_per_urb;
            transfer_dummy->num_bytes = (packets_per_urb == 0 ? 1 : packets_per_urb) * bytes_per_packet;
            for (size_t j = 0; j < packets_per_urb; j++) {
                //We need to initialize each individual isoc packet descriptor of the URB
                urb_list[i]->transfer.isoc_packet_desc[j].num_bytes = bytes_per_packet;
            }
        }
    }
}

esp_err_t _usb_urb_list_enqueue(hcd_pipe_handle_t pipe_handle, urb_t **urb_list, uint32_t urb_num)
{
    UVC_CHECK(pipe_handle != NULL, "stream pipe not init", ESP_ERR_INVALID_ARG);
    UVC_CHECK(urb_list != NULL, "stream urb list not init", ESP_ERR_INVALID_ARG);
    UVC_CHECK(urb_num > 0, "stream urb num invalid", ESP_ERR_INVALID_ARG);
    esp_err_t ret = ESP_OK;
    for (int i = 0; i < urb_num; i++) {
        UVC_CHECK(urb_list[i] != NULL, "urb not init", ESP_FAIL);
        ret = hcd_urb_enqueue(pipe_handle, urb_list[i]);
        UVC_CHECK(ESP_OK == ret, "pipe enqueue failed", ESP_FAIL);
    }
    return ESP_OK;
}

void _usb_urb_list_free(urb_t **urb_list, uint32_t urb_num)
{
    if (urb_list) {
        for (size_t i = 0; i < urb_num; i++) {
            _usb_urb_free(urb_list[i]);
        }
        free(urb_list);
    }
}

/*------------------------------------------------ USB Port Code ----------------------------------------------------*/
hcd_port_event_t _usb_port_event_dflt_process(hcd_port_handle_t port_hdl, hcd_port_event_t event)
{
    (void)event;
    UVC_CHECK(port_hdl != NULL, "port handle can not be NULL", HCD_PORT_EVENT_NONE);
    hcd_port_event_t actual_evt = hcd_port_handle_event(port_hdl);

    switch (actual_evt) {
    case HCD_PORT_EVENT_CONNECTION:
        //Reset newly connected device
        ESP_LOGI(TAG, "line %u HCD_PORT_EVENT_CONNECTION", __LINE__);
        break;

    case HCD_PORT_EVENT_DISCONNECTION:
        ESP_LOGW(TAG, "line %u HCD_PORT_EVENT_DISCONNECTION", __LINE__);
        break;

    case HCD_PORT_EVENT_ERROR:
        ESP_LOGW(TAG, "line %u HCD_PORT_EVENT_ERROR", __LINE__);
        break;

    case HCD_PORT_EVENT_OVERCURRENT:
        ESP_LOGW(TAG, "line %u HCD_PORT_EVENT_OVERCURRENT", __LINE__);
        break;

    case HCD_PORT_EVENT_NONE:
        ESP_LOGD(TAG, "line %u HCD_PORT_EVENT_NONE", __LINE__);
        break;

    default:
        ESP_LOGE(TAG, "line %u invalid HCD_PORT_EVENT%d", __LINE__, actual_evt);
        break;
    }
    return actual_evt;
}

hcd_port_handle_t _usb_port_init(hcd_port_callback_t callback, void *callback_arg)
{
    UVC_CHECK(callback != NULL && callback_arg != NULL, "invalid args", NULL);
    esp_err_t ret = ESP_OK;
    hcd_port_handle_t port_hdl = NULL;
    usb_phy_config_t phy_config = {
        .controller = USB_PHY_CTRL_OTG,
        .target = USB_PHY_TARGET_INT,
        .otg_mode = USB_OTG_MODE_HOST,
        .otg_speed = USB_PHY_SPEED_UNDEFINED,   //In Host mode, the speed is determined by the connected device
#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0))
        .otg_io_conf = NULL,
#else
        .gpio_conf = NULL,
#endif
    };
    usb_phy_handle_t phy_handle = NULL;
    ret = usb_new_phy(&phy_config, &phy_handle);
    UVC_CHECK(ESP_OK == ret, "USB PHY init failed", NULL);
    hcd_config_t hcd_config = {
        .intr_flags = ESP_INTR_FLAG_LEVEL2,
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 4, 1)
        .peripheral_map = BIT0,
        .fifo_config = NULL,
#endif
    };
    ret = hcd_install(&hcd_config);
    UVC_CHECK_GOTO(ESP_OK == ret, "HCD Install failed", hcd_init_err);
    hcd_port_config_t port_cfg = {
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 3, 3)
        .fifo_bias = HCD_PORT_FIFO_BIAS_BALANCED,
#endif
        .callback = callback,
        .callback_arg = callback_arg,
        .context = phy_handle,
    };

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 4, 1)
    int root_port_index = 0;
    if (hcd_config.peripheral_map & BIT1) {
        root_port_index = 1;
    }
    ret = hcd_port_init(root_port_index, &port_cfg, &port_hdl);
#else
    ret = hcd_port_init(USB_PORT_NUM, &port_cfg, &port_hdl);
#endif
    UVC_CHECK_GOTO(ESP_OK == ret, "HCD Port init failed", port_init_err);
    ret = hcd_port_command(port_hdl, HCD_PORT_CMD_POWER_ON);
    UVC_CHECK_GOTO(ESP_OK == ret, "HCD Port Power on failed", port_power_err);
    ESP_LOGD(TAG, "Port=%d init succeed, context = %p", USB_PORT_NUM, phy_handle);
    return port_hdl;
port_power_err:
    hcd_port_deinit(port_hdl);
port_init_err:
    hcd_uninstall();
hcd_init_err:
    usb_del_phy(phy_handle);
    return NULL;
}

esp_err_t _usb_port_deinit(hcd_port_handle_t port_hdl)
{
    esp_err_t ret;
    ret = hcd_port_command(port_hdl, HCD_PORT_CMD_DISABLE);
    UVC_CHECK_CONTINUE(ESP_OK == ret, "port disable failed");
    ret = hcd_port_command(port_hdl, HCD_PORT_CMD_POWER_OFF);
    UVC_CHECK_CONTINUE(ESP_OK == ret, "Port power off failed");
    usb_phy_handle_t phy_handle = hcd_port_get_context(port_hdl);
    ret = hcd_port_deinit(port_hdl);
    UVC_CHECK_CONTINUE(ESP_OK == ret, "port deinit failed");
    ret = hcd_uninstall();
    UVC_CHECK_CONTINUE(ESP_OK == ret, "hcd uninstall failed");
    ret = usb_del_phy(phy_handle);
    UVC_CHECK_CONTINUE(ESP_OK == ret, "phy delete failed");
    ESP_LOGD(TAG, "Port=%d deinit succeed, context = %p", USB_PORT_NUM, phy_handle);
    return ret;
}

esp_err_t _usb_port_get_speed(hcd_port_handle_t port_hdl, usb_speed_t *port_speed)
{
    UVC_CHECK(port_hdl != NULL && port_speed != NULL, "invalid args", ESP_ERR_INVALID_ARG);
    esp_err_t ret = hcd_port_get_speed(port_hdl, port_speed);
    UVC_CHECK(ESP_OK == ret, "port speed get failed", ESP_FAIL);
    return ret;
}

#ifdef RANDOM_ERROR_TEST
#include "esp_random.h"
#endif
/*------------------------------------------------ USB Pipe Code ----------------------------------------------------*/
IRAM_ATTR hcd_pipe_event_t _pipe_event_dflt_process(hcd_pipe_handle_t pipe_handle, const char *pipe_name, hcd_pipe_event_t pipe_event)
{
    UVC_CHECK(pipe_handle != NULL, "pipe handle can not be NULL", pipe_event);
    hcd_pipe_event_t actual_evt = pipe_event;

#ifdef RANDOM_ERROR_TEST
    if (HCD_PIPE_EVENT_URB_DONE == pipe_event) {
        actual_evt = (esp_random() % 10 > 8) ? HCD_PIPE_EVENT_ERROR_XFER : HCD_PIPE_EVENT_URB_DONE;
    }
#endif

    switch (pipe_event) {
    case HCD_PIPE_EVENT_NONE:
        break;
    case HCD_PIPE_EVENT_URB_DONE:
        ESP_LOGV(TAG, "Pipe(%s): XFER_DONE", pipe_name);
        break;

    case HCD_PIPE_EVENT_ERROR_XFER:
        ESP_LOGW(TAG, "Pipe(%s): ERROR_XFER", pipe_name);
        break;

    case HCD_PIPE_EVENT_ERROR_URB_NOT_AVAIL:
        ESP_LOGW(TAG, "Pipe(%s): ERROR_URB_NOT_AVAIL", pipe_name);
        break;

    case HCD_PIPE_EVENT_ERROR_OVERFLOW:
        ESP_LOGW(TAG, "Pipe(%s): ERROR_OVERFLOW", pipe_name);
        break;
    case HCD_PIPE_EVENT_ERROR_STALL:
        ESP_LOGW(TAG, "Pipe(%s): ERROR_STALL", pipe_name);
        break;
    default:
        ESP_LOGW(TAG, "Pipe(%s): invalid EVENT = %d", pipe_name, pipe_event);
        break;
    }
    return actual_evt;
}

hcd_pipe_handle_t _usb_pipe_init(hcd_port_handle_t port_hdl, usb_ep_desc_t *ep_desc, uint8_t dev_addr, usb_speed_t dev_speed, void *context, hcd_pipe_callback_t callback, void *callback_arg)
{
    UVC_CHECK(port_hdl != NULL, "invalid args", NULL);
    hcd_pipe_config_t pipe_cfg = {
        .callback = callback,
        .callback_arg = callback_arg,
        .context = context,
        .ep_desc = ep_desc,    //NULL EP descriptor to create a default pipe
        .dev_addr = dev_addr,
        .dev_speed = dev_speed,
    };

    esp_err_t ret = ESP_OK;
    hcd_pipe_handle_t pipe_handle = NULL;
    ret = hcd_pipe_alloc(port_hdl, &pipe_cfg, &pipe_handle);
    UVC_CHECK(ESP_OK == ret, "pipe alloc failed", NULL);
    ESP_LOGD(TAG, "pipe(%p) init succeed", pipe_handle);
    return pipe_handle;
}

esp_err_t _usb_pipe_flush(hcd_pipe_handle_t pipe_hdl, size_t urb_num)
{
    UVC_CHECK(pipe_hdl != NULL, "invalid args", ESP_ERR_INVALID_ARG);
    ESP_LOGD(TAG, "pipe flushing: state = %d", hcd_pipe_get_state(pipe_hdl));
    esp_err_t ret = hcd_pipe_command(pipe_hdl, HCD_PIPE_CMD_HALT);
    if (ESP_OK != ret) {
        ESP_LOGW(TAG, "pipe=%p halt failed", pipe_hdl);
    }
    ret = hcd_pipe_command(pipe_hdl, HCD_PIPE_CMD_FLUSH);
    if (ESP_OK != ret) {
        ESP_LOGW(TAG, "pipe=%p flush failed", pipe_hdl);
    }
    for (size_t i = 0; i < urb_num; i++) {
        urb_t *urb = hcd_urb_dequeue(pipe_hdl);
        ESP_LOGV(TAG, "urb dequeue handle = %p", urb);
    }
    ESP_LOGD(TAG, "urb dequeued num = %d", urb_num);
    ESP_LOGD(TAG, "pipe(%p) flush succeed", pipe_hdl);
    return ESP_OK;
}

esp_err_t _usb_pipe_clear(hcd_pipe_handle_t pipe_hdl, size_t urb_num)
{
    UVC_CHECK(pipe_hdl != NULL, "invalid args", ESP_ERR_INVALID_ARG);
    ESP_LOGD(TAG, "pipe clearing: state = %d", hcd_pipe_get_state(pipe_hdl));
    for (size_t i = 0; i < urb_num; i++) {
        urb_t *urb = hcd_urb_dequeue(pipe_hdl);
        ESP_LOGV(TAG, "urb dequeue handle = %p", urb);
    }
    ESP_LOGD(TAG, "urb dequeued num = %d", urb_num);
    if (hcd_pipe_get_state(pipe_hdl) == HCD_PIPE_STATE_ACTIVE) {
        return ESP_OK;
    }
    esp_err_t ret = hcd_pipe_command(pipe_hdl, HCD_PIPE_CMD_CLEAR);
    if (ESP_OK != ret) {
        ESP_LOGD(TAG, "pipe=%p clear failed", pipe_hdl);
        return ESP_FAIL;
    }
    ESP_LOGD(TAG, "pipe(%p) clear succeed", pipe_hdl);
    return ESP_OK;
}

esp_err_t _usb_pipe_deinit(hcd_pipe_handle_t pipe_hdl, size_t urb_num)
{
    UVC_CHECK(pipe_hdl != NULL, "invalid args", ESP_ERR_INVALID_ARG);
    esp_err_t ret = _usb_pipe_flush(pipe_hdl, urb_num);
    if (ESP_OK != ret) {
        ESP_LOGW(TAG, "pipe=%p flush failed", pipe_hdl);
    }
    ret = hcd_pipe_free(pipe_hdl);
    if (ESP_OK != ret) {
        ESP_LOGW(TAG, "pipe=%p free failed", pipe_hdl);
        return ret;
    }
    ESP_LOGD(TAG, "pipe(%p) deinit succeed", pipe_hdl);
    return ESP_OK;
}
