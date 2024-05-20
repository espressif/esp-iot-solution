/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "OpenAI.h"
#include "unity.h"
#include "esp_random.h"
#include "protocol_examples_common.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "driver/uart.h"

static const char *TAG = "openai_test";

#define TEST_MEMORY_LEAK_THRESHOLD (-3000)

static char *openai_key = CI_OPENAI_KEY;
extern const uint8_t turn_on_tv_en_mp3_start[] asm("_binary_turn_on_tv_en_mp3_start");
extern const uint8_t turn_on_tv_en_mp3_end[]   asm("_binary_turn_on_tv_en_mp3_end");
extern const uint8_t introduce_espressif_mp3_start[] asm("_binary_introduce_espressif_mp3_start");
extern const uint8_t introduce_espressif_mp3_end[]   asm("_binary_introduce_espressif_mp3_end");

TEST_CASE("test ChatCompletion", "[ChatCompletion]")
{
    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * Read "Establishing Wi-Fi or Ethernet Connection" section in
     * examples/protocols/README.md for more information about this function.
     */
    ESP_ERROR_CHECK(example_connect());
    ESP_LOGI(TAG, "Connected to AP, begin http example");

    OpenAI_t *openai = OpenAICreate(openai_key);
    TEST_ASSERT_NOT_NULL(openai);
    OpenAI_ChatCompletion_t *chatCompletion = openai->chatCreate(openai);
    TEST_ASSERT_NOT_NULL(chatCompletion);
    chatCompletion->setModel(chatCompletion, "gpt-3.5-turbo");  //Model to use for completion. Default is gpt-3.5-turbo
    chatCompletion->setSystem(chatCompletion, "You are a helpful assistant.");     //Description of the required assistant
    chatCompletion->setMaxTokens(chatCompletion, 1024);         //The maximum number of tokens to generate in the completion.
    chatCompletion->setTemperature(chatCompletion, 0.2);        //float between 0 and 1. Higher value gives more random results.
    chatCompletion->setStop(chatCompletion, "\r");              //Up to 4 sequences where the API will stop generating further tokens.
    chatCompletion->setPresencePenalty(chatCompletion, 0);      //float between -2.0 and 2.0. Positive values increase the model's likelihood to talk about new topics.
    chatCompletion->setFrequencyPenalty(chatCompletion, 0);     //float between -2.0 and 2.0. Positive values decrease the model's likelihood to repeat the same line verbatim.
    chatCompletion->setUser(chatCompletion, "OpenAI-ESP32");    //A unique identifier representing your end-user, which can help OpenAI to monitor and detect abuse.
    // chinese
    OpenAI_StringResponse_t *result = chatCompletion->message(chatCompletion, "给我讲一个笑话", false);
    TEST_ASSERT_NOT_NULL(result);
    if (result->getLen(result) == 1) {
        ESP_LOGI(TAG, "Received message. Tokens: %"PRIu32"", result->getUsage(result));
        char *response = result->getData(result, 0);
        ESP_LOGI(TAG, "%s", response);
    } else if (result->getLen(result) > 1) {
        ESP_LOGI(TAG, "Received %"PRIu32" messages. Tokens: %"PRIu32"", result->getLen(result), result->getUsage(result));
        for (int i = 0; i < result->getLen(result); ++i) {
            char *response = result->getData(result, i);
            ESP_LOGI(TAG, "Message[%d]: %s", i, response);
        }
    } else if (result->getError(result)) {
        ESP_LOGE(TAG, "Error! %s", result->getError(result));
    } else {
        ESP_LOGE(TAG, "Unknown error!");
    }
    result->deleteResponse(result);
    // english
    result = chatCompletion->message(chatCompletion, "tell me a joke", false);
    TEST_ASSERT_NOT_NULL(result);
    if (result->getLen(result) == 1) {
        ESP_LOGI(TAG, "Received message. Tokens: %"PRIu32"", result->getUsage(result));
        char *response = result->getData(result, 0);
        ESP_LOGI(TAG, "%s", response);
    } else if (result->getLen(result) > 1) {
        ESP_LOGI(TAG, "Received %"PRIu32" messages. Tokens: %"PRIu32"", result->getLen(result), result->getUsage(result));
        for (int i = 0; i < result->getLen(result); ++i) {
            char *response = result->getData(result, i);
            ESP_LOGI(TAG, "Message[%d]: %s", i, response);
        }
    } else if (result->getError(result)) {
        ESP_LOGE(TAG, "Error! %s", result->getError(result));
    } else {
        ESP_LOGE(TAG, "Unknown error!");
    }

    result->deleteResponse(result);
    openai->chatDelete(chatCompletion);
    OpenAIDelete(openai);
    example_disconnect();
    vTaskDelay(1000 / portTICK_PERIOD_MS);
}

