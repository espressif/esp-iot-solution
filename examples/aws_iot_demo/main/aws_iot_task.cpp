/*
 * Copyright 2010-2015 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 * Additions Copyright 2016 Espressif Systems (Shanghai) PTE LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 * You may not use this file except in compliance with the License.
 * A copy of the License is located at
 *
 *  http://aws.amazon.com/apache2.0
 *
 * or in the "license" file accompanying this file. This file is distributed
 * on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */
/**
 * @file subscribe_publish_sample.c
 * @brief simple MQTT publish and subscribe on the same topic
 *
 * This example takes the parameters from the build configuration and establishes a connection to the AWS IoT MQTT Platform.
 * It subscribes and publishes to the same topic - "test_topic/esp32"
 *
 * Some setup is required. See example README for details.
 *
 */
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_log.h"
#include "apps/sntp/sntp.h"
/*SPI & LCD Driver Includes*/
#include "iot_lcd.h"
#include "FreeSans9pt7b.h"
/*AWS includes*/
#include "aws_iot_config.h"
#include "aws_iot_log.h"
#include "aws_iot_version.h"
#include "aws_iot_mqtt_client_interface.h"
#include "aws_iot_demo.h"
#include "image.h"


#if defined(CONFIG_EXAMPLE_EMBEDDED_CERTS)

extern const uint8_t aws_root_ca_pem_start[] asm("_binary_aws_root_ca_pem_start");
extern const uint8_t aws_root_ca_pem_end[] asm("_binary_aws_root_ca_pem_end");
extern const uint8_t certificate_pem_crt_start[] asm("_binary_certificate_pem_crt_start");
extern const uint8_t certificate_pem_crt_end[] asm("_binary_certificate_pem_crt_end");
extern const uint8_t private_pem_key_start[] asm("_binary_private_pem_key_start");
extern const uint8_t private_pem_key_end[] asm("_binary_private_pem_key_end");

#elif defined(CONFIG_EXAMPLE_FILESYSTEM_CERTS)

static const char * DEVICE_CERTIFICATE_PATH = CONFIG_EXAMPLE_CERTIFICATE_PATH;
static const char * DEVICE_PRIVATE_KEY_PATH = CONFIG_EXAMPLE_PRIVATE_KEY_PATH;
static const char * ROOT_CA_PATH = CONFIG_EXAMPLE_ROOT_CA_PATH;

#else
#error "Invalid method for loading certs"
#endif

extern const char *AWSIOTTAG;

#if CONFIG_ESP32_AWS_LCD
extern CEspLcd* tft;
#endif

/**
 * @brief Default MQTT HOST URL is pulled from the aws_iot_config.h
 */
char HostAddress[255] = AWS_IOT_MQTT_HOST;

/**
 * @brief Default MQTT port is pulled from the aws_iot_config.h
 */
uint32_t port = AWS_IOT_MQTT_PORT;


void iot_subscribe_callback_handler(AWS_IoT_Client *pClient, char *topicName,
        uint16_t topicNameLen, IoT_Publish_Message_Params *params, void *pData)
{
    ESP_LOGI(AWSIOTTAG, "Subscribe callback");
    ESP_LOGI(AWSIOTTAG, "%.*s\t%.*s", topicNameLen, topicName,
            (int ) params->payloadLen, (char * )params->payload);

    /*Copy the new string to pData buffer*/
    strlcpy((char *) pData, (const char*) params->payload,
            (int) params->payloadLen + 1);

    ESP_LOGI(AWSIOTTAG, "Subscribing... %s \n", (char * )pData);
    app_lcd_aws_sub_cb(pData);
}

void disconnectCallbackHandler(AWS_IoT_Client *pClient, void *data)
{
    ESP_LOGW(AWSIOTTAG, "MQTT Disconnect");
    IoT_Error_t rc = FAILURE;

    if (NULL == pClient) {
        return;
    }

    if (aws_iot_is_autoreconnect_enabled(pClient)) {
        ESP_LOGI(AWSIOTTAG, "Auto Reconnect is enabled, Reconnecting attempt will start now");
    } else {
        ESP_LOGW(AWSIOTTAG, "Auto Reconnect not enabled. Starting manual reconnect...");
        rc = aws_iot_mqtt_attempt_reconnect(pClient);
        if (NETWORK_RECONNECTED == rc) {
            ESP_LOGW(AWSIOTTAG, "Manual Reconnect Successful");
        } else {
            ESP_LOGW(AWSIOTTAG, "Manual Reconnect Failed - %d", rc);
        }
    }
}

