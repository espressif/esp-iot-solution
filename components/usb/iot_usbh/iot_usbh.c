/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_attr.h"
#include "esp_intr_alloc.h"
#include "esp_bit_defs.h"
#include "hcd.h"
#include "usb/usb_types_stack.h"
#include "usb_private.h"
#include "usb/usb_helpers.h"
#include "esp_private/usb_phy.h"
#include "iot_usbh.h"

#define ERR_CHECK(a, str, ret) if(!(a)) { \
        ESP_LOGE(TAG,"%s:%d (%s):%s", __FILE__, __LINE__, __FUNCTION__, str); \
        return (ret); \
    }

#define ERR_CHECK_GOTO(a, str, label) if(!(a)) { \
        ESP_LOGE(TAG,"%s:%d (%s):%s", __FILE__, __LINE__, __FUNCTION__, str); \
        goto label; \
    }

static const char *TAG = "IOT_USBH";

#define USB_ENUM_CONFIG_INDEX                0                                        /*! Index of the first configuration of the device */
#define USB_ENUM_SHORT_DESC_REQ_LEN          8                                        /*! Number of bytes to request when getting a short descriptor (just enough to get bMaxPacketSize0 or wTotalLength) */
#define USB_DEVICE_CONFIG                    1                                        /*! Default CDC device configuration */
#define USB_EP_CTRL_DEFAULT_MPS              64                                       /*! Default MPS(max payload size) of Endpoint 0 */
#define CTRL_TRANSFER_DATA_MAX_BYTES         CONFIG_CTRL_TRANSFER_DATA_MAX_BYTES      /*! Just assume that will only IN/OUT 256 bytes in ctrl pipe */
#define TIMEOUT_USBH_CTRL_XFER_MS            1000                                     /*! Timeout for USB control transfer */
#define TIMEOUT_USBH_DRIVER_MS               1000                                     /*! Timeout for driver operate */
#define TIMEOUT_USBH_CTRL_XFER_QUICK_MS      100                                      /*! Timeout for USB control transfer */
#define TIMEOUT_USBH_CONNECT_WAIT_MS         100                                      /*! Timeout for device ready */
#define TIMEOUT_USBH_PW_OFF_MS               100                                      /*! Timeout for device power off disconnect */
#define TIMEOUT_USBH_DRIVER_CHECK_MS         10                                       /*! Timeout for check step */

/**
 * @brief Task for USB I/O request and payload processing,
 * can not be blocked, higher task priority is suggested.
 *
 */
#define USBH_TASK_BASE_PRIORITY       CONFIG_USBH_TASK_BASE_PRIORITY        /*! USB Task base priority */
#define USBH_PROC_TASK_NAME           "usbh_proc"                           /*! USB processing task name, task to handle hot-plug */
#define USBH_PROC_TASK_PRIORITY       (USBH_TASK_BASE_PRIORITY + 3)         /*! USB processing task priority */
#define USBH_PROC_TASK_STACK_SIZE     3072                                  /*! USB processing task stack size */
#define USBH_PROC_TASK_CORE           CONFIG_USBH_TASK_CORE_ID              /*! USB processing task pin to core id */
#define USBH_TASK_KILL_BIT            BIT1                                  /*! Event bit to kill USB processing task */
#define USBH_TASK_SUSPEND_BIT         BIT2                                  /*! Event bit to suspend USB processing task */

/**
 * @brief USB Core state, maintained by the driver
 *
 */
typedef enum {
    USBH_STATE_NONE,
    USBH_STATE_CHECK,
    USBH_STATE_DRIVER_INSTALLED,
    USBH_STATE_DEVICE_NOT_ATTACHED,
    USBH_STATE_DEVICE_ATTACHED,
    USBH_STATE_DEVICE_POWERED,
    USBH_STATE_DEVICE_DEFAULT,
    USBH_STATE_DEVICE_ADDRESS,
    USBH_STATE_DEVICE_CONFIGURED,
    USBH_STATE_DEVICE_SUSPENDED,
} _usbh_state_t;

static _usbh_state_t _usbh_update_state(_usbh_state_t state)
{
    static _usbh_state_t device_state = USBH_STATE_NONE;
    if (state != USBH_STATE_CHECK) {
        device_state = state;
    }
    return device_state;
}

static _usbh_state_t _usbh_get_state()
{
    return _usbh_update_state(USBH_STATE_CHECK);
}

static bool _usbh_driver_installed()
{
    return (_usbh_get_state() > USBH_STATE_NONE);
}

/************************************************************* USB Host Port API ***********************************************************/
typedef struct {
    int port_num;                               /*!< USB port number assigned by driver */
    usb_speed_t dev_speed;                      /*!< device speed detected by driver */
    uint8_t dev_addr;                           /*!< device address assigned by driver */
    hcd_port_handle_t port_handle;              /*!< port handler */
    hcd_port_fifo_bias_t fifo_bias;             /*!< fifo bias strategy */
    usbh_pipe_handle_t dflt_pipe_handle;        /*!< default pipe handler */
    usb_phy_handle_t phy_handle;                /*!< USB phy handle */
    QueueHandle_t queue_handle;                 /*!< queue handle, using for event transmit */
    EventGroupHandle_t evt_group_handle;        /*!< event handle, using for event sync */
    TaskHandle_t usb_task_handle;               /*!< USB processing task handle */
    usbh_cb_t conn_callback;                    /*!< USB connect callback, set NULL if not use */
    usbh_cb_t disconn_callback;                 /*!< USB disconnect callback, set NULL if not use */
    usbh_evt_cb_t event_callback;               /*!< USB event(pipe/user) callback, set NULL if not use */
    void *conn_callback_arg;                    /*!< USB connect callback arg, set NULL if not use  */
    void *disconn_callback_arg;                 /*!< USB disconnect callback arg, set NULL if not use */
    void *event_callback_arg;                   /*!< USB event callback arg, set NULL if not use  */
} _usbh_port_t;

typedef struct {
    uint8_t ep_addr;                            /*!< endpoint address of the pipe */
    const usb_ep_desc_t *ep_desc;               /*!< endpoint descriptor using for pipe create */
    hcd_pipe_handle_t pipe_handle;              /*!< handle of the pipe */
    QueueHandle_t queue_handle;                 /*!< queue handle using for event transmit */
} _usbh_pipe_t;

static usbh_port_event_t _port_event_dflt_process(usbh_port_handle_t port_hdl)
{
    _usbh_port_t *port_instance = (_usbh_port_t *)port_hdl;
    usbh_port_event_t actual_evt = (usbh_port_event_t)hcd_port_handle_event(port_instance->port_handle);

    switch (actual_evt) {
    case PORT_EVENT_CONNECTION:
        ESP_LOGI(TAG, "line %u PORT_EVENT_CONNECTION", __LINE__);
        break;

    case PORT_EVENT_DISCONNECTION:
        ESP_LOGW(TAG, "line %u PORT_EVENT_DISCONNECTION", __LINE__);
        break;

    case PORT_EVENT_ERROR:
        ESP_LOGW(TAG, "line %u PORT_EVENT_ERROR", __LINE__);
        break;

    case PORT_EVENT_OVERCURRENT:
        ESP_LOGW(TAG, "line %u PORT_EVENT_OVERCURRENT", __LINE__);
        break;

    case PORT_EVENT_NONE:
        ESP_LOGD(TAG, "line %u PORT_EVENT_NONE", __LINE__);
        break;

    default:
        ESP_LOGE(TAG, "line %u invalid HCD_PORT_EVENT%d", __LINE__, actual_evt);
        break;
    }

    return actual_evt;
}

