// Copyright 2019-2021 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "stdio.h"
#include "string.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "freertos/ringbuf.h"
#include "freertos/event_groups.h"

#include "esp_intr_alloc.h"
#include "esp_log.h"
#include "hal/usbh_ll.h"
#include "hcd.h"
#include "usb_private.h"
#include "esp_usbh_cdc.h"

static const char *TAG = "USB_HCDC";

#define EVENT_QUEUE_LEN                  32
#define PORT_NUM                         1
#define EP0_MPS                          64
#define CTRL_TRANSFER_DATA_MAX_BYTES     256  //Just assume that will only IN/OUT 256 bytes in ctrl pipe

#define USB_TASK_BASE_PRIORITY CONFIG_USB_TASK_BASE_PRIORITY
#define PORT_TASK_NAME "port"
#define PORT_TASK_PRIORITY (USB_TASK_BASE_PRIORITY+4)
#define PORT_TASK_STACK_SIZE 3072
#define PORT_TASK_CORE CONFIG_USB_TASK_CORE_ID

#define DFLT_PIPE_TASK_NAME "dflt"
#define DFLT_PIPE_TASK_PRIORITY (USB_TASK_BASE_PRIORITY+3)
#define DFLT_PIPE_TASK_STACK_SIZE 3072
#define DFLT_PIPE_TASK_CORE CONFIG_USB_TASK_CORE_ID

#define BULK_IN_PIPE_TASK_NAME "bulk-in"
#define BULK_IN_PIPE_TASK_PRIORITY (USB_TASK_BASE_PRIORITY)
#define BULK_IN_PIPE_TASK_STACK_SIZE 3072
#define BULK_IN_PIPE_TASK_CORE CONFIG_USB_TASK_CORE_ID

#define BULK_OUT_PIPE_TASK_NAME "bulk-out"
#define BULK_OUT_PIPE_TASK_PRIORITY (USB_TASK_BASE_PRIORITY+1)
#define BULK_OUT_PIPE_TASK_STACK_SIZE 3072
#define BULK_OUT_PIPE_TASK_CORE CONFIG_USB_TASK_CORE_ID

#define BULK_OUT_URB_NUM CONFIG_CDC_BULK_OUT_URB_NUM
#define BULK_IN_URB_NUM CONFIG_CDC_BULK_IN_URB_NUM
#define BUFFER_SIZE_BULK_OUT CONFIG_CDC_BULK_OUT_URB_BUFFER_SIZE
#define BUFFER_SIZE_BULK_IN CONFIG_CDC_BULK_IN_URB_BUFFER_SIZE
#define PORT_TASK_KILL_BIT           BIT1
#define DFLT_PIPE_TASK_KILL_BIT      BIT2
#define BULK_IN_PIPE_TASK_KILL_BIT   BIT3
#define BULK_OUT_PIPE_TASK_KILL_BIT  BIT4
#define USB_TASK_INIT_COMPLETED_BIT  BIT19
#define USB_TASK_PIPES_KILL_BITS (DFLT_PIPE_TASK_KILL_BIT | BULK_IN_PIPE_TASK_KILL_BIT | BULK_OUT_PIPE_TASK_KILL_BIT)

#ifdef CONFIG_CDC_USE_TRACE_FACILITY
static int s_ringbuf_in_size = 0;
static int s_ringbuf_out_size = 0;
static int s_bulkbuf_out_max = 0;
static int s_bulkbuf_in_max = 0;
static int s_ringbuf_out_max = 0;
static int s_ringbuf_in_max = 0;
#endif

static EventGroupHandle_t s_usb_event_group = NULL;
static usb_desc_ep_t s_bulk_in_ep;
static usb_desc_ep_t s_bulk_out_ep;
static usb_speed_t s_port_speed = 0;

static RingbufHandle_t in_ringbuf_handle = NULL;
static RingbufHandle_t out_ringbuf_handle = NULL;
static SemaphoreHandle_t in_ringbuf_mux = NULL;
static SemaphoreHandle_t out_ringbuf_mux = NULL;
static SemaphoreHandle_t usb_read_mux = NULL;
static SemaphoreHandle_t usb_write_mux = NULL;

static hcd_port_handle_t s_port_hdl = NULL;
static TaskHandle_t s_port_task_hdl = NULL;
static TaskHandle_t s_dflt_pipe_task_hdl = NULL;
static TaskHandle_t s_bulk_in_pipe_task_hdl = NULL;
static TaskHandle_t s_bulk_out_pipe_task_hdl = NULL;
volatile static int s_in_buffered_data_len = 0;
volatile static int s_out_buffered_data_len = 0;

/**
 * @brief hard-coded enum steps
 */
typedef enum {
    ENUM_STAGE_SET_ADDR,
    ENUM_STAGE_SET_CONFIG,
    ENUM_STAGE_SET_LINE_STATE,
    ENUM_STAGE_SET_ALT_SETTING,     //WAIT EP1 IN ready MPS:512, then set interface to start
    ENUM_STAGE_DONE,     //WAIT EP1 IN ready MPS:512, then set interface to start
} cdc_enum_stage_t;

