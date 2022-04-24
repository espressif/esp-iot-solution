/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "string.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "avi_def.h"

static const char *TAG = "avi recorder";

#define MEM_ALIGNMENT 4
#define MEM_ALIGN_SIZE(size) (((size) + MEM_ALIGNMENT - 1) & ~(MEM_ALIGNMENT-1))
#define CUSTOM_CHUNK_SIZE (16*1024)

#define MAKE_FOURCC(a, b, c, d) ((uint32_t)(d)<<24 | (uint32_t)(c)<<16 | (uint32_t)(b)<<8 | (uint32_t)(a))

typedef struct {
    const char *fname; // file name
    int (*get_frame)(void **buf, size_t *len);
    int (*return_frame)(void *buf);
    uint16_t image_width;
    uint16_t image_high;
    uint32_t rec_time;
    EventGroupHandle_t event_hdl;
} recorder_param_t;

typedef struct {
    char filename[64];  // filename for temporary
    int avifile;      // avi file fd
    int idxfile;      // inx file fd, .inx file used to storage the size of each image
    uint32_t nframes;   // the number of frame
    uint32_t totalsize; // all frame image size

    // buffer for preformence
    uint8_t *buffer;
    uint32_t buf_len;
    uint32_t write_len; // the buffer has been used, or length of temporarily stored data to be processed.
} jpeg2avi_data_t;

typedef enum {
    REC_STATE_IDLE,
    REC_STATE_BUSY,
} record_state_t;

// #define USE_MULTI_FRAMES // Enable this if you want get mutil frames first, and then storage them to SD Card
#ifdef USE_MULTI_FRAMES
typedef struct {
    uint8_t **buffer;
    uint32_t buf_len;
} jpeg2avi_frame_array_t;
#define FRAME_ARRAY_SIZE (CONFIG_EXAMPLE_FB_COUNT/2 + 1) // get a set of images(not one), and then write to SD card to improve speed.
#endif

static uint8_t g_force_end = 0;
static record_state_t g_state = REC_STATE_IDLE;

static int jpeg2avi_start(jpeg2avi_data_t *j2a, const char *filename)
{
    ESP_LOGI(TAG, "Starting an avi [%s]", filename);
    if (strlen(filename) > sizeof(j2a->filename) - 5) { // 5 is for '.avi' suffix.
        ESP_LOGE(TAG, "The given file name is too long");
        return ESP_ERR_INVALID_ARG;
    }

    j2a->buf_len = CUSTOM_CHUNK_SIZE + sizeof(AVI_CHUNK_HEAD);
    j2a->buffer = heap_caps_malloc(j2a->buf_len, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT); // for now, we request for inter dram space to improve speed
    if (NULL == j2a->buffer) {
        ESP_LOGE(TAG, "recorder mem failed");
        return -1;
    }

    j2a->write_len = 0;

    memset(j2a->filename, 0, sizeof(j2a->filename));
    strcpy(j2a->filename, filename);

    j2a->avifile = open(j2a->filename, O_RDWR | O_CREAT | O_TRUNC, S_IRWXU);
    if (j2a->avifile == -1) {
        ESP_LOGE(TAG, "Could not open %s (%s)", j2a->filename, strerror(errno));
        return -1;
    }

    strcat(j2a->filename, ".idx");
    j2a->idxfile = open(j2a->filename, O_RDWR | O_CREAT | O_TRUNC, S_IRWXU);
    if (j2a->idxfile == -1) {
        ESP_LOGE(TAG, "Could not open %s (%s)", j2a->filename, strerror(errno));
        close(j2a->avifile);
        unlink(filename);
        return -1;
    }

    uint32_t offset1 = sizeof(AVI_LIST_HEAD);  // the size of riff head
    uint32_t offset2 = sizeof(AVI_HDRL_LIST);  // the size of hdrl list
    uint32_t offset3 = sizeof(AVI_LIST_HEAD);  // the size of movi list head

    // After the offset of the AVI file is set to the movi list head, the JPEG data is written backwards from this position.
    int ret = lseek(j2a->avifile, offset1 + offset2 + offset3, SEEK_SET);
    if (-1 == ret) {
        ESP_LOGE(TAG, "seek avi file failed");
        close(j2a->avifile);
        close(j2a->idxfile);
        unlink(filename);
        unlink(j2a->filename);
        return -1;
    }

    j2a->nframes = 0;
    j2a->totalsize = 0;
    return 0;
}