static void _pipe_event_dflt_process(usbh_pipe_handle_t pipe_handle, usbh_pipe_event_t pipe_event)
{
    switch (pipe_event) {
    case PIPE_EVENT_URB_DONE:
        ESP_LOGD(TAG, "line %u Pipe: default PIPE_EVENT_URB_DONE", __LINE__);
        break;

    case PIPE_EVENT_ERROR_XFER:
        ESP_LOGW(TAG, "line %u Pipe: default PIPE_EVENT_ERROR_XFER", __LINE__);
        break;

    case PIPE_EVENT_ERROR_URB_NOT_AVAIL:
        ESP_LOGW(TAG, "line %u Pipe: default PIPE_EVENT_ERROR_URB_NOT_AVAIL", __LINE__);
        break;

    case PIPE_EVENT_ERROR_OVERFLOW:
        ESP_LOGW(TAG, "line %u Pipe: default PIPE_EVENT_ERROR_OVERFLOW", __LINE__);
        break;

    case PIPE_EVENT_ERROR_STALL:
        ESP_LOGW(TAG, "line %u Pipe: default PIPE_EVENT_ERROR_STALL", __LINE__);
        break;

    case PIPE_EVENT_NONE:
        ESP_LOGD(TAG, "line %u Pipe: default PIPE_EVENT_NONE", __LINE__);
        break;

    default:
        ESP_LOGE(TAG, "line %u invalid HCD_PORT_EVENT%d", __LINE__, pipe_event);
        break;
    }
}

static esp_err_t iot_usbh_port_event_wait(usbh_port_handle_t port_hdl,
        usbh_port_event_t expected_event, TickType_t xTicksToWait)
{
    ERR_CHECK(port_hdl != NULL, "invalid args", ESP_ERR_INVALID_ARG);
    //Wait for port callback to send an event message
    _usbh_port_t *port_instance = (_usbh_port_t *)port_hdl;
    assert(port_instance->queue_handle != NULL);
    esp_err_t ret = ESP_ERR_NOT_FOUND;
    usbh_event_msg_t evt_msg;

    do {
        BaseType_t queue_ret = xQueueReceive(port_instance->queue_handle, &evt_msg, xTicksToWait);

        if (queue_ret != pdPASS) {
            break;
        }

        if (PIPE_EVENT == evt_msg._type) {
            _pipe_event_dflt_process(evt_msg._handle.pipe_handle, evt_msg._event.pipe_event);
            ret = ESP_ERR_NOT_FOUND;
        } else {
            if (port_hdl == evt_msg._handle.port_hdl && expected_event == evt_msg._event.port_event) {
                _port_event_dflt_process(evt_msg._handle.port_hdl);
                ret = ESP_OK;
            } else {
                ESP_LOGD(TAG, "Got unexpected port_handle and event");
                _port_event_dflt_process(evt_msg._handle.port_hdl);
                ret = ESP_ERR_INVALID_RESPONSE;
            }
        }
    } while (ret == ESP_ERR_NOT_FOUND);

    return ret;
}

/**
 * @brief HCD port callback. Registered when initializing an HCD port
 *
 * @param port_hdl Port handle
 * @param port_event Port event that triggered the callback
 * @param user_arg User argument
 * @param in_isr Whether callback was called in an ISR context
 * @return true ISR should yield after this callback returns
 * @return false No yield required (non-ISR context calls should always return false)
 */
static bool IRAM_ATTR _usbh_port_callback(hcd_port_handle_t port_hdl, hcd_port_event_t port_event, void *user_arg, bool in_isr)
{
    _usbh_port_t *port_instance = (_usbh_port_t *)user_arg;
    QueueHandle_t usb_event_queue = port_instance->queue_handle;
    usbh_event_msg_t event_msg = {
        ._type = PORT_EVENT,
        ._handle.port_hdl = (usbh_port_handle_t)port_instance,
        ._event.port_event = (usbh_port_event_t)port_event,
    };
    if (in_isr) {
        BaseType_t xTaskWoken = pdFALSE;
        xQueueSendFromISR(usb_event_queue, &event_msg, &xTaskWoken);
        return (xTaskWoken == pdTRUE);
    } else {
        xQueueSend(usb_event_queue, &event_msg, portMAX_DELAY);
        return false;
    }
}

static esp_err_t _usbh_port_reset(hcd_port_handle_t port_hdl)
{
    ERR_CHECK(port_hdl != NULL, "invalid port handle", ESP_ERR_INVALID_ARG);
    esp_err_t ret = hcd_port_command(port_hdl, HCD_PORT_CMD_RESET);
    ERR_CHECK(ESP_OK == ret, "Port reset failed", ESP_FAIL);
    ESP_LOGI(TAG, "Port reset succeed");
    return ret;
}

static esp_err_t _usbh_port_disable(hcd_port_handle_t port_hdl)
{
    ERR_CHECK(port_hdl != NULL, "invalid port handle", ESP_ERR_INVALID_ARG);
    esp_err_t ret = hcd_port_command(port_hdl, HCD_PORT_CMD_DISABLE);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Port disable failed");
    }
    return ret;
}

static esp_err_t _usbh_port_power(hcd_port_handle_t port_hdl, bool on)
{
    ERR_CHECK(port_hdl != NULL, "invalid port handle", ESP_ERR_INVALID_ARG);
    esp_err_t ret;
    if (on) {
        ret = hcd_port_command(port_hdl, HCD_PORT_CMD_POWER_ON);
    } else {
        ret = hcd_port_command(port_hdl, HCD_PORT_CMD_POWER_OFF);
    }
    ESP_LOGI(TAG, "Port power: %s %s", on ? "ON" : "OFF", ESP_OK == ret ? "Succeed" : "Failed");
    return ret;
}

static hcd_port_state_t _usbh_port_get_state(hcd_port_handle_t port_hdl)
{
    ERR_CHECK(port_hdl != NULL, "invalid args", ESP_ERR_INVALID_ARG);
    return hcd_port_get_state(port_hdl);
}

static esp_err_t _usbh_port_get_speed(hcd_port_handle_t port_hdl, usb_speed_t *port_speed)
{
    ERR_CHECK(port_hdl != NULL && port_speed != NULL, "invalid args", ESP_ERR_INVALID_ARG);
    esp_err_t ret = hcd_port_get_speed(port_hdl, port_speed);
    ERR_CHECK(ESP_OK == ret, "port get speed failed", ret);
    return ret;
}

static void _usbh_processing_task(void *arg);
/**
 * @brief Initialize USB port and default pipe
 *
 * @param context context can be get from port handle
 * @param callback  usb port event callback
 * @param callback_arg callback args
 * @return hcd_port_handle_t port handle if initialize succeed, null if failed
 */