void aws_iot_task(void *param)
{
    char cPayload[100];
    int32_t i = 0;
    IoT_Error_t rc = FAILURE;
    AWS_IoT_Client client;
    IoT_Client_Init_Params mqttInitParams = iotClientInitParamsDefault;
    IoT_Client_Connect_Params connectParams = iotClientConnectParamsDefault;
    IoT_Publish_Message_Params paramsQOS0;

    ESP_LOGI(AWSIOTTAG, "AWS IoT SDK Version %d.%d.%d-%s", VERSION_MAJOR,
            VERSION_MINOR, VERSION_PATCH, VERSION_TAG);

    mqttInitParams.enableAutoReconnect = false; // We enable this later below
    mqttInitParams.pHostURL = HostAddress;
    mqttInitParams.port = port;
    ESP_LOGI(AWSIOTTAG, "mqttInitParams AWS IoT SDK Version pHostURL:%s.port:%d.\n",
            HostAddress, port);

#if defined(CONFIG_EXAMPLE_EMBEDDED_CERTS)
    mqttInitParams.pRootCALocation = (const char *)aws_root_ca_pem_start;
    mqttInitParams.pDeviceCertLocation = (const char *)certificate_pem_crt_start;
    mqttInitParams.pDevicePrivateKeyLocation = (const char *)private_pem_key_start;

#elif defined(CONFIG_EXAMPLE_FILESYSTEM_CERTS)
    mqttInitParams.pRootCALocation = ROOT_CA_PATH;
    mqttInitParams.pDeviceCertLocation = DEVICE_CERTIFICATE_PATH;
    mqttInitParams.pDevicePrivateKeyLocation = DEVICE_PRIVATE_KEY_PATH;
#endif

    mqttInitParams.mqttCommandTimeout_ms = 20000;
    mqttInitParams.tlsHandshakeTimeout_ms = 5000;
    mqttInitParams.isSSLHostnameVerify = true;
    mqttInitParams.disconnectHandler = disconnectCallbackHandler;
    mqttInitParams.disconnectHandlerData = NULL;

#ifdef CONFIG_EXAMPLE_SDCARD_CERTS
    ESP_LOGI(AWSIOTTAG, "Mounting SD card...");
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 3,
    };
    sdmmc_card_t* card;
    esp_err_t ret = esp_vfs_fat_sdmmc_mount("/sdcard", &host, &slot_config, &mount_config, &card);
    if (ret != ESP_OK) {
        ESP_LOGE(AWSIOTTAG, "Failed to mount SD card VFAT filesystem.");
        abort();
    }
#endif

    rc = aws_iot_mqtt_init(&client, &mqttInitParams);
    if (SUCCESS != rc) {
        ESP_LOGE(AWSIOTTAG, "aws_iot_mqtt_init returned error : %d ", rc);
        abort();
    }

    connectParams.keepAliveIntervalInSec = 10;
    connectParams.isCleanSession = true;
    connectParams.MQTTVersion = MQTT_3_1_1;

    /* Client ID is set in the menuconfig of the example */
    connectParams.pClientID = CONFIG_AWS_EXAMPLE_CLIENT_ID;
    connectParams.clientIDLen = (uint16_t) strlen(CONFIG_AWS_EXAMPLE_CLIENT_ID);
    connectParams.isWillMsgPresent = false;
    ESP_LOGI(AWSIOTTAG, "Connecting to AWS...");
    app_lcd_aws_connecting();
    do {
        rc = aws_iot_mqtt_connect(&client, &connectParams);
        if (SUCCESS != rc) {
            ESP_LOGE(AWSIOTTAG, "Error(%d) connecting to %s:%d", rc,
                    mqttInitParams.pHostURL, mqttInitParams.port);
            vTaskDelay(1000 / portTICK_RATE_MS);
        }
    } while (SUCCESS != rc);

    /*
     * Enable Auto Reconnect functionality. Minimum and Maximum time of Exponential backoff are set in aws_iot_config.h
     */
    rc = aws_iot_mqtt_autoreconnect_set_status(&client, true);
    if (SUCCESS != rc) {
        ESP_LOGE(AWSIOTTAG, "Unable to set Auto Reconnect to true - %d", rc);
        abort();
    }

    app_lcd_aws_connected();
    char cb_data[100];

    ESP_LOGI(AWSIOTTAG, "Subscribing...");
    ESP_LOGI(AWSIOTTAG, "Subscribing... %s \n", SUBSCRIBE_TOPIC);
    rc = aws_iot_mqtt_subscribe(&client, SUBSCRIBE_TOPIC, TOPIC_LEN, QOS0,
            iot_subscribe_callback_handler, &cb_data);

    if (SUCCESS != rc) {
        ESP_LOGE(AWSIOTTAG, "Error subscribing : %d ", rc);
        abort();
    }
    ESP_LOGE(AWSIOTTAG, "Success subscribing : %d ", rc);
    sprintf(cPayload, "%s : %d ", "hello from SDK", i);

    paramsQOS0.qos = QOS0;
    paramsQOS0.payload = (void *) cPayload;
    paramsQOS0.isRetained = 0;

    while ((NETWORK_ATTEMPTING_RECONNECT == rc || NETWORK_RECONNECTED == rc
            || SUCCESS == rc)) {

        //Max time the yield function will wait for read messages
        rc = aws_iot_mqtt_yield(&client, 100);
        if (NETWORK_ATTEMPTING_RECONNECT == rc) {
            // If the client is attempting to reconnect we will skip the rest of the loop.
            ESP_LOGE(AWSIOTTAG, "NETWORK_ATTEMPTING_RECONNECT.\n");
            continue;
        }
        sprintf(cPayload, "%s : %d ", "hello from ESP32 (QOS0)", i++);
        paramsQOS0.payloadLen = strlen(cPayload);
        rc = aws_iot_mqtt_publish(&client, PUBLISH_TOPIC, PUB_TOPIC_LEN,
                &paramsQOS0);

        app_lcd_aws_pub_cb(cPayload);
        if (rc == MQTT_REQUEST_TIMEOUT_ERROR) {
            ESP_LOGW(AWSIOTTAG, "QOS1 publish ack not received.");
            rc = SUCCESS;
        }

        ESP_LOGI(AWSIOTTAG, "-->sleep");
        vTaskDelay(4000 / portTICK_RATE_MS);

    }

    ESP_LOGE(AWSIOTTAG, "An error occurred in the main loop.");
    abort();
}