TEST_CASE("test AudioTranscription en", "[AudioTranscription]")
{
    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * Read "Establishing Wi-Fi or Ethernet Connection" section in
     * examples/protocols/README.md for more information about this function.
     */
    ESP_ERROR_CHECK(example_connect());
    ESP_LOGI(TAG, "Connected to AP, begin http example");
    OpenAI_t *openai = OpenAICreate(openai_key);
    OpenAI_AudioTranscription_t *audioTranscription = openai->audioTranscriptionCreate(openai);
    TEST_ASSERT_NOT_NULL(audioTranscription);
    size_t length = turn_on_tv_en_mp3_end - turn_on_tv_en_mp3_start;
    audioTranscription->setResponseFormat(audioTranscription, OPENAI_AUDIO_RESPONSE_FORMAT_JSON);
    audioTranscription->setTemperature(audioTranscription, 0.2);                                                            //float between 0 and 1. Higher value gives more random results.
    audioTranscription->setLanguage(audioTranscription, "en");                                                              //Set to English to make GPT return faster and more accurate
    char *text = audioTranscription->file(audioTranscription, (uint8_t *)turn_on_tv_en_mp3_start, length, OPENAI_AUDIO_INPUT_FORMAT_MP3);
    TEST_ASSERT_NOT_NULL(text);
    ESP_LOGI(TAG, "Text: %s", text);
    free(text);
    openai->audioTranscriptionDelete(audioTranscription);
    OpenAIDelete(openai);
    example_disconnect();
    vTaskDelay(1000 / portTICK_PERIOD_MS);
}

TEST_CASE("test AudioTranscription cn", "[AudioTranscription]")
{
    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * Read "Establishing Wi-Fi or Ethernet Connection" section in
     * examples/protocols/README.md for more information about this function.
     */
    ESP_ERROR_CHECK(example_connect());
    ESP_LOGI(TAG, "Connected to AP, begin http example");
    OpenAI_t *openai = OpenAICreate(openai_key);
    OpenAI_AudioTranscription_t *audioTranscription = openai->audioTranscriptionCreate(openai);
    TEST_ASSERT_NOT_NULL(audioTranscription);
    size_t length = introduce_espressif_mp3_end - introduce_espressif_mp3_start;
    audioTranscription->setResponseFormat(audioTranscription, OPENAI_AUDIO_RESPONSE_FORMAT_JSON);
    audioTranscription->setPrompt(audioTranscription, "请回复简体中文");                                                    //The default will return Traditional Chinese, here we add prompt to make GPT return Simplified Chinese
    audioTranscription->setTemperature(audioTranscription, 0.2);                                                            //float between 0 and 1. Higher value gives more random results.
    audioTranscription->setLanguage(audioTranscription, "zh");                                                              //Set to Chinese to make GPT return faster and more accurate
    char *text = audioTranscription->file(audioTranscription, (uint8_t *)introduce_espressif_mp3_start, length, OPENAI_AUDIO_INPUT_FORMAT_MP3);
    TEST_ASSERT_NOT_NULL(text);
    ESP_LOGI(TAG, "Text: %s", text);
    free(text);
    openai->audioTranscriptionDelete(audioTranscription);
    OpenAIDelete(openai);
    example_disconnect();
    vTaskDelay(1000 / portTICK_PERIOD_MS);
}