usbh_port_handle_t iot_usbh_port_init(usbh_port_config_t *config)
{
    ERR_CHECK(_usbh_driver_installed() == false, "usb port has init", NULL);
    ESP_LOGI(TAG, "iot_usbh, version: %d.%d.%d", IOT_USBH_VER_MAJOR, IOT_USBH_VER_MINOR, IOT_USBH_VER_PATCH);
    ERR_CHECK(config != NULL, "invalid args", NULL);
    ERR_CHECK(config->queue_length != 0, "queue_length can not be 0", NULL);
    ERR_CHECK(config->port_num == 1, "port_num only supports 1", NULL);
    esp_err_t ret = ESP_OK;
    _usbh_port_t *port_instance = calloc(1, sizeof(_usbh_port_t));
    ERR_CHECK(port_instance != NULL, "calloc failed", NULL);
    port_instance->queue_handle = xQueueCreate(config->queue_length, sizeof(usbh_event_msg_t));
    ERR_CHECK(port_instance->queue_handle != NULL, "queue create failed", NULL);
    port_instance->evt_group_handle = xEventGroupCreate();
    ERR_CHECK(port_instance->evt_group_handle != NULL, "event group create failed", NULL);
    port_instance->port_num = config->port_num;
    port_instance->dev_addr = config->port_num;
    port_instance->fifo_bias = (hcd_port_fifo_bias_t)config->fifo_bias;
    port_instance->conn_callback = config->conn_callback;
    port_instance->disconn_callback = config->disconn_callback;
    port_instance->event_callback = config->event_callback;
    port_instance->conn_callback_arg = config->conn_callback_arg;
    port_instance->disconn_callback_arg = config->disconn_callback_arg;
    port_instance->event_callback_arg = config->event_callback_arg;
    /* Router internal USB PHY to usb-otg instead of usb-serial-jtag (if it has) */
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
    ret = usb_new_phy(&phy_config, &port_instance->phy_handle);
    ERR_CHECK(ESP_OK == ret, "USB PHY init failed", NULL);
    /* Initialize USB Peripheral */
    hcd_config_t hcd_config = {
        .intr_flags = config->intr_flags,
    };
    ret = hcd_install(&hcd_config);
    ERR_CHECK_GOTO(ESP_OK == ret, "HCD Install failed", hcd_init_err);

    /* Initialize USB Port */
    hcd_port_config_t port_cfg = {
        .fifo_bias = port_instance->fifo_bias,
        .callback = _usbh_port_callback,
        .callback_arg = (void *)port_instance,
        .context = (void *)config->context,
    };
    ret = hcd_port_init(config->port_num, &port_cfg, &port_instance->port_handle);
    ERR_CHECK_GOTO(ESP_OK == ret, "HCD Port init failed", port_init_err);
    xTaskCreatePinnedToCore(_usbh_processing_task, USBH_PROC_TASK_NAME, USBH_PROC_TASK_STACK_SIZE, (void *)port_instance,
                            USBH_PROC_TASK_PRIORITY, &port_instance->usb_task_handle, USBH_PROC_TASK_CORE);
    ERR_CHECK_GOTO(port_instance->usb_task_handle != NULL, "Create usb task failed", port_init_err);
    _usbh_update_state(USBH_STATE_DRIVER_INSTALLED);
    ESP_LOGI(TAG, "USB Port=%d init succeed, fifo strategy=%d", config->port_num, config->fifo_bias);
    return port_instance;

port_init_err:
    hcd_uninstall();
hcd_init_err:
    vQueueDelete(port_instance->queue_handle);
    usb_del_phy(port_instance->phy_handle);
    free(port_instance);
    return NULL;
}

esp_err_t iot_usbh_port_start(usbh_port_handle_t port_hdl)
{
    ERR_CHECK(port_hdl != NULL, "invalid port handle", ESP_ERR_INVALID_ARG);
    ERR_CHECK(_usbh_driver_installed() == true, "usb port not init", ESP_ERR_INVALID_STATE);
    _usbh_port_t *port_instance = (_usbh_port_t *)port_hdl;
    xEventGroupClearBits(port_instance->evt_group_handle, USBH_TASK_SUSPEND_BIT);
    xTaskNotifyGive(port_instance->usb_task_handle);
    ESP_LOGI(TAG, "usb port start succeed");
    return ESP_OK;
}

esp_err_t iot_usbh_port_stop(usbh_port_handle_t port_hdl)
{
    ERR_CHECK(port_hdl != NULL, "invalid port handle", ESP_ERR_INVALID_ARG);
    ERR_CHECK(_usbh_driver_installed() == true, "usb port not init", ESP_ERR_INVALID_STATE);
    _usbh_port_t *port_instance = (_usbh_port_t *)port_hdl;
    xEventGroupSetBits(port_instance->evt_group_handle, USBH_TASK_SUSPEND_BIT);
    ESP_LOGW(TAG, "usb port suspended %d", port_instance->port_num);
    return ESP_OK;
}

esp_err_t iot_usbh_port_deinit(usbh_port_handle_t port_hdl)
{
    ERR_CHECK(port_hdl != NULL, "invalid port handle", ESP_ERR_INVALID_ARG);
    ERR_CHECK(_usbh_driver_installed() == true, "usb port not init", ESP_ERR_INVALID_STATE);
    _usbh_port_t *port_instance = (_usbh_port_t *)port_hdl;

    xEventGroupSetBits(port_instance->evt_group_handle, USBH_TASK_KILL_BIT);
    ESP_LOGW(TAG, "Waiting for USB Host task Delete");
    int counter = TIMEOUT_USBH_DRIVER_MS;
    while (xEventGroupGetBits(port_instance->evt_group_handle) & USBH_TASK_KILL_BIT) {
        vTaskDelay(pdMS_TO_TICKS(TIMEOUT_USBH_DRIVER_CHECK_MS));
        counter -= TIMEOUT_USBH_DRIVER_CHECK_MS;
        if (counter <= 0) {
            ESP_LOGW(TAG, "Waiting for USB Host task Delete, timeout");
            break;
        }
    }
    esp_err_t ret = _usbh_port_disable(port_instance->port_handle);
    if (ESP_OK != ret) {
        ESP_LOGW(TAG, "port disable failed");
    }
    _port_event_dflt_process(port_hdl);
    ret = _usbh_port_power(port_instance->port_handle, false);
    if (ESP_OK != ret) {
        ESP_LOGW(TAG, "Port power off failed");
    }
    iot_usbh_port_event_wait(port_instance, PORT_EVENT_DISCONNECTION, pdMS_TO_TICKS(TIMEOUT_USBH_PW_OFF_MS));
    ret = hcd_port_deinit(port_instance->port_handle);
    if (ESP_OK != ret) {
        ESP_LOGW(TAG, "port deinit failed");
    }
    ret = hcd_uninstall();
    if (ESP_OK != ret) {
        ESP_LOGW(TAG, "hcd uninstall failed");
    }
    ret = usb_del_phy(port_instance->phy_handle);
    if (ESP_OK != ret) {
        ESP_LOGW(TAG, "phy delete failed");
    }
    vQueueDelete(port_instance->queue_handle);
    vEventGroupDelete(port_instance->evt_group_handle);
    ESP_LOGI(TAG, "USB Port=%d deinit succeed", port_instance->port_num);
    free(port_instance);
    _usbh_update_state(USBH_STATE_NONE);
    return ret;
}

void *iot_usbh_port_get_context(usbh_port_handle_t port_hdl)
{
    ERR_CHECK(port_hdl != NULL, "invalid args", NULL);
    ERR_CHECK(_usbh_driver_installed() == true, "usb port not init", NULL);
    _usbh_port_t *port_instance = (_usbh_port_t *)port_hdl;
    return hcd_port_get_context(port_instance->port_handle);
}

