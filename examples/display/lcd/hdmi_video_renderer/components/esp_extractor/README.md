# ESP_Extractor

This component is used to extract stream data (video, audio etc) from container files.  
The extracted stream data is formed by one or multiple frames which can be easily decoded or re-encapsulate into other containers.

## Features
   
- Support filter video and audio track data
- Support time seek operation
- Support resume playback from breakpoint
- Support change audio and video track
- Support input data cache to improve parse speed
- Support fast quit during parsing
- Support fast seek when container not contain seek table

## Supported Containers and Codecs

|           | MP4  | TS   | FLV  | WAV  | AudioES | HLS  | OGG  | AVI |
| :-------- | :--- | :--- | :--- | :--- | :---    | :--- | :--- |:--- |
| PCM       | Y    | N    | Y    | Y    | N       | N    | N    | N   |
| AAC       | Y    | Y    | Y    | N    | Y       | Y    | N    | Y   |
| MP3       | Y    | Y    | Y    | N    | N       | N    | N    | Y   |
| ADPCM     | N    | N    | N    | Y    | N       | N    | N    | N   |
| G711-Alaw | N    | N    | N    | N    | N       | N    | N    | N   |
| G711-Ulaw | N    | N    | N    | N    | Y       | N    | N    | N   |
| AMR-NB    | N    | N    | N    | N    | Y       | N    | N    | N   |
| AMR-WB    | N    | N    | N    | N    | N       | N    | N    | N   |
| FLAC      | N    | N    | N    | N    | Y       | N    | Y    | N   |
| VORBIS    | N    | N    | N    | N    | N       | N    | Y    | N   |
| OPUS      | N    | N    | N    | N    | N       | N    | Y    | N   |
| H264      | Y    | Y    | Y    | N    | N       | Y    | N    | Y   |
| MJPEG     | Y    | Y    | Y    | N    | N       | N    | N    | Y   |

Notes:
    Since TS and FLV do not officially support MJPEG. We add MJPEG support for TS and FLV using following methods:
  - FLV use codec id 1 for MJPEG codec
  - TS use stream id 6 for MJPEG codec 

## Usage