static int jpeg2avi_add_frame(jpeg2avi_data_t *j2a, uint8_t *data, uint32_t len)
{
    int ret = 0;
    AVI_CHUNK_HEAD frame_head;
    uint32_t align_size = MEM_ALIGN_SIZE(len); // To make JPEG image size 4-byte alignment.
    const int CHUNK_SIZE = CUSTOM_CHUNK_SIZE;

    frame_head.FourCC = MAKE_FOURCC('0', '0', 'd', 'c'); // '00dc' means that it is a compressed video data.
    frame_head.size = align_size;

    // the total size need to be write after get a new frame:[last remain data + chunk_head + new frame data]
    uint32_t remain = j2a->write_len + align_size + sizeof(AVI_CHUNK_HEAD);

    uint32_t last_remain = j2a->write_len;
    while (remain) {
        // int wl = remain >= CHUNK_SIZE ? CHUNK_SIZE : remain;
        /*To improve efficiency, we always get the specified amount of data before actually writing the data to the file.
         *The j2a->buffer[] is:
        ---------------------------------------------------------------------
        |                  CHUNK_SIZE                 | AVI_CHUNK_HEAD_SIZE |
        ---------------------------------------------------------------------
           ^ write_len
        */
        if (remain >= CHUNK_SIZE) {
            memcpy(&j2a->buffer[j2a->write_len], &frame_head, sizeof(AVI_CHUNK_HEAD)); // Copy the header data to the back of the current data to be processed
            j2a->write_len += sizeof(AVI_CHUNK_HEAD);
            int _len = CHUNK_SIZE - last_remain - sizeof(AVI_CHUNK_HEAD);
            memcpy(&j2a->buffer[j2a->write_len], data, _len);
            j2a->write_len += _len;
            data += _len;
            write(j2a->avifile, j2a->buffer, j2a->write_len);

            remain -= j2a->write_len;
            j2a->write_len = 0;
            if (remain >= CHUNK_SIZE) {
                // aligne the remain data to `CHUNK_SIZE` and then write to the avifile.
                int count = remain / CHUNK_SIZE;
                for (size_t i = 0; i < count; i++) {
                    write(j2a->avifile, data, CHUNK_SIZE);
                    data += CHUNK_SIZE;
                    remain -= CHUNK_SIZE;
                }
            }
            memcpy(&j2a->buffer[j2a->write_len], data, remain);
            j2a->write_len += remain;
            remain = 0;
        } else {
            // the remain is [last remain data + chunk_head + new frame data]
            memcpy(&j2a->buffer[j2a->write_len], &frame_head, sizeof(AVI_CHUNK_HEAD)); // add the avi_chunk_header
            j2a->write_len += sizeof(AVI_CHUNK_HEAD);
            memcpy(&j2a->buffer[j2a->write_len], data, remain - sizeof(AVI_CHUNK_HEAD)); // add the new frame data
            j2a->write_len += remain - sizeof(AVI_CHUNK_HEAD);
            remain = 0;
        }
    }

    write(j2a->idxfile, &align_size, 4);/*Save the 4-byte aligned JPEG image size to idx file*/
    j2a->nframes += 1;
    j2a->totalsize += align_size;
    return ret;
}

#ifdef USE_MULTI_FRAMES
static int jpeg2avi_add_frame_array(recorder_param_t *rec_arg, jpeg2avi_data_t *j2a, jpeg2avi_frame_array_t *frames, size_t frame_count)
{
    for (size_t i = 0; i < frame_count; i++) {
        if (jpeg2avi_add_frame(j2a, *frames[i].buffer, frames[i].buf_len)) {
            ESP_LOGE(TAG, "add frames failed");
            return -1;
        }
#ifdef FRAME_RETURN_ENABLE
        rec_arg->return_frame(frames[i].buffer);
#endif
    }
    return 0;
}
#endif