/************************************************** USB Host PIPE API **************************************************/

/**
 * @brief HCD pipe callback. Registered when allocating a HCD pipe
 *
 * @param pipe_hdl Pipe handle
 * @param pipe_event Pipe event that triggered the callback
 * @param user_arg User argument
 * @param in_isr Whether the callback was called in an ISR context
 * @return true ISR should yield after this callback returns
 * @return false No yield required (non-ISR context calls should always return false)
 */
static bool IRAM_ATTR _usbh_pipe_callback(hcd_pipe_handle_t pipe_handle, hcd_pipe_event_t pipe_event, void *user_arg, bool in_isr)
{
    _usbh_pipe_t *pipe_instance = (_usbh_pipe_t *)user_arg;
    QueueHandle_t pipe_event_queue = pipe_instance->queue_handle;
    usbh_event_msg_t event_msg = {
        ._type = PIPE_EVENT,
        ._handle.pipe_handle = (usbh_pipe_handle_t)pipe_instance,
        ._event.pipe_event = (usbh_pipe_event_t)pipe_event,
    };

    if (in_isr) {
        BaseType_t xTaskWoken = pdFALSE;
        xQueueSendFromISR(pipe_event_queue, &event_msg, &xTaskWoken);
        return (xTaskWoken == pdTRUE);
    } else {
        xQueueSend(pipe_event_queue, &event_msg, portMAX_DELAY);
        return false;
    }
}

static esp_err_t iot_usbh_pipe_event_wait(usbh_pipe_handle_t pipe_hdl,
        usbh_pipe_event_t expected_event, TickType_t xTicksToWait)
{
    //Wait for pipe callback to send an event message
    ERR_CHECK(pipe_hdl != NULL, "invalid args", ESP_ERR_INVALID_ARG);
    _usbh_pipe_t *pipe_instance = (_usbh_pipe_t *)pipe_hdl;
    QueueHandle_t pipe_evt_queue = pipe_instance->queue_handle;
    assert(pipe_evt_queue != NULL);
    esp_err_t ret = ESP_ERR_NOT_FOUND;
    usbh_event_msg_t evt_msg;
    do {
        BaseType_t queue_ret = xQueueReceive(pipe_evt_queue, &evt_msg, xTicksToWait);

        if (queue_ret != pdPASS) {
            break;
        }

        if (PORT_EVENT == evt_msg._type) {
            _port_event_dflt_process(evt_msg._handle.port_hdl);
            ret = ESP_ERR_NOT_FOUND;
        } else {
            if (pipe_hdl == evt_msg._handle.pipe_handle && expected_event == evt_msg._event.pipe_event) {
                _pipe_event_dflt_process(evt_msg._handle.pipe_handle, evt_msg._event.pipe_event);
                ret = ESP_OK;
            } else {
                ESP_LOGD(TAG, "Got unexpected pipe_handle or event");
                _pipe_event_dflt_process(evt_msg._handle.pipe_handle, evt_msg._event.pipe_event);
                ret = ESP_ERR_INVALID_RESPONSE;
            }
        }
    } while (ret == ESP_ERR_NOT_FOUND);

    return ret;
}

usbh_pipe_handle_t iot_usbh_pipe_init(usbh_port_handle_t port_hdl, const usb_ep_desc_t *ep_desc, QueueHandle_t queue_hdl, void *context)
{
    ERR_CHECK(port_hdl != NULL, "invalid args", NULL);
    ERR_CHECK(_usbh_get_state() >= USBH_STATE_DEVICE_DEFAULT, "invalid usb state", NULL);
    _usbh_port_t *port_instance = (_usbh_port_t *)port_hdl;
    _usbh_pipe_t *pipe_instance = (_usbh_pipe_t *)calloc(1, sizeof(_usbh_pipe_t));
    ERR_CHECK(pipe_instance != NULL, "calloc failed", NULL);
    uint8_t dev_addr = port_instance->dev_addr;

    if (ep_desc) {
        pipe_instance->ep_addr = ep_desc->bEndpointAddress;
        pipe_instance->ep_desc = ep_desc;
    } else {
        // endpoint 0, default pipe
        pipe_instance->ep_addr = 0;
        pipe_instance->ep_desc = NULL;
        dev_addr = 0;
    }

    if (queue_hdl) {
        pipe_instance->queue_handle = queue_hdl;
    } else {
        // using same queue with usb port
        pipe_instance->queue_handle = port_instance->queue_handle;
    }

    hcd_pipe_config_t pipe_cfg = {
        .callback = _usbh_pipe_callback,
        .callback_arg = (void *)pipe_instance,
        .context = (void *)context,
        .ep_desc = pipe_instance->ep_desc,    // NULL EP descriptor to create a default pipe
        .dev_addr = dev_addr,
        .dev_speed = port_instance->dev_speed,
    };
    // Pipe context is queue handle by default
    if (context == NULL) {
        pipe_cfg.context = (void *)pipe_instance->queue_handle;
    }
    esp_err_t ret = hcd_pipe_alloc(port_instance->port_handle, &pipe_cfg, &pipe_instance->pipe_handle);
    ERR_CHECK(ESP_OK == ret, "pipe alloc failed", NULL);
    ESP_LOGI(TAG, "Pipe init succeed, addr: %02X", pipe_instance->ep_addr);
    return pipe_instance;
}

static esp_err_t iot_usbh_pipe_command(usbh_pipe_handle_t pipe_hdl, hcd_pipe_cmd_t command)
{
    ERR_CHECK(pipe_hdl != NULL, "invalid args", ESP_ERR_INVALID_ARG);
    ERR_CHECK(_usbh_driver_installed() == true, "usb port not init", ESP_ERR_INVALID_STATE);
    _usbh_pipe_t *pipe_instance = (_usbh_pipe_t *)pipe_hdl;
    return hcd_pipe_command(pipe_instance->pipe_handle, command);
}

esp_err_t iot_usbh_pipe_deinit(usbh_pipe_handle_t pipe_hdl)
{
    ERR_CHECK(pipe_hdl != NULL, "invalid args", ESP_ERR_INVALID_ARG);
    ERR_CHECK(_usbh_driver_installed() == true, "usb port not init", ESP_ERR_INVALID_STATE);
    _usbh_pipe_t *pipe_instance = (_usbh_pipe_t *)pipe_hdl;
    ESP_LOGI(TAG, "%s: pipe state = %d", __func__, hcd_pipe_get_state(pipe_instance->pipe_handle));

    esp_err_t ret = iot_usbh_pipe_command(pipe_hdl, HCD_PIPE_CMD_HALT);
    if (ESP_OK != ret) {
        ESP_LOGW(TAG, "pipe=%p halt failed", pipe_hdl);
    }

    ret = hcd_pipe_free(pipe_instance->pipe_handle);
    if (ESP_OK != ret) {
        ESP_LOGW(TAG, "pipe=%p free failed", pipe_hdl);
    }
    ESP_LOGI(TAG, "Pipe deinit succeed, addr: %02X", pipe_instance->ep_addr);
    free(pipe_instance);
    return ret;
}