TEST_CASE("test AudioSpeech", "[AudioSpeech]")
{
    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * Read "Establishing Wi-Fi or Ethernet Connection" section in
     * examples/protocols/README.md for more information about this function.
     */
    ESP_ERROR_CHECK(example_connect());
    ESP_LOGI(TAG, "Connected to AP, begin http example");
    OpenAI_t *openai = OpenAICreate(openai_key);
    OpenAI_AudioTranscription_t *audioTranscription = openai->audioTranscriptionCreate(openai);
    OpenAI_AudioSpeech_t *audioSpeech = openai->audioSpeechCreate(openai);
    /*preparing a transcription*/
    TEST_ASSERT_NOT_NULL(audioTranscription);
    size_t length = turn_on_tv_en_mp3_end - turn_on_tv_en_mp3_start;
    audioTranscription->setResponseFormat(audioTranscription, OPENAI_AUDIO_RESPONSE_FORMAT_JSON);
    audioTranscription->setTemperature(audioTranscription, 0.2);                                                            //float between 0 and 1. Higher value gives more random results.
    audioTranscription->setLanguage(audioTranscription, "en");                                                              //Set to English to make GPT return faster and more accurate
    /*preparing a audio speech*/
    TEST_ASSERT_NOT_NULL(audioSpeech);
    audioSpeech->setModel(audioSpeech, "tts-1-hd");
    audioSpeech->setVoice(audioSpeech, "nova");
    audioSpeech->setResponseFormat(audioSpeech, OPENAI_AUDIO_OUTPUT_FORMAT_MP3);
    audioSpeech->setSpeed(audioSpeech, 0.8);
    /*sending to transcription api to get transcriptions of save audio file*/
    char *giventext = audioTranscription->file(audioTranscription, (uint8_t *)turn_on_tv_en_mp3_start, length, OPENAI_AUDIO_INPUT_FORMAT_MP3);
    TEST_ASSERT_NOT_NULL(giventext);
    ESP_LOGI(TAG, "Given Text: %s", giventext);
    /*sending to speech api to get mp3 of transcriptions*/
    OpenAI_SpeechResponse_t *speechresult = audioSpeech->speech(audioSpeech, giventext);
    TEST_ASSERT_NOT_NULL(speechresult);
    char *response = speechresult->getData(speechresult);
    /*sending to transcriptions api to get transcriptions of mp3 coming from speech api*/
    char *finaltext = audioTranscription->file(audioTranscription, (uint8_t *)response, speechresult->getLen(speechresult), OPENAI_AUDIO_INPUT_FORMAT_MP3);
    TEST_ASSERT_NOT_NULL(finaltext);
    ESP_LOGI(TAG, "Final Text: %s", finaltext);
    TEST_ASSERT_TRUE(strcmp(giventext, finaltext) == 0);
    free(giventext);
    free(finaltext);
    openai->audioTranscriptionDelete(audioTranscription);
    speechresult->deleteResponse(speechresult);
    openai->audioSpeechDelete(audioSpeech);
    OpenAIDelete(openai);
    example_disconnect();
    vTaskDelay(1000 / portTICK_PERIOD_MS);
}

TEST_CASE("test memory leak", "[memory]")
{
    OpenAI_t *openai = OpenAICreate(openai_key);
    TEST_ASSERT_NOT_NULL(openai);
    OpenAI_Completion_t *completion = openai->completionCreate(openai);
    TEST_ASSERT_NOT_NULL(completion);
    completion->setModel(completion, "gpt-3.5-turbo");  //Model to use for completion. Default is gpt-3.5-turbo
    completion->setUser(completion, "test");
    openai->completionDelete(completion);
    OpenAIDelete(openai);
}

static size_t before_free_8bit;
static size_t before_free_32bit;

static void check_leak(size_t before_free, size_t after_free, const char *type)
{
    ssize_t delta = after_free - before_free;
    printf("MALLOC_CAP_%s: Before %u bytes free, After %u bytes free (delta %d)\n", type, before_free, after_free, delta);
    TEST_ASSERT_MESSAGE(delta >= TEST_MEMORY_LEAK_THRESHOLD, "memory leak");
}

void setUp(void)
{
    before_free_8bit = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    before_free_32bit = heap_caps_get_free_size(MALLOC_CAP_32BIT);
}

void tearDown(void)
{
    size_t after_free_8bit = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    size_t after_free_32bit = heap_caps_get_free_size(MALLOC_CAP_32BIT);
    check_leak(before_free_8bit, after_free_8bit, "8BIT");
    check_leak(before_free_32bit, after_free_32bit, "32BIT");
}

void app_main(void)
{
    printf("OpenAI TEST \n");
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // 比较 openai_key 是否是空字符串
    if (strlen(openai_key) == 0) {
        ESP_LOGE(TAG, "Please enter your openai_key");
        return;
    }

    unity_run_menu();
}