static int jpeg2avi_write_header(jpeg2avi_data_t *j2a, uint32_t width, uint32_t height, uint32_t fps)
{

    AVI_LIST_HEAD riff_head = {
        .List = MAKE_FOURCC('R', 'I', 'F', 'F'),
        // The size is 'AVI ' + hdrl + movi + index; hdrl = avih + strl; movi = n * (sizeof(AVI_CHUNK_HEAD) + aligned_image_data)
        .size = 4 + sizeof(AVI_HDRL_LIST) + sizeof(AVI_LIST_HEAD) + j2a->nframes * 8 + j2a->totalsize + (sizeof(AVI_IDX1) * j2a->nframes) + 8,
        .FourCC = MAKE_FOURCC('A', 'V', 'I', ' ')
    };

    AVI_HDRL_LIST hdrl_list = {
        {
            .List = MAKE_FOURCC('L', 'I', 'S', 'T'),
            .size = sizeof(AVI_HDRL_LIST) - 8,
            .FourCC = MAKE_FOURCC('h', 'd', 'r', 'l'),
        },
        {
            .FourCC = MAKE_FOURCC('a', 'v', 'i', 'h'),
            .size = sizeof(AVI_AVIH_CHUNK) - 8,
            .us_per_frame = 1000000 / fps,
            .max_bytes_per_sec = (width * height * 2) / 10,
            .padding = 0,
            .flags = 0,
            .total_frames = j2a->nframes,
            .init_frames = 0,
            .streams = 1,
            .suggest_buff_size = (width * height * 2),
            .width = width,
            .height = height,
            .reserved = {0, 0, 0, 0},
        },
        {
            {
                .List = MAKE_FOURCC('L', 'I', 'S', 'T'),
                .size = sizeof(AVI_STRL_LIST) - 8,
                .FourCC = MAKE_FOURCC('s', 't', 'r', 'l'),
            },
            {
                .FourCC = MAKE_FOURCC('s', 't', 'r', 'h'),
                .size = sizeof(AVI_STRH_CHUNK) - 8,
                .fourcc_type = MAKE_FOURCC('v', 'i', 'd', 's'),
                .fourcc_codec = MAKE_FOURCC('M', 'J', 'P', 'G'),
                .flags = 0,
                .priority = 0,
                .language = 0,
                .init_frames = 0,
                .scale = 1,
                .rate = fps, //rate / scale = fps
                .start = 0,
                .length = j2a->nframes,
                .suggest_buff_size = (width * height * 2),
                .quality = 1,
                .sample_size = 0,
                .rcFrame = {0, 0, width, height},
            },
            {
                .FourCC = MAKE_FOURCC('s', 't', 'r', 'f'),
                .size = sizeof(AVI_VIDS_STRF_CHUNK) - 8,
                .size1 = sizeof(AVI_VIDS_STRF_CHUNK) - 8,
                .width = width,
                .height = height,
                .planes = 1,
                .bitcount = 24,
                .fourcc_compression = MAKE_FOURCC('M', 'J', 'P', 'G'),
                .image_size = width * height * 3,
                .x_pixels_per_meter = 0,
                .y_pixels_per_meter = 0,
                .num_colors = 0,
                .imp_colors = 0,
            }
        }
    };

    AVI_LIST_HEAD movi_list_head = {
        .List = MAKE_FOURCC('L', 'I', 'S', 'T'),
        .size = 4 + j2a->nframes * 8 + j2a->totalsize,
        .FourCC = MAKE_FOURCC('m', 'o', 'v', 'i')
    };

    //Relocate to the file header and backfill each block of data
    lseek(j2a->avifile, 0, SEEK_SET);
    write(j2a->avifile, &riff_head, sizeof(AVI_LIST_HEAD));
    write(j2a->avifile, &hdrl_list, sizeof(AVI_HDRL_LIST));
    write(j2a->avifile, &movi_list_head, sizeof(AVI_LIST_HEAD));

    return 0;
}

static int jpeg2avi_write_index_chunk(jpeg2avi_data_t *j2a)
{
    size_t i;
    uint32_t index = MAKE_FOURCC('i', 'd', 'x', '1');  // index chunk default fourcc
    uint32_t index_chunk_size = sizeof(AVI_IDX1) * j2a->nframes;   // the total size of avi index data
    uint32_t offset = 4;
    uint32_t frame_size;
    AVI_IDX1 idx;

    lseek(j2a->idxfile, 0, SEEK_SET);
    ESP_LOGI(TAG, "frame number=%d, size=%dKB", j2a->nframes, j2a->totalsize / 1024);
    write(j2a->avifile, &index, 4);
    write(j2a->avifile, &index_chunk_size, 4);

    idx.FourCC = MAKE_FOURCC('0', '0', 'd', 'c'); //00dc = compressed video data
    for (i = 0; i < j2a->nframes; i++) {
        read(j2a->idxfile, &frame_size, 4); // Read size of each jpeg image
        idx.flags = 0x10; // 0x10 indicates that the current frame is a keyframe(golden frame).
        idx.chunkoffset = offset;
        idx.chunklength = frame_size;
        write(j2a->avifile, &idx, sizeof(AVI_IDX1));

        offset = offset + frame_size + 8;
    }
    close(j2a->idxfile);
    unlink(j2a->filename);
    if (i != j2a->nframes) {
        ESP_LOGE(TAG, "avi index write failed");
        return -1;
    }

    return 0;
}

static void jpeg2avi_end(jpeg2avi_data_t *j2a, int width, int height, uint32_t fps)
{
    ESP_LOGI(TAG, "video info: width=%d | height=%d | fps=%u", width, height, fps);
    if (j2a->write_len) { // If there is still data in the current buffer, write all to the file
        write(j2a->avifile, j2a->buffer, j2a->write_len);
    }

    jpeg2avi_write_index_chunk(j2a); // write index block to the avi file
    jpeg2avi_write_header(j2a, width, height, fps); // file the avi file header data.
    close(j2a->avifile);
    free(j2a->buffer);

    ESP_LOGI(TAG, "avi recording completed");
}