void *iot_usbh_pipe_get_context(usbh_pipe_handle_t pipe_hdl)
{
    ERR_CHECK(pipe_hdl != NULL, "invalid args", NULL);
    ERR_CHECK(_usbh_driver_installed() == true, "usb port not init", NULL);
    _usbh_pipe_t *pipe_instance = (_usbh_pipe_t *)pipe_hdl;
    return hcd_pipe_get_context(pipe_instance->pipe_handle);
}

esp_err_t iot_usbh_pipe_flush(usbh_pipe_handle_t pipe_hdl, size_t urb_num)
{
    ERR_CHECK(pipe_hdl != NULL, "invalid args", ESP_ERR_INVALID_ARG);
    ERR_CHECK(_usbh_driver_installed() == true, "usb port not init", ESP_ERR_INVALID_STATE);
    _usbh_pipe_t *pipe_instance = (_usbh_pipe_t *)pipe_hdl;
    ESP_LOGD(TAG, "pipe state = %d", hcd_pipe_get_state(pipe_instance->pipe_handle));

    esp_err_t ret = iot_usbh_pipe_command(pipe_hdl, HCD_PIPE_CMD_HALT);
    if (ESP_OK != ret) {
        ESP_LOGW(TAG, "pipe=%p halt failed", pipe_hdl);
    }

    ret = iot_usbh_pipe_command(pipe_hdl, HCD_PIPE_CMD_FLUSH);
    if (ESP_OK != ret) {
        ESP_LOGW(TAG, "pipe=%p flush failed", pipe_hdl);
    }
    iot_usbh_pipe_event_wait(pipe_hdl, PIPE_EVENT_URB_DONE, pdMS_TO_TICKS(TIMEOUT_USBH_CTRL_XFER_QUICK_MS));

    for (size_t i = 0; i < urb_num; i++) {
        size_t transfer_num = 0;
        usb_transfer_status_t status = USB_TRANSFER_STATUS_ERROR;
        urb_t *urb = iot_usbh_urb_dequeue(pipe_hdl, &transfer_num, &status);
        ESP_LOGD(TAG, "urb dequeue handle = %p", urb);
    }

    ret = iot_usbh_pipe_command(pipe_hdl, HCD_PIPE_CMD_CLEAR);
    if (ESP_OK != ret) {
        ESP_LOGW(TAG, "pipe=%p clear failed", pipe_hdl);
    }

    return ret;
}

/*------------------------------------------------ USB URB Code ----------------------------------------------------*/
iot_usbh_urb_handle_t iot_usbh_urb_alloc(int num_isoc_packets, size_t packet_data_buffer_size, void *context)
{
    //Allocate list of URBS
    urb_t *urb = heap_caps_calloc(1, sizeof(urb_t) + (num_isoc_packets * sizeof(usb_isoc_packet_desc_t)), MALLOC_CAP_DEFAULT);

    if (NULL == urb) {
        ESP_LOGE(TAG, "urb alloc failed");
        return NULL;
    }

    //Allocate data buffer for each URB and assign them
    uint8_t *data_buffer = NULL;

    if (num_isoc_packets) {
        /* ISOC urb */
        data_buffer = heap_caps_calloc(num_isoc_packets, packet_data_buffer_size, MALLOC_CAP_DMA);
    } else {
        /* no ISOC urb */
        data_buffer = heap_caps_calloc(1, packet_data_buffer_size, MALLOC_CAP_DMA);
    }

    if (NULL == data_buffer) {
        heap_caps_free(urb);
        ESP_LOGE(TAG, "urb data_buffer alloc failed");
        return NULL;
    }

    //Initialize URB and underlying transfer structure. Need to cast to dummy due to const fields
    usb_transfer_dummy_t *transfer_dummy = (usb_transfer_dummy_t *)&urb->transfer;
    transfer_dummy->data_buffer = data_buffer;
    transfer_dummy->data_buffer_size = num_isoc_packets == 0 ? packet_data_buffer_size : (num_isoc_packets * packet_data_buffer_size);
    transfer_dummy->num_isoc_packets = num_isoc_packets;
    transfer_dummy->context = context;

    ESP_LOGD(TAG, "urb alloced");
    return (iot_usbh_urb_handle_t)urb;
}

esp_err_t iot_usbh_urb_free(iot_usbh_urb_handle_t urb_hdl)
{
    //Free data buffers of each URB
    ERR_CHECK(urb_hdl != NULL, "invalid args", ESP_ERR_INVALID_ARG);
    urb_t *_urb = (urb_t *)urb_hdl;
    heap_caps_free(_urb->transfer.data_buffer);
    //Free the URB list
    heap_caps_free(_urb);
    ESP_LOGD(TAG, "urb free");
    return ESP_OK;
}

void *iot_usbh_urb_buffer_claim(iot_usbh_urb_handle_t urb_hdl, size_t *buf_size, size_t *num_isoc)
{
    ERR_CHECK(urb_hdl != NULL, "invalid args", NULL);
    urb_t *_urb = (urb_t *)urb_hdl;
    if (buf_size) {
        *buf_size = _urb->transfer.data_buffer_size;
    }
    if (num_isoc) {
        *num_isoc = _urb->transfer.num_isoc_packets;
    }
    return _urb->transfer.data_buffer;
}

esp_err_t iot_usbh_urb_enqueue(usbh_pipe_handle_t pipe_hdl, iot_usbh_urb_handle_t urb_hdl, size_t xfer_size)
{
    ERR_CHECK(pipe_hdl != NULL && urb_hdl != NULL, "invalid args", ESP_ERR_INVALID_ARG);
    ERR_CHECK(_usbh_get_state() >= USBH_STATE_DEVICE_DEFAULT, "invalid usb state", ESP_ERR_INVALID_STATE);
    _usbh_pipe_t *pipe_instance = (_usbh_pipe_t *)pipe_hdl;
    urb_t *_urb = (urb_t *)urb_hdl;
    if (xfer_size != -1) {
        _urb->transfer.num_bytes = xfer_size;
    }
    esp_err_t ret = hcd_urb_enqueue(pipe_instance->pipe_handle, _urb);
    return ret;
}

iot_usbh_urb_handle_t iot_usbh_urb_dequeue(usbh_pipe_handle_t pipe_hdl, size_t *xfered_size, usb_transfer_status_t *status)
{
    ERR_CHECK(pipe_hdl != NULL, "invalid args", NULL);
    ERR_CHECK(_usbh_driver_installed() == true, "usb port not init", NULL);
    _usbh_pipe_t *pipe_instance = (_usbh_pipe_t *)pipe_hdl;
    urb_t *_urb = hcd_urb_dequeue(pipe_instance->pipe_handle);
    if (_urb == NULL) {
        if (xfered_size) {
            *xfered_size = 0;
        }
        return NULL;
    }
    if (xfered_size) {
        *xfered_size = _urb->transfer.actual_num_bytes;
    }
    if (status) {
        *status = _urb->transfer.status;
    }
    return (iot_usbh_urb_handle_t)_urb;
}