Follow example demonstrate how to extract audio and video track data from container files and save it into SDCard
```c
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_extractor.h"
#include "esp_wav_extractor.h"
#include "esp_mp4_extractor.h"
#include "esp_audio_es_extractor.h"
#include "esp_ogg_extractor.h"
#include "esp_ts_extractor.h"
#include "esp_avi_extractor.h"
#include "esp_hls_extractor.h"
#include "mem_pool.h"

#define TAG "Test"

#define MOUNT_PATH "/sdcard/"

#define RETURN_ON_FAIL(ret)                                 \
if (ret != 0) {                                             \
    ESP_LOGE(TAG, "Fail at line %d ret:%d", __LINE__, ret); \
    return ret;                                             \
}

#define BREAK_ON_FAIL(ret)                                  \
if (ret != 0) {                                             \
    ESP_LOGE(TAG, "Fail at line %d ret:%d", __LINE__, ret); \
    break;                                                  \
}

typedef struct {
    esp_extractor_handle_t            extractor;
    extractor_audio_format_t          audio_format;
    extractor_video_format_t          video_format;
    FILE                              *fp_dump_audio;
    FILE                              *fp_dump_video;
    uint32_t                          audio_pts;
    uint32_t                          video_pts;
} extractor_test_t;

static void *_wrapper_open(char *url, void *ctx)
{
    return fopen(url, "rb");
}

static int _wrapper_read(void *data, uint32_t size, void *ctx)
{
    return fread(data, 1, size, (FILE*) ctx);
}

static int _wrapper_seek(uint32_t position, void *ctx)
{
    return fseek((FILE*) ctx, position, 0);
}

static int _wrapper_close(void *ctx)
{
    return fclose((FILE*) ctx);
}

static uint32_t _wrapper_file_size(void *ctx)
{
    FILE *fp = (FILE*) ctx;
    uint32_t old = ftell(fp);
    fseek(fp, 0, 2);
    long end = ftell(fp);
    fseek(fp, old, 0);
    return end <= 0 ? 0 : (uint32_t) end;
}

static const char *get_audio_ext(extractor_audio_format_t fmt)
{
    switch (fmt) {
        case EXTRACTOR_AUDIO_FORMAT_PCM:
            return "pcm";
        case EXTRACTOR_AUDIO_FORMAT_ADPCM:
            return "adpcm";
        case EXTRACTOR_AUDIO_FORMAT_AAC:
            return "aac";
        case EXTRACTOR_AUDIO_FORMAT_MP3:
            return "mp3";
        case EXTRACTOR_AUDIO_FORMAT_AC3:
            return "ac3";
        case EXTRACTOR_AUDIO_FORMAT_VORBIS:
            return "ogg";
        case EXTRACTOR_AUDIO_FORMAT_OPUS:
            return "opus";
        case EXTRACTOR_AUDIO_FORMAT_FLAC:
            return "flac";
        case EXTRACTOR_AUDIO_FORMAT_AMRNB:
        case EXTRACTOR_AUDIO_FORMAT_AMRWB:
            return "amr";
        default:
            return NULL;
    }
}

static const char *get_video_ext(extractor_video_format_t fmt)
{
    switch (fmt) {
        case EXTRACTOR_VIDEO_FORMAT_H264:
            return "264";
        case EXTRACTOR_VIDEO_FORMAT_MJPEG:
            return "mjpeg";
        default:
            return NULL;
    }
}

static void start_dump(extractor_test_t *ts)
{
    const char *audio_ext = get_audio_ext(ts->audio_format);
    char name[32];
    if (audio_ext) {
        snprintf(name, sizeof(name), MOUNT_PATH "audio.%s", audio_ext);
        ts->fp_dump_audio = fopen(name, "wb");
        if (ts->fp_dump_audio) {
            ESP_LOGI(TAG, "Dump audio to %s", name);
        }
    }
    const char *video_ext = get_video_ext(ts->video_format);
    if (video_ext) {
        snprintf(name, sizeof(name), MOUNT_PATH "video.%s", video_ext);
        ts->fp_dump_video = fopen(name, "wb");
        if (ts->fp_dump_video) {
            ESP_LOGI(TAG, "Dump video to %s", name);
        }
    }
}

static void stop_dump(extractor_test_t *ts)
{
    if (ts->fp_dump_audio) {
        fclose(ts->fp_dump_audio);
    }
    if (ts->fp_dump_video) {
        fclose(ts->fp_dump_video);
    }
}

static int _frame_reached(extractor_frame_info_t *frame, extractor_test_t *ts)
{
    switch (frame->stream_type) {
        case EXTRACTOR_STREAM_TYPE_AUDIO:
            if (ts->audio_pts == 0) {
                ESP_LOGI(TAG, "Audio start at %d", frame->pts);
            }
            if (frame->pts) {
                ts->audio_pts = frame->pts;
            }
            if (frame->eos) {
                ESP_LOGI(TAG, "Audio end at %d", ts->audio_pts);
            }
            if (ts->fp_dump_audio && frame->frame_buffer) {
                fwrite(frame->frame_buffer, frame->frame_size, 1, ts->fp_dump_audio);
            }
            break;
        case EXTRACTOR_STREAM_TYPE_VIDEO:
            if (ts->video_pts == 0) {
                ESP_LOGI(TAG, "Video start at %d", frame->pts);
            }
            if (frame->pts) {
                ts->video_pts = frame->pts;
            }
            if (frame->eos) {
                ESP_LOGI(TAG, "Video end at %d", ts->video_pts);
            }
            if (ts->fp_dump_video && frame->frame_buffer) {
                fwrite(frame->frame_buffer, frame->frame_size, 1, ts->fp_dump_video);
            }
            break;
        default:
            break;
    }
    // Frame buffer can be freed here or freed after unused at other places
    if (frame->frame_buffer) {
        mem_pool_free(esp_extractor_get_output_pool(ts->extractor), frame->frame_buffer);
    }
    return 0;
}

static int register_all_extractor()
{
    esp_extr_err_t ret;
    ret = esp_wav_extractor_register();
    ret = esp_mp4_extractor_register();
    ret = esp_ts_extractor_register();
    ret = esp_ogg_extractor_register();
    ret = esp_avi_extractor_register();
    ret = esp_audio_es_extractor_register();
    ret = esp_hls_extractor_register();
    RETURN_ON_FAIL(ret);
    return ret;
}

static void unregister_extractor()
{
    esp_extractor_unregister_all();
}

static int open_and_parse(extractor_test_t *ts, char *url)
{
    esp_extractor_config_t config = {
        .open = _wrapper_open,
        .read = _wrapper_read,
        .seek = _wrapper_seek,
        .file_size = _wrapper_file_size,
        .close = _wrapper_close,
        .extract_mask = ESP_EXTRACT_MASK_VIDEO | ESP_EXTRACT_MASK_AUDIO,
        .url = url,
        .input_ctx = ts,
    };
    int ret = esp_extractor_open(&config, &ts->extractor);
    RETURN_ON_FAIL(ret);
    ret = esp_extractor_parse_stream_info(ts->extractor);
    RETURN_ON_FAIL(ret);
    return 0;
}

static int get_stream_info(extractor_test_t *ts)
{
    uint16_t audio_num = 0;
    uint16_t video_num = 0;
    extractor_stream_info_t stream_info;
    int ret;
    ret = esp_extractor_get_stream_num(ts->extractor, EXTRACTOR_STREAM_TYPE_AUDIO, &audio_num);
    if (audio_num) {
        ret = esp_extractor_get_stream_info(ts->extractor, EXTRACTOR_STREAM_TYPE_AUDIO, 0, &stream_info);
        RETURN_ON_FAIL(ret);
        extractor_audio_stream_info_t *audio_info = &stream_info.stream_info.audio_info;
        ts->audio_format = audio_info->format;
        ESP_LOGI(TAG, "Audio codec:%d samplerate:%d channel:%d bits:%d dur:%d\n",
            audio_info->format,
            (int) audio_info->sample_rate, audio_info->channel, audio_info->bits_per_sample,
            (int) stream_info.duration);
    }
    ret = esp_extractor_get_stream_num(ts->extractor, EXTRACTOR_STREAM_TYPE_VIDEO, &video_num);
    if (video_num) {
        ret = esp_extractor_get_stream_info(ts->extractor, EXTRACTOR_STREAM_TYPE_VIDEO, 0, &stream_info);
        RETURN_ON_FAIL(ret);
        extractor_video_stream_info_t *video_info = &stream_info.stream_info.video_info;
        ts->video_format = video_info->format;
        ESP_LOGI(TAG, "Video format:%d size:%dx%d fps:%d dur:%d\n",
            video_info->format,
            (int) video_info->width, (int)video_info->height, video_info->fps,
            (int) stream_info.duration);
    }
    return 0;
}

static void clear_up(extractor_test_t *ts)
{
    stop_dump(ts);
    if (ts->extractor) {
        esp_extractor_close(ts->extractor);
        ts->extractor = NULL;
    }
    unregister_extractor();
}

int extractor_test(char *url, int start_pos)
{
    int ret;
    extractor_test_t ts;
    memset(&ts, 0, sizeof(extractor_test_t));
    do {
        ret = register_all_extractor();
        BREAK_ON_FAIL(ret);
        ret = open_and_parse(&ts, url);
        BREAK_ON_FAIL(ret);
        get_stream_info(&ts);
        ret = esp_extractor_seek(ts.extractor, start_pos);
        BREAK_ON_FAIL(ret);
        start_dump(&ts);
        while (ret == ESP_EXTR_ERR_OK) {
            extractor_frame_info_t frame = {0};
            ret = esp_extractor_read_frame(ts.extractor, &frame);
            if (ret >= 0) {
                _frame_reached(&frame, &ts);

            }
        }
    } while (0);
    clear_up(&ts);
    return ret;
}
```

## Change log


### Version 1.0.0

- Added MP4, TS, FLV, WAV, OGG, HLS, AVI, Audio ES(MP3, AAC, AMR, FLAC) container support