/**
 * @brief SET_CONTROL_LINE_STATE bRequest=0x22
 *
 */
#define USB_CTRL_REQ_CDC_SET_LINE_STATE(ctrl_req_ptr, itf, dtr, rts) ({  \
        (ctrl_req_ptr)->bRequestType = 0x21;   \
        (ctrl_req_ptr)->bRequest = 0x22;    \
        (ctrl_req_ptr)->wValue = (rts ? 2 : 0) | (dtr ? 1 : 0); \
        (ctrl_req_ptr)->wIndex =  itf;    \
        (ctrl_req_ptr)->wLength = 0;   \
    })

/*------------------------------------------------ Task Code ----------------------------------------------------*/

static bool port_callback(hcd_port_handle_t port_hdl, hcd_port_event_t port_event, void *user_arg, bool in_isr)
{
    QueueHandle_t port_evt_queue = (QueueHandle_t)user_arg;
    assert(in_isr);    //Current HCD implementation should never call a port callback in a task context
    BaseType_t xTaskWoken = pdFALSE;
    xQueueSendFromISR(port_evt_queue, &port_event, &xTaskWoken);
    return (xTaskWoken == pdTRUE);
}

static bool pipe_callback(hcd_pipe_handle_t pipe_hdl, hcd_pipe_event_t pipe_event, void *user_arg, bool in_isr)
{
    QueueHandle_t pipe_evt_queue = (QueueHandle_t)user_arg;

    if (in_isr) {
        BaseType_t xTaskWoken = pdFALSE;
        xQueueSendFromISR(pipe_evt_queue, &pipe_event, &xTaskWoken);
        return (xTaskWoken == pdTRUE);
    } else {
        xQueueSend(pipe_evt_queue, &pipe_event, portMAX_DELAY);
        return false;
    }
}

static bool usb_driver_is_init(void)
{
    if (s_usb_event_group == NULL) {
        return false;
    }
    return xEventGroupGetBits(s_usb_event_group) & USB_TASK_INIT_COMPLETED_BIT;
}

static esp_err_t usb_out_ringbuf_push(const uint8_t *buf, size_t write_bytes, TickType_t xTicksToWait)
{
    int res = xRingbufferSend(out_ringbuf_handle, buf, write_bytes, xTicksToWait);

    if (res != pdTRUE) {
        ESP_LOGW(TAG, "The out buffer is too small, the data has been lost %u", write_bytes);
        return ESP_FAIL;
    }

    xSemaphoreTake(out_ringbuf_mux, portMAX_DELAY);
    s_out_buffered_data_len += write_bytes;
#ifdef CONFIG_CDC_USE_TRACE_FACILITY
    s_ringbuf_out_max = s_out_buffered_data_len > s_ringbuf_out_max?s_out_buffered_data_len:s_ringbuf_out_max;
#endif
    xSemaphoreGive(out_ringbuf_mux);
    return ESP_OK;
}

static esp_err_t usb_out_ringbuf_pop(uint8_t *buf, size_t req_bytes, size_t *read_bytes, TickType_t ticks_to_wait)
{
    uint8_t *buf_rcv = xRingbufferReceiveUpTo(out_ringbuf_handle, read_bytes, ticks_to_wait, req_bytes);

    if (buf_rcv) {
        memcpy(buf, buf_rcv, *read_bytes);
        vRingbufferReturnItem(out_ringbuf_handle, (void *)(buf_rcv));
        xSemaphoreTake(out_ringbuf_mux, portMAX_DELAY);
        s_out_buffered_data_len -= *read_bytes;
        xSemaphoreGive(out_ringbuf_mux);
        return ESP_OK;
    } else {
        return ESP_ERR_NO_MEM;
    }
}

static esp_err_t usb_in_ringbuf_push(const uint8_t *buf, size_t write_bytes, TickType_t xTicksToWait)
{
    int res = xRingbufferSend(in_ringbuf_handle, buf, write_bytes, xTicksToWait);

    if (res != pdTRUE) {
        ESP_LOGW(TAG, "The in buffer is too small, the data has been lost");
        return ESP_FAIL;
    }

    xSemaphoreTake(in_ringbuf_mux, portMAX_DELAY);
    s_in_buffered_data_len += write_bytes;
#ifdef CONFIG_CDC_USE_TRACE_FACILITY
    s_ringbuf_in_max = s_in_buffered_data_len > s_ringbuf_in_max?s_in_buffered_data_len:s_ringbuf_in_max;
#endif
    xSemaphoreGive(in_ringbuf_mux);
    return ESP_OK;
}

static esp_err_t usb_in_ringbuf_pop(uint8_t *buf, size_t req_bytes, size_t *read_bytes, TickType_t ticks_to_wait)
{
    uint8_t *buf_rcv = xRingbufferReceiveUpTo(in_ringbuf_handle, read_bytes, ticks_to_wait, req_bytes);

    if (buf_rcv) {
        memcpy(buf, buf_rcv, *read_bytes);
        vRingbufferReturnItem(in_ringbuf_handle, (void *)(buf_rcv));
        xSemaphoreTake(in_ringbuf_mux, portMAX_DELAY);
        s_in_buffered_data_len -= *read_bytes;
        xSemaphoreGive(in_ringbuf_mux);
        return ESP_OK;
    } else {
        return ESP_ERR_NO_MEM;
    }
}