iot_usbh_urb_handle_t iot_usbh_urb_ctrl_xfer(usbh_port_handle_t port_hdl, iot_usbh_urb_handle_t urb_hdl, size_t xfer_size, size_t *xfered_size, usb_transfer_status_t *status)
{
    ERR_CHECK(port_hdl != NULL && urb_hdl != NULL, "invalid args", NULL);
    ERR_CHECK(_usbh_get_state() >= USBH_STATE_DEVICE_DEFAULT, "invalid usb state", NULL);
    _usbh_port_t *port_instance = (_usbh_port_t *)port_hdl;
    TaskHandle_t running_task = xTaskGetCurrentTaskHandle();
    if (running_task != port_instance->usb_task_handle) {
        ESP_LOGE(TAG, "ctrl_xfer can ONLY be run from usb callback");
        return NULL;
    }
    esp_err_t ret = iot_usbh_urb_enqueue(port_instance->dflt_pipe_handle, urb_hdl, xfer_size);
    ERR_CHECK(ESP_OK == ret, "urb enqueue failed", NULL);
    ret = iot_usbh_pipe_event_wait(port_instance->dflt_pipe_handle, PIPE_EVENT_URB_DONE, pdMS_TO_TICKS(TIMEOUT_USBH_CTRL_XFER_MS));
    ERR_CHECK(ESP_OK == ret, "urb event error", NULL);
    iot_usbh_urb_handle_t urb_done = iot_usbh_urb_dequeue(port_instance->dflt_pipe_handle, xfered_size, status);
    return urb_done;
}

/*------------------------------------------------ USB Control Process Code ----------------------------------------------------*/

#ifdef CONFIG_USBH_GET_DEVICE_DESC
static esp_err_t _usbh_get_dev_desc(usbh_pipe_handle_t pipe_handle, usb_device_desc_t *device_desc)
{
    ERR_CHECK(pipe_handle != NULL, "pipe_handle can't be NULL", ESP_ERR_INVALID_ARG);
    // malloc URB for default control
    urb_t *urb_ctrl = iot_usbh_urb_alloc(0, sizeof(usb_setup_packet_t) + CTRL_TRANSFER_DATA_MAX_BYTES, NULL);
    ERR_CHECK(urb_ctrl != NULL, "alloc urb failed", ESP_ERR_NO_MEM);
    urb_t *urb_done = NULL;
    usb_transfer_status_t status = USB_TRANSFER_STATUS_ERROR;

    // store setup request in URB
    USB_SETUP_PACKET_INIT_GET_DEVICE_DESC((usb_setup_packet_t *)urb_ctrl->transfer.data_buffer);
    size_t num_bytes = sizeof(usb_setup_packet_t) + usb_round_up_to_mps(sizeof(usb_device_desc_t), USB_EP_CTRL_DEFAULT_MPS);

    // enqueue URB in HCD, trigger a transmit
    ESP_LOGI(TAG, "get device desc");
    esp_err_t ret = iot_usbh_urb_enqueue(pipe_handle, urb_ctrl, num_bytes);
    ERR_CHECK_GOTO(ESP_OK == ret, "urb enqueue failed", free_urb_);

    // waiting for transmit done
    ret = iot_usbh_pipe_event_wait(pipe_handle, PIPE_EVENT_URB_DONE, pdMS_TO_TICKS(TIMEOUT_USBH_CTRL_XFER_MS));
    ERR_CHECK_GOTO(ESP_OK == ret, "urb event error", flush_urb_);

    // dequeue URB, check transmit result
    urb_done = iot_usbh_urb_dequeue(pipe_handle, &num_bytes, &status);
    ERR_CHECK_GOTO(urb_done == urb_ctrl, "urb status: not same", free_urb_);
    ERR_CHECK_GOTO(USB_TRANSFER_STATUS_COMPLETED == status, "urb status: not complete", free_urb_);
    ERR_CHECK_GOTO((num_bytes <= sizeof(usb_setup_packet_t) + sizeof(usb_device_desc_t)), "urb status: data overflow", free_urb_);

    // store device descriptor from transfer buffer
    ESP_LOGI(TAG, "get device desc, actual_num_bytes:%d", num_bytes);
    usb_device_desc_t *dev_desc = (usb_device_desc_t *)(urb_done->transfer.data_buffer + sizeof(usb_setup_packet_t));
    if (device_desc != NULL ) {
        *device_desc = *dev_desc;
    }
    usb_print_device_descriptor(dev_desc);
    goto free_urb_;

flush_urb_:
    iot_usbh_pipe_flush(pipe_handle, 1);
free_urb_:
    iot_usbh_urb_free(urb_ctrl);
    return ret;
}
#endif

#ifdef CONFIG_USBH_GET_CONFIG_DESC
static esp_err_t _usbh_get_config_desc(usbh_pipe_handle_t pipe_handle, usb_config_desc_t **config_desc)
{
    (void)config_desc;
    ERR_CHECK(pipe_handle != NULL, "pipe_handle can't be NULL", ESP_ERR_INVALID_ARG);

    // malloc URB for default control
    urb_t *urb_ctrl = iot_usbh_urb_alloc(0, sizeof(usb_setup_packet_t) + CTRL_TRANSFER_DATA_MAX_BYTES, NULL);
    ERR_CHECK(urb_ctrl != NULL, "alloc urb failed", ESP_ERR_NO_MEM);
    urb_t *urb_done = NULL;
    usb_transfer_status_t status = USB_TRANSFER_STATUS_ERROR;

    // store setup request in URB, got short config descriptor (used to get the full length)
    USB_SETUP_PACKET_INIT_GET_CONFIG_DESC((usb_setup_packet_t *)urb_ctrl->transfer.data_buffer, USB_ENUM_CONFIG_INDEX, USB_ENUM_SHORT_DESC_REQ_LEN);
    size_t num_bytes = sizeof(usb_setup_packet_t) + usb_round_up_to_mps(sizeof(usb_config_desc_t), USB_EP_CTRL_DEFAULT_MPS);

    // enqueue URB in HCD, trigger a transmit
    ESP_LOGI(TAG, "get short config desc");
    esp_err_t ret = iot_usbh_urb_enqueue(pipe_handle, urb_ctrl, num_bytes);
    ERR_CHECK_GOTO(ESP_OK == ret, "urb enqueue failed", free_urb_);

    // waiting for transmit done
    ret = iot_usbh_pipe_event_wait(pipe_handle, PIPE_EVENT_URB_DONE, pdMS_TO_TICKS(TIMEOUT_USBH_CTRL_XFER_MS));
    ERR_CHECK_GOTO(ESP_OK == ret, "urb event error", flush_urb_);

    // dequeue URB, check transmit result
    urb_done = iot_usbh_urb_dequeue(pipe_handle, &num_bytes, &status);
    ERR_CHECK_GOTO(urb_done == urb_ctrl, "urb status: not same", free_urb_);
    ERR_CHECK_GOTO(USB_TRANSFER_STATUS_COMPLETED == status, "urb status: not complete", free_urb_);
    ERR_CHECK_GOTO((num_bytes <= sizeof(usb_setup_packet_t) + sizeof(usb_config_desc_t)), "urb status: data overflow", free_urb_);

    // parse transmit result, get the full config descriptor
    ESP_LOGI(TAG, "get config desc, actual_num_bytes:%u", num_bytes);
    usb_config_desc_t *cfg_desc = (usb_config_desc_t *)(urb_done->transfer.data_buffer + sizeof(usb_setup_packet_t));
    uint16_t full_config_length = cfg_desc->wTotalLength;
    if (cfg_desc->wTotalLength > CTRL_TRANSFER_DATA_MAX_BYTES) {
        ESP_LOGE(TAG, "Configuration descriptor larger than control transfer max length");
        goto free_urb_;
    }

    // prepare the new URB to get full config descriptor
    ESP_LOGI(TAG, "get full config desc");
    USB_SETUP_PACKET_INIT_GET_CONFIG_DESC((usb_setup_packet_t *)urb_ctrl->transfer.data_buffer, USB_ENUM_CONFIG_INDEX, full_config_length);
    num_bytes = sizeof(usb_setup_packet_t) + usb_round_up_to_mps(full_config_length, USB_EP_CTRL_DEFAULT_MPS);
    ret = iot_usbh_urb_enqueue(pipe_handle, urb_ctrl, num_bytes);
    ERR_CHECK_GOTO(ESP_OK == ret, "urb enqueue failed", free_urb_);
    ret = iot_usbh_pipe_event_wait(pipe_handle, PIPE_EVENT_URB_DONE, pdMS_TO_TICKS(TIMEOUT_USBH_CTRL_XFER_MS));
    ERR_CHECK_GOTO(ESP_OK == ret, "urb event error", flush_urb_);
    urb_done = iot_usbh_urb_dequeue(pipe_handle, &num_bytes, &status);
    ERR_CHECK_GOTO(urb_done == urb_ctrl, "urb status: not same", free_urb_);
    ERR_CHECK_GOTO(USB_TRANSFER_STATUS_COMPLETED == status, "urb status: not complete", free_urb_);
    ERR_CHECK_GOTO((num_bytes <= sizeof(usb_setup_packet_t) + full_config_length), "urb status: data overflow", free_urb_);

    // parse, then printf config descriptor
    ESP_LOGI(TAG, "get full config desc, actual_num_bytes:%d", urb_done->transfer.actual_num_bytes);
    cfg_desc = (usb_config_desc_t *)(urb_done->transfer.data_buffer + sizeof(usb_setup_packet_t));
    usb_print_config_descriptor(cfg_desc, NULL);
    goto free_urb_;

flush_urb_:
    iot_usbh_pipe_flush(pipe_handle, 1);
free_urb_:
    iot_usbh_urb_free(urb_ctrl);
    return ret;
}
#endif