static void recorder_task(void *args)
{
    int ret;
    recorder_param_t *rec_arg = (recorder_param_t *)args;

    jpeg2avi_data_t avi_recoder;
    ret = jpeg2avi_start(&avi_recoder, rec_arg->fname);
    if (0 != ret) {
        ESP_LOGE(TAG, "start failed");
        goto err;
    }

    g_state = REC_STATE_BUSY;

    uint64_t fr_start = esp_timer_get_time() / 1000;
    uint64_t end_time = rec_arg->rec_time * 1000 + fr_start;
    uint64_t printf_time = fr_start;
    while (1) {
#ifndef USE_MULTI_FRAMES
        uint8_t **buffer;
        size_t len;
        ret = rec_arg->get_frame((void **)&buffer, &len);
        if (0 != ret) {
            ESP_LOGE(TAG, "get frame failed");
            vTaskDelay(5000 / portTICK_PERIOD_MS);
            continue;
        }

        ret = jpeg2avi_add_frame(&avi_recoder, *buffer, len);

        if (rec_arg->return_frame) {
            rec_arg->return_frame(buffer);
        }
#else
        jpeg2avi_frame_array_t s_frames[FRAME_ARRAY_SIZE];
        for (size_t i = 0; i < FRAME_ARRAY_SIZE; i++) {
            ret = rec_arg->get_frame((void **)&s_frames[i].buffer, &s_frames[i].buf_len);
            if (0 != ret) {
                ESP_LOGE(TAG, "get frame failed");
                vTaskDelay(200 / portTICK_PERIOD_MS);
                continue;
            }
        }

        ret = jpeg2avi_add_frame_array(rec_arg, &avi_recoder, s_frames, (sizeof(s_frames) / sizeof(jpeg2avi_frame_array_t)));

        for (size_t i = 0; i < FRAME_ARRAY_SIZE; i++) {
            rec_arg->return_frame(s_frames[i].buffer);
        }
#endif

        if (0 != ret) {
            break;
        }

        uint64_t t = esp_timer_get_time() / 1000;
        if (t - printf_time > 1000) {
            printf_time = t;
            ESP_LOGD(TAG, "recording %d/%d s", (uint32_t)((t - fr_start) / 1000), rec_arg->rec_time);
        }
        if (t > end_time || g_force_end) {
            break;
        }
    }
    uint32_t fps = avi_recoder.nframes * 1000 / (esp_timer_get_time() / 1000 - fr_start);
    jpeg2avi_end(&avi_recoder, rec_arg->image_width, rec_arg->image_high, fps);
err:
    g_state = REC_STATE_IDLE;
    g_force_end = 0;
    if (rec_arg->event_hdl) {
        xEventGroupSetBits(rec_arg->event_hdl, BIT2);
    }
    vTaskDelete(NULL);
}

esp_err_t avi_recorder_start(const char *fname,
    int (*get_frame)(void **buf, size_t *len),
    int (*return_frame)(void *buf),
    uint16_t image_width,
    uint16_t image_high,
    uint32_t rec_time,
    bool block)
{
    if (REC_STATE_IDLE != g_state) {
        ESP_LOGE(TAG, "recorder already running");
        return ESP_ERR_INVALID_ARG;
    }
    if (NULL == get_frame) {
        ESP_LOGE(TAG, "recorder get frame function is invalid");
        return ESP_ERR_INVALID_ARG;
    }
    g_force_end = 0;
    static recorder_param_t rec_arg = {0};
    rec_arg.get_frame = get_frame;
    rec_arg.return_frame = return_frame;
    rec_arg.image_width = image_width;
    rec_arg.image_high = image_high;
    rec_arg.fname = fname;
    rec_arg.rec_time = rec_time;
    rec_arg.event_hdl = xEventGroupCreate();
    
    if(pdPASS != xTaskCreatePinnedToCore(recorder_task, "recorder", 1024 * 4, &rec_arg, configMAX_PRIORITIES - 2, NULL, 1)) {
        vEventGroupDelete(rec_arg.event_hdl);
        return ESP_FAIL;
    }
    g_state = REC_STATE_BUSY;

    if (block) {
        xEventGroupWaitBits(rec_arg.event_hdl, BIT2, pdTRUE, pdTRUE, portMAX_DELAY);
    }
    vEventGroupDelete(rec_arg.event_hdl);
    rec_arg.event_hdl = NULL;

    return ESP_OK;
}

void avi_recorder_stop(void)
{
    if (REC_STATE_BUSY != g_state) {
        ESP_LOGE(TAG, "recorder already idle");
        return;
    }
    g_force_end = 1;
}