static void port_task(void *arg)
{
    //Waitting for camera ready
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    hcd_config_t hcd_config = {
        .intr_flags = ESP_INTR_FLAG_LEVEL3,
    };

    ESP_ERROR_CHECK(hcd_install(&hcd_config));

    //Initialize a port
    QueueHandle_t port_evt_queue = xQueueCreate(EVENT_QUEUE_LEN, sizeof(hcd_port_event_t));
    assert(port_evt_queue != NULL);

    hcd_port_config_t port_config = {
        .callback = port_callback,
        .callback_arg = (void *)port_evt_queue,
        .context = NULL,
    };
    hcd_port_handle_t port_hdl;
    ESP_ERROR_CHECK(hcd_port_init(PORT_NUM, &port_config, &port_hdl));
    s_port_hdl = port_hdl;
    ESP_ERROR_CHECK(hcd_port_command(port_hdl, HCD_PORT_CMD_POWER_ON));

    hcd_port_event_t port_evt;

    while (!(xEventGroupGetBits(s_usb_event_group) & PORT_TASK_KILL_BIT)) {

        if (xQueueReceive(port_evt_queue, &port_evt, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        hcd_port_event_t actual_evt = hcd_port_handle_event(port_hdl);

        switch (actual_evt) {
            case HCD_PORT_EVENT_CONNECTION: {
                //Reset newly connected device
                ESP_LOGI(TAG, "Resetting Port\n");
                ESP_ERROR_CHECK(hcd_port_command(port_hdl, HCD_PORT_CMD_RESET));
                ESP_ERROR_CHECK(hcd_port_get_speed(port_hdl, &s_port_speed));
                ESP_LOGI(TAG, "Port speed = %d\n", s_port_speed);
                //Start the default pipe task
                xTaskNotifyGive(s_dflt_pipe_task_hdl);
                break;
            }

            case HCD_PORT_EVENT_DISCONNECTION:
                ESP_LOGW(TAG, "line %u HCD_PORT_EVENT_DISCONNECTION", __LINE__);
                break;

            case HCD_PORT_EVENT_ERROR:
                ESP_LOGW(TAG, "line %u HCD_PORT_EVENT_ERROR", __LINE__);
                abort();
                break;

            case HCD_PORT_EVENT_OVERCURRENT:
                ESP_LOGW(TAG, "line %u HCD_PORT_EVENT_OVERCURRENT", __LINE__);
                abort();
                break;

            case HCD_PORT_EVENT_SUDDEN_DISCONN:
                ESP_LOGW(TAG, "line %u HCD_PORT_EVENT_SUDDEN_DISCONN", __LINE__);
                break;

            case HCD_PORT_EVENT_NONE:
                break;

            default:
                ESP_LOGE(TAG, "line %u invalid HCD_PORT_EVENT%d", __LINE__, actual_evt);
                abort();
                break;
        }
    }

    ESP_LOGW(TAG, "port task will be deleted");
    xEventGroupClearBits(s_usb_event_group, PORT_TASK_KILL_BIT);
    hcd_port_command(port_hdl, HCD_PORT_CMD_POWER_OFF);
    hcd_port_deinit(port_hdl);
    s_port_hdl = NULL;
    hcd_uninstall();
    vQueueDelete(port_evt_queue);
    vTaskDelete(NULL);
}

static void dflt_pipe_task(void *arg)
{
    //Waiting for port connected
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    hcd_pipe_handle_t dflt_pipe_hdl;
    hcd_pipe_event_t pipe_evt;

    //Initialize default pipe
    QueueHandle_t dflt_pipe_evt_queue = xQueueCreate(EVENT_QUEUE_LEN, sizeof(hcd_pipe_event_t)); // for pipe event handling
    assert(dflt_pipe_evt_queue != NULL);
    hcd_pipe_config_t config = { //
        .callback = pipe_callback,
        .callback_arg = (void *)dflt_pipe_evt_queue,
        .context = NULL,
        .ep_desc = NULL,    //NULL EP descriptor to create a default pipe
        .dev_addr = 0,
        .dev_speed = s_port_speed,
    };
    ESP_ERROR_CHECK(hcd_pipe_alloc(s_port_hdl, &config, &dflt_pipe_hdl));
    ESP_LOGI(TAG, "Pipe Default Created");

    //malloc URB for default control
    uint8_t *data_buffer = heap_caps_malloc(sizeof(usb_ctrl_req_t) + CTRL_TRANSFER_DATA_MAX_BYTES, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    urb_t *urb_ctrl = heap_caps_calloc(1, sizeof(urb_t), MALLOC_CAP_DEFAULT);
    //Initialize URB and underlying transfer structure. Need to cast to dummy due to const fields
    usb_transfer_dummy_t *transfer_dummy = (usb_transfer_dummy_t *)&urb_ctrl->transfer;
    transfer_dummy->data_buffer = data_buffer;
    //STD: Set ADDR
    cdc_enum_stage_t stage = ENUM_STAGE_SET_ADDR;
    USB_CTRL_REQ_INIT_SET_ADDR((usb_ctrl_req_t *)data_buffer, 1);
    urb_ctrl->transfer.num_bytes = 0; //No data stage
    //Enqueue it
    ESP_LOGI(TAG, "1. Set Device Addr = %u", 1);
    ESP_ERROR_CHECK(hcd_urb_enqueue(dflt_pipe_hdl, urb_ctrl));

    while (!(xEventGroupGetBits(s_usb_event_group) & DFLT_PIPE_TASK_KILL_BIT)) {
        if (xQueueReceive(dflt_pipe_evt_queue, &pipe_evt, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        switch (pipe_evt) {
            case HCD_PIPE_EVENT_URB_DONE: {
                //Dequeue the URB
                urb_t *done_urb = hcd_urb_dequeue(dflt_pipe_hdl);
                assert(done_urb == urb_ctrl);

                if (stage == ENUM_STAGE_SET_ADDR) {
                    ESP_LOGI(TAG, "Set Device Addr Done");
                    hcd_pipe_update_mps(dflt_pipe_hdl, EP0_MPS);
                    hcd_pipe_update_dev_addr(dflt_pipe_hdl, 1);
                    //Goto set config stage
                    stage = ENUM_STAGE_SET_CONFIG;
                    USB_CTRL_REQ_INIT_SET_CONFIG((usb_ctrl_req_t *)data_buffer, 1);//only one configuration
                    done_urb->transfer.num_bytes = 0; //No data stage
                    ESP_LOGI(TAG, "2. Sending set_config = %u", 1);
                    ESP_ERROR_CHECK(hcd_urb_enqueue(dflt_pipe_hdl, done_urb));
                } else if (stage == ENUM_STAGE_SET_CONFIG) {
                    ESP_LOGI(TAG, "Config is set");
#ifdef CONFIG_CDC_SEND_DTE_ACTIVE
                    //Set DTE active
                    stage = ENUM_STAGE_SET_LINE_STATE;
                    USB_CTRL_REQ_CDC_SET_LINE_STATE((usb_ctrl_req_t *)data_buffer, 0, 1, 0);
                    done_urb->transfer.num_bytes = 0; //No data stage
                    ESP_LOGI(TAG, "3. Sending set_line state itf= %u dtr=%d rts=%d", 0, 1, 0);
                    ESP_ERROR_CHECK(hcd_urb_enqueue(dflt_pipe_hdl, done_urb));
                } else if (stage == ENUM_STAGE_SET_LINE_STATE) {
                    ESP_LOGI(TAG, "Line state is set");
#else
                    xTaskNotifyGive(s_bulk_out_pipe_task_hdl);
                    xTaskNotifyGive(s_bulk_in_pipe_task_hdl);
#endif
                } else {
                    ESP_LOGW(TAG, "line %u Pipe: default undefined stage", __LINE__);
                }

                break;
            }

            case HCD_PIPE_EVENT_INVALID:
                ESP_LOGW(TAG, "line %u Pipe: default HCD_PIPE_EVENT_INVALID", __LINE__);
                break;

            case HCD_PIPE_EVENT_ERROR_XFER:
                ESP_LOGW(TAG, "line %u Pipe: default HCD_PIPE_EVENT_ERROR_XFER", __LINE__);
                break;

            case HCD_PIPE_EVENT_ERROR_URB_NOT_AVAIL:
                ESP_LOGW(TAG, "line %u Pipe: default HCD_PIPE_EVENT_ERROR_URB_NOT_AVAIL", __LINE__);
                break;

            case HCD_PIPE_EVENT_ERROR_OVERFLOW:
                ESP_LOGW(TAG, "line %u Pipe: default HCD_PIPE_EVENT_ERROR_OVERFLOW", __LINE__);
                break;

            case HCD_PIPE_EVENT_ERROR_STALL:
                ESP_LOGW(TAG, "line %u Pipe: default HCD_PIPE_EVENT_ERROR_STALL", __LINE__);
                break;

            case HCD_PIPE_EVENT_NONE:
                break;

            default:
                ESP_LOGE(TAG, "line %u invalid HCD_PORT_EVENT%d", __LINE__, pipe_evt);
                assert(0);
                break;
        }
    }

    ESP_LOGW(TAG, "default task will be deleted");
    xEventGroupClearBits(s_usb_event_group, DFLT_PIPE_TASK_KILL_BIT);
    hcd_pipe_command(dflt_pipe_hdl, HCD_PIPE_CMD_HALT);
    hcd_pipe_free(dflt_pipe_hdl);
    free(urb_ctrl->transfer.data_buffer);
    free(urb_ctrl);
    vQueueDelete(dflt_pipe_evt_queue);
    vTaskDelete(NULL);
}

static void bulk_out_pipe_task(void *arg)
{
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    //Create a queue for pipe callback to queue up pipe events
    hcd_pipe_event_t pipe_evt;
    QueueHandle_t pipe_evt_queue = xQueueCreate(EVENT_QUEUE_LEN, sizeof(hcd_pipe_event_t));
    assert(NULL != pipe_evt_queue);
    ESP_LOGI(TAG, "Creating bulk out pipe\n");
    hcd_pipe_config_t pipe_config = {
        .callback = pipe_callback,
        .callback_arg = (void *)pipe_evt_queue,
        .context = (void *)pipe_evt_queue,
        .ep_desc = &s_bulk_out_ep,
        .dev_addr = 1,
        .dev_speed = s_port_speed,
    };
    hcd_pipe_handle_t pipe_hdl;
    hcd_pipe_alloc(s_port_hdl, &pipe_config, &pipe_hdl);
    assert(pipe_hdl != NULL);
    size_t num_bytes_to_send = 0;
    urb_t *urb_out[BULK_OUT_URB_NUM] = {NULL};

    for (size_t i = 0; i < BULK_OUT_URB_NUM; i++) {
        urb_t *urb = heap_caps_calloc(1, sizeof(urb_t), MALLOC_CAP_INTERNAL);
        assert(NULL != urb);
        urb_out[i] = urb;
        uint8_t *data_buffer = heap_caps_malloc(BUFFER_SIZE_BULK_OUT, MALLOC_CAP_DMA);
        assert(NULL != data_buffer);
        //Initialize URB and underlying transfer structure. Need to cast to dummy due to const fields
        usb_transfer_dummy_t *transfer_dummy = (usb_transfer_dummy_t *)&urb->transfer;
        transfer_dummy->data_buffer = data_buffer;

    #ifdef CONFIG_CDC_SYNC_WITH_AT
        num_bytes_to_send = 4;
        memcpy(data_buffer, "AT\r\n", num_bytes_to_send); //check AT respond for some modem
    #endif
        urb->transfer.num_bytes = num_bytes_to_send;
        hcd_urb_enqueue(pipe_hdl, urb);
        ESP_LOGV(TAG, "ST %d: %.*s", urb->transfer.num_bytes, urb->transfer.num_bytes, urb->transfer.data_buffer);
    }

    while (!(xEventGroupGetBits(s_usb_event_group) & BULK_OUT_PIPE_TASK_KILL_BIT)) {
        if (xQueueReceive(pipe_evt_queue, &pipe_evt, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        switch (pipe_evt) {
            case HCD_PIPE_EVENT_URB_DONE: {
                urb_t *done_urb = hcd_urb_dequeue(pipe_hdl);

                if (done_urb->transfer.status != USB_TRANSFER_STATUS_COMPLETED) {
                    /* retry if transfer not completed */
                    hcd_urb_enqueue(pipe_hdl, done_urb);
                    continue;
                }

                ESP_LOGV(TAG, "ST actual = %d", done_urb->transfer.actual_num_bytes);
                /* send next data payload */
                esp_err_t ret = ESP_FAIL;

                while (ESP_OK != ret) {
                    ret = usb_out_ringbuf_pop(done_urb->transfer.data_buffer, BUFFER_SIZE_BULK_OUT, &num_bytes_to_send, 50 / portTICK_PERIOD_MS);

                    if (xEventGroupGetBits(s_usb_event_group) & BULK_OUT_PIPE_TASK_KILL_BIT) {
                        ret = ESP_FAIL;
                        num_bytes_to_send = 0;
                        break;
                    }
                }

                if (ESP_OK == ret) {
                    done_urb->transfer.num_bytes = num_bytes_to_send;
#ifdef CONFIG_CDC_USE_TRACE_FACILITY
                    s_bulkbuf_out_max = num_bytes_to_send>s_bulkbuf_out_max?num_bytes_to_send:s_bulkbuf_out_max;
#endif
                    hcd_urb_enqueue(pipe_hdl, done_urb);
                    ESP_LOGV(TAG, "ST %d: %.*s", done_urb->transfer.num_bytes, done_urb->transfer.num_bytes, done_urb->transfer.data_buffer);
                } else {
                    ESP_LOGE(TAG, "buil_out skip enqueue urb");
                }

                break;
            }

            case HCD_PIPE_EVENT_INVALID:
                ESP_LOGW(TAG, "line %u Pipe: bulk_out HCD_PIPE_EVENT_INVALID", __LINE__);
                break;

            case HCD_PIPE_EVENT_ERROR_XFER:
                ESP_LOGW(TAG, "line %u Pipe: bulk_out HCD_PIPE_EVENT_ERROR_XFER", __LINE__);
                break;

            case HCD_PIPE_EVENT_ERROR_URB_NOT_AVAIL:
                ESP_LOGW(TAG, "line %u Pipe: bulk_out HCD_PIPE_EVENT_ERROR_URB_NOT_AVAIL", __LINE__);
                break;

            case HCD_PIPE_EVENT_ERROR_OVERFLOW:
                ESP_LOGW(TAG, "line %u Pipe: bulk_out HCD_PIPE_EVENT_ERROR_OVERFLOW", __LINE__);
                break;

            case HCD_PIPE_EVENT_ERROR_STALL:
                ESP_LOGW(TAG, "line %u Pipe: bulk_out HCD_PIPE_EVENT_ERROR_STALL", __LINE__);
                break;

            case HCD_PIPE_EVENT_NONE:
                break;

            default:
                ESP_LOGE(TAG, "line %u invalid HCD_PORT_EVENT%d", __LINE__, pipe_evt);
                break;
        }

    }

    ESP_LOGW(TAG, "buil_out task will be deleted");
    xEventGroupClearBits(s_usb_event_group, BULK_OUT_PIPE_TASK_KILL_BIT);
    hcd_pipe_command(pipe_hdl, HCD_PIPE_CMD_HALT);
    hcd_pipe_free(pipe_hdl);
    for (size_t i = 0; i < BULK_OUT_URB_NUM; i++) {
        free(urb_out[i]->transfer.data_buffer);
        free(urb_out[i]);
    }
    vQueueDelete(pipe_evt_queue);
    vTaskDelete(NULL);
}

static void bulk_in_pipe_task(void *arg)
{
    usbh_cdc_config_t *cdc_config = (usbh_cdc_config_t *)(arg);
    usbh_cdc_cb_t rx_cb = cdc_config->rx_callback;
    void *rx_cb_arg= cdc_config->rx_callback_arg;

    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    //Create a queue for pipe callback to queue up pipe events
    hcd_pipe_event_t pipe_evt;
    QueueHandle_t pipe_evt_queue = xQueueCreate(EVENT_QUEUE_LEN, sizeof(hcd_pipe_event_t));
    assert(NULL != pipe_evt_queue);
    ESP_LOGI(TAG, "Creating bulk in pipe\n");
    hcd_pipe_config_t pipe_config = {
        .callback = pipe_callback,
        .callback_arg = (void *)pipe_evt_queue,
        .context = (void *)pipe_evt_queue,
        .ep_desc = &s_bulk_in_ep,
        .dev_addr = 1,
        .dev_speed = s_port_speed,
    };
    hcd_pipe_handle_t pipe_hdl;
    hcd_pipe_alloc(s_port_hdl, &pipe_config, &pipe_hdl);
    assert(pipe_hdl != NULL);

    urb_t *urb_in[BULK_IN_URB_NUM] = {NULL};

    for (size_t i = 0; i < BULK_IN_URB_NUM; i++) {
        urb_t *urb = heap_caps_calloc(1, sizeof(urb_t), MALLOC_CAP_INTERNAL);
        assert(NULL != urb);
        urb_in[i] = urb;
        uint8_t *data_buffer = heap_caps_malloc(BUFFER_SIZE_BULK_IN, MALLOC_CAP_DMA);
        assert(NULL != data_buffer);
        //Initialize URB and underlying transfer structure. Need to cast to dummy due to const fields
        usb_transfer_dummy_t *transfer_dummy = (usb_transfer_dummy_t *)&urb->transfer;
        transfer_dummy->data_buffer = data_buffer;
        transfer_dummy->num_bytes = BUFFER_SIZE_BULK_IN;
        hcd_urb_enqueue(pipe_hdl, urb);
    }

    while (!(xEventGroupGetBits(s_usb_event_group) & BULK_IN_PIPE_TASK_KILL_BIT)) {
        if (xQueueReceive(pipe_evt_queue, &pipe_evt, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        switch (pipe_evt) {
            case HCD_PIPE_EVENT_URB_DONE: {
                urb_t *done_urb = hcd_urb_dequeue(pipe_hdl);
                ESP_LOGV(TAG, "RCV actual %d: %.*s", done_urb->transfer.actual_num_bytes, done_urb->transfer.actual_num_bytes, done_urb->transfer.data_buffer);

                if (done_urb->transfer.actual_num_bytes > 0) {
                    esp_err_t ret = usb_in_ringbuf_push(done_urb->transfer.data_buffer, done_urb->transfer.actual_num_bytes, portMAX_DELAY);
#ifdef CONFIG_CDC_USE_TRACE_FACILITY
                    s_bulkbuf_in_max = done_urb->transfer.actual_num_bytes>s_bulkbuf_in_max?done_urb->transfer.actual_num_bytes:s_bulkbuf_in_max;
#endif
                    if (ret != ESP_OK) {
                        ESP_LOGW(TAG, "skip a usb payload");
                    }

#ifdef CONFIG_CDC_SYNC_WITH_AT
                    xEventGroupSetBits(s_usb_event_group, USB_TASK_INIT_COMPLETED_BIT);
#endif
                    if (rx_cb) {
                        rx_cb(rx_cb_arg);
                    }
                }

                hcd_urb_enqueue(pipe_hdl, done_urb);
                break;
            }

            case HCD_PIPE_EVENT_INVALID:
                ESP_LOGW(TAG, "line %u Pipe: in HCD_PIPE_EVENT_INVALID", __LINE__);
                break;

            case HCD_PIPE_EVENT_ERROR_XFER:
                ESP_LOGW(TAG, "line %u Pipe: in HCD_PIPE_EVENT_ERROR_XFER", __LINE__);
                break;

            case HCD_PIPE_EVENT_ERROR_URB_NOT_AVAIL:
                ESP_LOGW(TAG, "line %u Pipe: in HCD_PIPE_EVENT_ERROR_URB_NOT_AVAIL", __LINE__);
                break;

            case HCD_PIPE_EVENT_ERROR_OVERFLOW:
                ESP_LOGW(TAG, "line %u Pipe: in HCD_PIPE_EVENT_ERROR_OVERFLOW", __LINE__);
                break;

            case HCD_PIPE_EVENT_ERROR_STALL:
                ESP_LOGW(TAG, "line %u Pipe: in HCD_PIPE_EVENT_ERROR_STALL", __LINE__);
                break;

            case HCD_PIPE_EVENT_NONE:
                break;

            default:
                ESP_LOGE(TAG, "line %u invalid HCD_PORT_EVENT%d", __LINE__, pipe_evt);
                break;
        }
    }

    ESP_LOGW(TAG, "bulk_in task will be deleted");
    xEventGroupClearBits(s_usb_event_group, BULK_IN_PIPE_TASK_KILL_BIT);
    hcd_pipe_command(pipe_hdl, HCD_PIPE_CMD_HALT);
    hcd_pipe_free(pipe_hdl);
    for (size_t i = 0; i < BULK_IN_URB_NUM; i++) {
        free(urb_in[i]->transfer.data_buffer);
        free(urb_in[i]);
    }
    vQueueDelete(pipe_evt_queue);
    vTaskDelete(NULL);
}

esp_err_t usbh_cdc_driver_install(const usbh_cdc_config_t *config)
{
    if (config == NULL || config->bulk_in_ep == NULL || config->bulk_out_ep == NULL
            || config->rx_buffer_size == 0 || config->rx_buffer_size == 0) {
        ESP_LOGW(TAG, "Invalid Args");
        return ESP_ERR_INVALID_ARG;
    }

    s_bulk_in_ep = *(config->bulk_in_ep);
    s_bulk_out_ep = *(config->bulk_out_ep);
#ifdef CONFIG_CDC_USE_TRACE_FACILITY
    s_ringbuf_in_size = config->rx_buffer_size;
    s_ringbuf_out_size = config->tx_buffer_size;
#endif
    if (usb_driver_is_init()) {
        ESP_LOGW(TAG, "USB Driver has inited");
        return ESP_ERR_INVALID_STATE;
    }

    s_usb_event_group = xEventGroupCreate();

    if (s_usb_event_group == NULL) {
        ESP_LOGE(TAG, "Creation event group error");
        return ESP_FAIL;
    }

    in_ringbuf_handle = xRingbufferCreate(config->rx_buffer_size, RINGBUF_TYPE_BYTEBUF);

    if (in_ringbuf_handle == NULL) {
        ESP_LOGE(TAG, "Creation buffer error");
        return ESP_FAIL;
    }

    out_ringbuf_handle = xRingbufferCreate(config->tx_buffer_size, RINGBUF_TYPE_BYTEBUF);

    if (out_ringbuf_handle == NULL) {
        ESP_LOGE(TAG, "Creation buffer error");
        return ESP_FAIL;
    }

    usb_read_mux = xSemaphoreCreateMutex();

    if (usb_read_mux == NULL) {
        ESP_LOGE(TAG, "Creation mutex error");
        return ESP_FAIL;
    }

    usb_write_mux = xSemaphoreCreateMutex();

    if (usb_write_mux == NULL) {
        ESP_LOGE(TAG, "Creation mutex error");
        return ESP_FAIL;
    }

    in_ringbuf_mux = xSemaphoreCreateMutex();

    if (in_ringbuf_mux == NULL) {
        ESP_LOGE(TAG, "Creation mutex error");
        return ESP_FAIL;
    }

    out_ringbuf_mux = xSemaphoreCreateMutex();

    if (out_ringbuf_mux == NULL) {
        ESP_LOGE(TAG, "Creation mutex error");
        return ESP_FAIL;
    }

    xTaskCreatePinnedToCore(bulk_in_pipe_task, BULK_IN_PIPE_TASK_NAME, BULK_IN_PIPE_TASK_STACK_SIZE, (void*)config,
                            BULK_IN_PIPE_TASK_PRIORITY, &s_bulk_in_pipe_task_hdl, BULK_IN_PIPE_TASK_CORE);
    xTaskCreatePinnedToCore(bulk_out_pipe_task, BULK_OUT_PIPE_TASK_NAME, BULK_OUT_PIPE_TASK_STACK_SIZE, NULL,
                            BULK_OUT_PIPE_TASK_PRIORITY, &s_bulk_out_pipe_task_hdl, BULK_OUT_PIPE_TASK_CORE);
    xTaskCreatePinnedToCore(dflt_pipe_task, DFLT_PIPE_TASK_NAME, DFLT_PIPE_TASK_STACK_SIZE, NULL,
                            DFLT_PIPE_TASK_PRIORITY, &s_dflt_pipe_task_hdl, DFLT_PIPE_TASK_CORE);
    xTaskCreatePinnedToCore(port_task, PORT_TASK_NAME, PORT_TASK_STACK_SIZE, NULL,
                            PORT_TASK_PRIORITY, &s_port_task_hdl, PORT_TASK_CORE);

    //Start the port task
    xTaskNotifyGive(s_port_task_hdl);

#ifdef CONFIG_CDC_SYNC_WITH_AT
    ESP_LOGI(TAG, "Waitting 4G_moudle AT respond...");
    while (!(xEventGroupGetBits(s_usb_event_group) & USB_TASK_INIT_COMPLETED_BIT)) {
        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
    ESP_LOGI(TAG, "4G_moudle AT responded !");
#else
    xEventGroupSetBits(s_usb_event_group, USB_TASK_INIT_COMPLETED_BIT);
#endif

    ESP_LOGI(TAG, "usb driver install succeed");
    return ESP_OK;
}

esp_err_t usbh_cdc_driver_delete(void)
{
    if (!usb_driver_is_init()) {
        return ESP_ERR_INVALID_STATE;
    }

    xEventGroupClearBits(s_usb_event_group, USB_TASK_INIT_COMPLETED_BIT);
    xEventGroupSetBits(s_usb_event_group, USB_TASK_PIPES_KILL_BITS);
    hcd_port_command(s_port_hdl, HCD_PORT_CMD_POWER_OFF);
    xEventGroupSetBits(s_usb_event_group, PORT_TASK_KILL_BIT);

    vRingbufferDelete(in_ringbuf_handle);
    in_ringbuf_handle = NULL;
    vRingbufferDelete(out_ringbuf_handle);
    out_ringbuf_handle = NULL;
    vSemaphoreDelete(usb_read_mux);
    usb_read_mux = NULL;
    vSemaphoreDelete(usb_write_mux);
    usb_write_mux = NULL;
    vSemaphoreDelete(in_ringbuf_mux);
    in_ringbuf_mux = NULL;
    vSemaphoreDelete(out_ringbuf_mux);
    out_ringbuf_mux = NULL;

    while (xEventGroupGetBits(s_usb_event_group) & (USB_TASK_PIPES_KILL_BITS | PORT_TASK_KILL_BIT)) {
        ESP_LOGW(TAG, "Waitting for USB Driver uninstall");
        vTaskDelay(50 / portTICK_PERIOD_MS);
    }

    vEventGroupDelete(s_usb_event_group);
    s_usb_event_group = NULL;
    ESP_LOGW(TAG, "Delete Full USB Driver!");
    return ESP_OK;
}

esp_err_t usbh_cdc_get_buffered_data_len(size_t *size)
{
    if (!usb_driver_is_init() || xSemaphoreTake(in_ringbuf_mux, portMAX_DELAY) != pdTRUE) {
        return ESP_FAIL;
    }

    *size = s_in_buffered_data_len;
    xSemaphoreGive(in_ringbuf_mux);
    return ESP_OK;
}

int usbh_cdc_read_bytes(uint8_t *buf, size_t length, TickType_t ticks_to_wait)
{
    size_t read_sz = 0;
    int rx_data_size = 0;

    if (!usb_driver_is_init() || xSemaphoreTake(usb_read_mux, ticks_to_wait) != pdTRUE) {
        return -1;
    }

    esp_err_t res = usb_in_ringbuf_pop(buf, length, &read_sz, ticks_to_wait);

    if (res != ESP_OK) {
        xSemaphoreGive(usb_read_mux);
        return -1;
    }

    rx_data_size = read_sz;

    /* Buffer's data can be wrapped, at that situations we should make another retrievement */
    if (usb_in_ringbuf_pop(buf + read_sz, length - read_sz, &read_sz, 0) == ESP_OK) {
        rx_data_size += read_sz;
    }

    xSemaphoreGive(usb_read_mux);
    return rx_data_size;
}

int usbh_cdc_write_bytes(const uint8_t *buf, size_t length)
{
    int tx_data_size = 0;

    if (!usb_driver_is_init() || xSemaphoreTake(usb_write_mux, portMAX_DELAY) != pdTRUE) {
        return -1;
    }

    esp_err_t ret = usb_out_ringbuf_push(buf, length, portMAX_DELAY);

    if (ret != ESP_OK) {
        return -1;
    }

    tx_data_size = length;
    xSemaphoreGive(usb_write_mux);
    return tx_data_size;
}

#ifdef CONFIG_CDC_USE_TRACE_FACILITY
void usbh_cdc_print_buffer_msg(void)
{
    ESP_LOGI(TAG, "USBH CDC Transfer Buffer Dump:");
    ESP_LOGI(TAG, "usb transfer Buffer size, out = %d, in = %d", BUFFER_SIZE_BULK_OUT, BUFFER_SIZE_BULK_IN);
    ESP_LOGI(TAG, "usb transfer Max packet size, out = %d, in = %d\n", s_bulkbuf_out_max, s_bulkbuf_in_max);
    ESP_LOGI(TAG, "USBH CDC Ringbuffer Dump:");
    ESP_LOGI(TAG, "usb ringbuffer size, out = %d, in = %d", s_ringbuf_out_size, s_ringbuf_in_size);
    ESP_LOGI(TAG, "usb ringbuffer load, out = %d, in = %d", s_out_buffered_data_len, s_in_buffered_data_len);
    ESP_LOGI(TAG, "usb ringbuffer High water mark, out = %d, in = %d", s_ringbuf_out_max, s_ringbuf_in_max);
}
#endif