static esp_err_t _usbh_set_device_addr(usbh_pipe_handle_t pipe_handle, uint8_t dev_addr)
{
    ERR_CHECK(pipe_handle != NULL, "pipe_handle can't be NULL", ESP_ERR_INVALID_ARG);
    _usbh_pipe_t *pipe_instance = (_usbh_pipe_t *)pipe_handle;
    //malloc URB for default control
    urb_t *urb_ctrl = iot_usbh_urb_alloc(0, sizeof(usb_setup_packet_t), NULL);
    ERR_CHECK(urb_ctrl != NULL, "alloc urb failed", ESP_ERR_NO_MEM);

    // store setup request in URB
    USB_SETUP_PACKET_INIT_SET_ADDR((usb_setup_packet_t *)urb_ctrl->transfer.data_buffer, dev_addr);
    size_t num_bytes = sizeof(usb_setup_packet_t); //No data stage

    // enqueue URB in HCD, trigger a transmit
    ESP_LOGI(TAG, "Set Device Addr = %u", dev_addr);
    esp_err_t ret = iot_usbh_urb_enqueue(pipe_handle, urb_ctrl, num_bytes);
    ERR_CHECK_GOTO(ESP_OK == ret, "urb enqueue failed", free_urb_);

    // waiting for transmit done
    ret = iot_usbh_pipe_event_wait(pipe_handle, PIPE_EVENT_URB_DONE, pdMS_TO_TICKS(TIMEOUT_USBH_CTRL_XFER_MS));
    ERR_CHECK_GOTO(ESP_OK == ret, "urb event error", flush_urb_);
    usb_transfer_status_t status = USB_TRANSFER_STATUS_ERROR;

    // dequeue URB, check transmit result
    urb_t *urb_done = iot_usbh_urb_dequeue(pipe_handle, &num_bytes, &status);
    ERR_CHECK_GOTO(urb_done == urb_ctrl, "urb status: not same", free_urb_);
    ERR_CHECK_GOTO(USB_TRANSFER_STATUS_COMPLETED == status, "urb status: not complete", free_urb_);

    // config hcd based on result
    ret = hcd_pipe_update_dev_addr(pipe_instance->pipe_handle, dev_addr);
    ERR_CHECK_GOTO(ESP_OK == ret, "default pipe update address failed", free_urb_);
    ret = hcd_pipe_update_mps(pipe_instance->pipe_handle, USB_EP_CTRL_DEFAULT_MPS);
    ERR_CHECK_GOTO(ESP_OK == ret, "default pipe update MPS failed", free_urb_);
    ESP_LOGI(TAG, "Set Device Addr Done");
    goto free_urb_;

flush_urb_:
    iot_usbh_pipe_flush(pipe_handle, 1);
free_urb_:
    iot_usbh_urb_free(urb_ctrl);
    return ret;
}

static esp_err_t _usbh_set_device_config(usbh_pipe_handle_t pipe_handle, uint16_t configuration)
{
    ERR_CHECK(pipe_handle != NULL, "pipe_handle can't be NULL", ESP_ERR_INVALID_ARG);
    ERR_CHECK(configuration != 0, "configuration can't be 0", ESP_ERR_INVALID_ARG);

    // malloc URB for default control
    urb_t *urb_ctrl = iot_usbh_urb_alloc(0, sizeof(usb_setup_packet_t), NULL);
    ERR_CHECK(urb_ctrl != NULL, "alloc urb failed", ESP_ERR_NO_MEM);

    // store setup request in URB
    USB_SETUP_PACKET_INIT_SET_CONFIG((usb_setup_packet_t *)urb_ctrl->transfer.data_buffer, configuration);
    size_t num_bytes = sizeof(usb_setup_packet_t); //No data stage
    usb_transfer_status_t status = USB_TRANSFER_STATUS_ERROR;

    // enqueue URB in HCD, trigger a transmit
    ESP_LOGI(TAG, "Set Device Configuration = %u", configuration);
    esp_err_t ret = iot_usbh_urb_enqueue(pipe_handle, urb_ctrl, num_bytes);
    ERR_CHECK_GOTO(ESP_OK == ret, "urb enqueue failed", free_urb_);

    // waiting for transmit done
    ret = iot_usbh_pipe_event_wait(pipe_handle, PIPE_EVENT_URB_DONE, pdMS_TO_TICKS(TIMEOUT_USBH_CTRL_XFER_MS));
    ERR_CHECK_GOTO(ESP_OK == ret, "urb event error", flush_urb_);

    // dequeue URB, check transmit result
    urb_t *urb_done = iot_usbh_urb_dequeue(pipe_handle, &num_bytes, &status);
    ERR_CHECK_GOTO(urb_done == urb_ctrl, "urb status: not same", free_urb_);
    ERR_CHECK_GOTO(USB_TRANSFER_STATUS_COMPLETED == status, "urb status: not complete", free_urb_);
    ESP_LOGI(TAG, "Set Device Configuration Done");
    goto free_urb_;

flush_urb_:
    iot_usbh_pipe_flush(pipe_handle, 1);
free_urb_:
    iot_usbh_urb_free(urb_ctrl);
    return ret;
}

/**
 * @brief Handle usb port and default pipe event
 *
 * @param arg
 * @return ** void
 */
static void _usbh_processing_task(void *arg)
{
    _usbh_port_t *port_instance = (_usbh_port_t *)arg;
    QueueHandle_t usb_queue_hdl = port_instance->queue_handle;
    EventGroupHandle_t evt_group_hdl = port_instance->evt_group_handle;
    usbh_pipe_handle_t pipe_instance_dflt = NULL;
    esp_err_t ret = ESP_OK;
    usb_speed_t port_speed = 0;
    usbh_port_event_t port_actual_evt;
    usbh_event_msg_t evt_msg = {};
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    ESP_LOGI(TAG, "USB Processing Start");

    while (1) {
        /* handle usb hotplug */
        ESP_LOGI(TAG, "Waiting USB Connection");
        if (_usbh_port_get_state(port_instance->port_handle) == HCD_PORT_STATE_NOT_POWERED) {
            ret = _usbh_port_power(port_instance->port_handle, true);
            ERR_CHECK_GOTO(ESP_OK == ret, "Port Set POWER_ON failed", usb_driver_reset_);
        }
        _usbh_update_state(USBH_STATE_DEVICE_POWERED);
        while (!(xEventGroupGetBits(evt_group_hdl) & USBH_TASK_KILL_BIT)) {
            /* handle usb event */
            if (xEventGroupGetBits(evt_group_hdl) & USBH_TASK_SUSPEND_BIT || xQueueReceive(usb_queue_hdl, &evt_msg, 1) != pdTRUE) {
                vTaskDelay(1);
                continue;
            }

            switch (evt_msg._type) {
            case PORT_EVENT:
                port_actual_evt = _port_event_dflt_process((usbh_port_handle_t)port_instance);
                switch (port_actual_evt) {
                case PORT_EVENT_CONNECTION:
                    // wait least 100 ms to allow completion of an insertion process
                    vTaskDelay(pdMS_TO_TICKS(TIMEOUT_USBH_CONNECT_WAIT_MS));
                    // Reset port and get speed
                    ESP_LOGI(TAG, "Resetting Port");
                    ret = _usbh_port_reset(port_instance->port_handle);
                    ERR_CHECK_GOTO(ESP_OK == ret, "Port Reset failed", usb_driver_reset_);
                    // fifo need re-config after port reset
                    ret = hcd_port_set_fifo_bias(port_instance->port_handle, port_instance->fifo_bias);
                    ERR_CHECK_GOTO(ESP_OK == ret, "Port Set fifo failed", usb_driver_reset_);
                    _usbh_update_state(USBH_STATE_DEVICE_DEFAULT);
                    ESP_LOGI(TAG, "Getting Port Speed");
                    ret = _usbh_port_get_speed(port_instance->port_handle, &port_speed);
                    ERR_CHECK_GOTO(ESP_OK == ret, "USB Port get speed failed", usb_driver_reset_);
                    ESP_LOGI(TAG, "USB Speed: %s-speed", port_speed ? "full" : "low");
                    port_instance->dev_speed = port_speed;

                    if (!pipe_instance_dflt) {
                        pipe_instance_dflt = iot_usbh_pipe_init(port_instance, NULL, NULL, NULL);
                    }
                    ERR_CHECK_GOTO(pipe_instance_dflt != NULL, "default pipe create failed", usb_driver_reset_);
                    ret = _usbh_set_device_addr(pipe_instance_dflt, port_instance->dev_addr);
                    ERR_CHECK_GOTO(ESP_OK == ret, "Set device address failed", usb_driver_reset_);
#ifdef CONFIG_USBH_GET_DEVICE_DESC
                    ret = _usbh_get_dev_desc(pipe_instance_dflt, NULL);
                    ERR_CHECK_GOTO(ESP_OK == ret, "Get device descriptor failed", usb_driver_reset_);
#endif
#ifdef CONFIG_USBH_GET_CONFIG_DESC
                    ret = _usbh_get_config_desc(pipe_instance_dflt, NULL);
                    ERR_CHECK_GOTO(ESP_OK == ret, "Get config descriptor failed", usb_driver_reset_);
#endif
                    _usbh_update_state(USBH_STATE_DEVICE_ADDRESS);
                    ret = _usbh_set_device_config(pipe_instance_dflt, USB_DEVICE_CONFIG);
                    ERR_CHECK_GOTO(ESP_OK == ret, "Set device configuration failed", usb_driver_reset_);
                    _usbh_update_state(USBH_STATE_DEVICE_CONFIGURED);
                    port_instance->dflt_pipe_handle = pipe_instance_dflt;

                    if (port_instance->conn_callback) {
                        port_instance->conn_callback((usbh_port_handle_t)port_instance, port_instance->conn_callback_arg);
                    }
                    break;

                case PORT_EVENT_DISCONNECTION:
                    _usbh_update_state(USBH_STATE_DEVICE_NOT_ATTACHED);
                    ESP_LOGI(TAG, "hcd port state = %d", _usbh_port_get_state(port_instance->port_handle));
                    if (port_instance->disconn_callback) {
                        port_instance->disconn_callback((usbh_port_handle_t)port_instance, port_instance->disconn_callback_arg);
                    }
                    goto usb_driver_reset_;
                    break;
                case PORT_EVENT_ERROR:
                    _usbh_update_state(USBH_STATE_DEVICE_NOT_ATTACHED);
                    ESP_LOGI(TAG, "hcd port state = %d", _usbh_port_get_state(port_instance->port_handle));
                    if (port_instance->disconn_callback) {
                        port_instance->disconn_callback((usbh_port_handle_t)port_instance, port_instance->disconn_callback_arg);
                    }
                    goto usb_driver_reset_;
                    break;
                default:
                    break;
                }

                break;

            case PIPE_EVENT:
                _pipe_event_dflt_process(pipe_instance_dflt, evt_msg._event.pipe_event);
                if (port_instance->event_callback) {
                    port_instance->event_callback(&evt_msg, port_instance->event_callback_arg);
                }
                break;
            default:
                break;
            }
        }

        /* Reset USB to init state */
usb_driver_reset_:
        ESP_LOGW(TAG, "Resetting USB Driver");
        if (pipe_instance_dflt != NULL) {
            port_instance->dflt_pipe_handle = NULL;
            ESP_LOGW(TAG, "Resetting default pipe");
            iot_usbh_pipe_flush(pipe_instance_dflt, 1);
            iot_usbh_pipe_deinit(pipe_instance_dflt);
            pipe_instance_dflt = NULL;
        }

        if (xEventGroupGetBits(evt_group_hdl) & USBH_TASK_KILL_BIT) {
            break;
        }

        ESP_LOGI(TAG, "hcd recovering");
        hcd_port_recover(port_instance->port_handle);
        _usbh_port_power(port_instance->port_handle, true);
        vTaskDelay(pdMS_TO_TICKS(TIMEOUT_USBH_CONNECT_WAIT_MS));
    }

    ESP_LOGW(TAG, "USB Host task deleted");
    ESP_LOGW(TAG, "USB Host task watermark = %d B", uxTaskGetStackHighWaterMark(NULL));
    xEventGroupClearBits(evt_group_hdl, USBH_TASK_KILL_BIT);
    vTaskDelete(NULL);
}
