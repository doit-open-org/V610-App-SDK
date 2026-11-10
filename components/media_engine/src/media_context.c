#include "media_internal.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#define MEDIA_I2C_8BIT_ADDRESS_MIN 0x06U
#define MEDIA_I2C_8BIT_ADDRESS_MAX 0xEEU
#define MEDIA_I2C_BUS_COUNT 2U

static media_status media_validate_stream(const media_video_stream_config *stream,
    const media_video_config *video)
{
    if (stream->camera_id >= video->camera_count) {
        return MEDIA_ERR_ARGUMENT;
    }
    uint32_t sensor_frame_rate = video->cameras[stream->camera_id].frame_rate;
    if (stream->width == 0 || stream->height == 0 ||
        (stream->width & 1U) != 0 || (stream->height & 1U) != 0 ||
        stream->width > MEDIA_SENSOR_WIDTH || stream->height > MEDIA_SENSOR_HEIGHT ||
        stream->frame_rate == 0 || stream->frame_rate > sensor_frame_rate) {
        return MEDIA_ERR_ARGUMENT;
    }
    if (stream->output < MEDIA_VIDEO_OUTPUT_FRAME || stream->output > MEDIA_VIDEO_OUTPUT_H265) {
        return MEDIA_ERR_ARGUMENT;
    }
    if (stream->output != MEDIA_VIDEO_OUTPUT_FRAME &&
        (stream->bit_rate_kbps == 0 || stream->gop == 0 || stream->gop > 65536U)) {
        return MEDIA_ERR_ARGUMENT;
    }
    return MEDIA_OK;
}

static media_status media_validate_config(const media_config *config)
{
    if (config == NULL) {
        return MEDIA_ERR_ARGUMENT;
    }
    if (!config->video.enabled && !config->audio_input.enabled &&
        !config->audio_output.enabled) {
        return MEDIA_ERR_ARGUMENT;
    }
    if (config->video.enabled) {
        if (config->video.camera_count == 0 ||
            config->video.camera_count > MEDIA_VIDEO_MAX_CAMERAS ||
            config->video.stream_count == 0 ||
            config->video.stream_count > MEDIA_VIDEO_MAX_STREAMS) {
            return MEDIA_ERR_ARGUMENT;
        }
        size_t streams_per_camera[MEDIA_VIDEO_MAX_CAMERAS] = {0};
        bool devices_used[MEDIA_VIDEO_MAX_CAMERAS] = {false};
        for (size_t i = 0; i < config->video.camera_count; i++) {
            const media_video_camera_config *camera = &config->video.cameras[i];
            media_video_rotation rotation = camera->rotation;
            bool has_custom_i2c_address = camera->i2c.address != 0U;
            if (camera->device_id >= MEDIA_VIDEO_MAX_CAMERAS ||
                devices_used[camera->device_id] ||
                camera->i2c.bus_id >= MEDIA_I2C_BUS_COUNT ||
                (uint32_t)camera->sensor_type >= (uint32_t)MEDIA_VIDEO_SENSOR_TYPE_COUNT ||
                (has_custom_i2c_address &&
                (camera->i2c.address < MEDIA_I2C_8BIT_ADDRESS_MIN ||
                camera->i2c.address > MEDIA_I2C_8BIT_ADDRESS_MAX ||
                (camera->i2c.address & 1U) != 0U)) || camera->frame_rate == 0 ||
                (rotation != MEDIA_VIDEO_ROTATION_0 && rotation != MEDIA_VIDEO_ROTATION_90 &&
                rotation != MEDIA_VIDEO_ROTATION_180 && rotation != MEDIA_VIDEO_ROTATION_270)) {
                return MEDIA_ERR_ARGUMENT;
            }
            devices_used[camera->device_id] = true;
        }
        for (size_t i = 0; i < config->video.stream_count; i++) {
            const media_video_stream_config *stream = &config->video.streams[i];
            media_status status = media_validate_stream(stream, &config->video);
            if (status != MEDIA_OK) {
                return status;
            }
            streams_per_camera[stream->camera_id]++;
            if (streams_per_camera[stream->camera_id] > MEDIA_VIDEO_MAX_STREAMS_PER_CAMERA) {
                return MEDIA_ERR_ARGUMENT;
            }
        }
        for (size_t i = 0; i < config->video.camera_count; i++) {
            if (streams_per_camera[i] == 0) {
                return MEDIA_ERR_ARGUMENT;
            }
        }
    }
    if (config->audio_input.enabled &&
        (config->audio_input.input < MEDIA_AUDIO_INPUT_IN0 ||
        config->audio_input.input > MEDIA_AUDIO_INPUT_DIFFERENTIAL ||
        config->audio_input.sample_rate != 16000 || config->audio_input.frame_samples == 0 ||
        config->audio_input.frame_samples > MEDIA_AUDIO_MAX_FRAME_SAMPLES ||
        config->audio_input.physical_channel > 1 || config->audio_input.gain_db < -78 ||
        config->audio_input.gain_db > 80 ||
        (config->audio_input.use_talk_v2 && !config->audio_input.enable_aec))) {
        return MEDIA_ERR_UNSUPPORTED;
    }
    if (config->audio_output.enabled &&
        (config->audio_output.sample_rate != 16000 || config->audio_output.frame_samples == 0 ||
        config->audio_output.frame_samples > MEDIA_AUDIO_MAX_FRAME_SAMPLES ||
        config->audio_output.max_buffer_size < 4096 || config->audio_output.gain_db < -121 ||
        config->audio_output.gain_db > 6)) {
        return MEDIA_ERR_UNSUPPORTED;
    }
    if (config->audio_input.enabled && config->audio_output.enabled &&
        config->audio_input.enable_aec &&
        (config->audio_input.sample_rate != config->audio_output.sample_rate ||
        config->audio_input.frame_samples != config->audio_output.frame_samples)) {
        return MEDIA_ERR_ARGUMENT;
    }
    return MEDIA_OK;
}

void media_config_init(media_config *config)
{
    if (config == NULL) {
        return;
    }
    *config = (media_config) {0};
}

void media_video_config_init(media_video_config *config)
{
    if (config == NULL) {
        return;
    }
    *config = (media_video_config) {
        .enabled = true,
        .camera_count = 1,
        .cameras = {
            {
                .frame_rate = 30,
                .device_id = 0,
                .i2c = {
                    .bus_id = 0,
                    .address = MEDIA_CAMERA0_I2C_ADDRESS,
                },
                .sensor_type = MEDIA_VIDEO_SENSOR_GC4023,
            },
            {
                .frame_rate = 30,
                .device_id = 1,
                .i2c = {
                    .bus_id = 1,
                    .address = MEDIA_CAMERA1_I2C_ADDRESS,
                },
                .sensor_type = MEDIA_VIDEO_SENSOR_GC4023,
            },
        },
        .stream_count = 1,
        .streams = {
            {
                .output = MEDIA_VIDEO_OUTPUT_H264,
                .width = 1920,
                .height = 1080,
                .frame_rate = 25,
                .bit_rate_kbps = 2500,
                .gop = 50,
            },
        },
    };
}

void media_audio_input_config_init(media_audio_input_config *config)
{
    if (config == NULL) {
        return;
    }
    *config = (media_audio_input_config) {
        .enabled = true,
        .input = MEDIA_AUDIO_INPUT_IN1,
        .physical_channel = 0,
        .gain_db = 50,
        .sample_rate = 16000,
        .frame_samples = 160,
        .enable_aec = true,
        .use_talk_v2 = false,
    };
}

void media_audio_output_config_init(media_audio_output_config *config)
{
    if (config == NULL) {
        return;
    }
    *config = (media_audio_output_config) {
        .enabled = true,
        .gain_db = -10,
        .sample_rate = 16000,
        .frame_samples = 160,
        .max_buffer_size = 64U * 1024U,
    };
}

void media_audio_encoder_config_init(media_audio_encoder_config *config)
{
    if (config == NULL) {
        return;
    }
    *config = (media_audio_encoder_config) {
        .codec = MEDIA_AUDIO_CODEC_OPUS,
        .sample_rate = 16000,
        .channels = 1,
        .bit_rate = 24000,
        .frame_samples = 160,
    };
}

void media_audio_decoder_config_init(media_audio_decoder_config *config)
{
    if (config == NULL) {
        return;
    }
    *config = (media_audio_decoder_config) {
        .codec = MEDIA_AUDIO_CODEC_OPUS,
        .sample_rate = 16000,
        .channels = 1,
        .buffer_size = 64U * 1024U,
    };
}

bool media_make_deadline(int timeout_ms, struct timespec *deadline)
{
    if (deadline == NULL || timeout_ms < 0 || clock_gettime(CLOCK_REALTIME, deadline) != 0) {
        return false;
    }
    deadline->tv_sec += timeout_ms / 1000;
    deadline->tv_nsec += (long)(timeout_ms % 1000) * 1000000L;
    if (deadline->tv_nsec >= 1000000000L) {
        deadline->tv_sec++;
        deadline->tv_nsec -= 1000000000L;
    }
    return true;
}

media_status media_copy_file(const char *path, uint8_t **data, size_t *size)
{
    if (path == NULL || path[0] == '\0' || data == NULL || size == NULL) {
        return MEDIA_ERR_ARGUMENT;
    }
    *data = NULL;
    *size = 0;
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        return MEDIA_ERR_IO;
    }
    struct stat info;
    if (fstat(fileno(file), &info) != 0 || info.st_size <= 0 ||
        (uintmax_t)info.st_size > SIZE_MAX) {
        fclose(file);
        return MEDIA_ERR_IO;
    }
    size_t file_size = (size_t)info.st_size;
    uint8_t *buffer = malloc(file_size);
    if (buffer == NULL) {
        fclose(file);
        return MEDIA_ERR_MEMORY;
    }
    size_t bytes_read = fread(buffer, 1, file_size, file);
    int close_result = fclose(file);
    if (bytes_read != file_size || close_result != 0) {
        free(buffer);
        return MEDIA_ERR_IO;
    }
    *data = buffer;
    *size = file_size;
    return MEDIA_OK;
}

media_status media_engine_open(const media_config *config, media_context **output)
{
    if (output == NULL) {
        return MEDIA_ERR_ARGUMENT;
    }
    *output = NULL;
    media_status status = media_validate_config(config);
    if (status != MEDIA_OK) {
        return status;
    }
    media_context *context = calloc(1, sizeof(*context));
    if (context == NULL) {
        return MEDIA_ERR_MEMORY;
    }
    context->config = *config;
    hi_mapi_sys_base_attr base = {0};
    status = media_video_prepare_system(config, &base);
    if (status != MEDIA_OK) {
        free(context);
        return status;
    }
    if (hi_mapi_sys_init() != TD_SUCCESS) {
        free(context);
        return MEDIA_ERR_DEVICE;
    }
    context->sys_initialized = true;

    hi_mapi_sys_media_attr media_attr = {0};
    if (hi_mapi_sys_get_default_media_attr(&base, &media_attr) != TD_SUCCESS) {
        status = MEDIA_ERR_DEVICE;
        goto failed;
    }
    media_attr.audio_mod_para.audio_bypass =
        (config->audio_input.enabled || config->audio_output.enabled) ? TD_FALSE : TD_TRUE;
    if (hi_mapi_sys_init_media(&media_attr) != TD_SUCCESS) {
        status = MEDIA_ERR_DEVICE;
        goto failed;
    }
    context->media_initialized = true;

    *output = context;
    return MEDIA_OK;

failed:
    media_engine_close(context);
    return status;
}

media_status media_video_open(media_context *context)
{
    if (context == NULL || !context->media_initialized ||
        !context->config.video.enabled || context->video.initialized) {
        return MEDIA_ERR_STATE;
    }
    return media_video_start(context);
}

media_status media_video_close(media_context *context)
{
    if (context == NULL || !context->video.initialized) {
        return MEDIA_ERR_STATE;
    }
    for (size_t i = 0; i < context->video.stream_count; i++) {
        if (context->video.streams[i].frame_outstanding) {
            return MEDIA_ERR_BUSY;
        }
    }
    media_video_stop(context);
    return MEDIA_OK;
}

media_status media_audio_output_open(media_context *context)
{
    if (context == NULL || !context->media_initialized ||
        !context->config.audio_output.enabled || context->player.initialized) {
        return MEDIA_ERR_STATE;
    }
    return media_player_start(context);
}

media_status media_audio_output_close(media_context *context)
{
    if (context == NULL || !context->player.initialized) {
        return MEDIA_ERR_STATE;
    }
    if (context->player.decoder != NULL ||
        context->player.main_source.kind == MEDIA_SOURCE_PLAYER) {
        return MEDIA_ERR_BUSY;
    }
    media_player_stop(context);
    return MEDIA_OK;
}

media_status media_audio_input_open(media_context *context)
{
    if (context == NULL || !context->media_initialized ||
        !context->config.audio_input.enabled || context->capture.initialized) {
        return MEDIA_ERR_STATE;
    }
    return media_capture_start(context);
}

media_status media_audio_input_close(media_context *context)
{
    if (context == NULL || !context->capture.initialized) {
        return MEDIA_ERR_STATE;
    }
    if (context->capture.encoder != NULL) {
        return MEDIA_ERR_BUSY;
    }
    media_capture_stop(context);
    return MEDIA_OK;
}

void media_engine_close(media_context *context)
{
    if (context == NULL) {
        return;
    }
    context->closing = true;
    media_capture_stop(context);
    media_player_stop(context);
    media_video_stop(context);
    if (context->media_initialized) {
        (void)hi_mapi_sys_deinit_media();
        context->media_initialized = false;
    }
    if (context->sys_initialized) {
        (void)hi_mapi_sys_deinit();
        context->sys_initialized = false;
    }
    free(context);
}

const char *media_status_string(media_status status)
{
    switch (status) {
        case MEDIA_OK: return "ok";
        case MEDIA_AGAIN: return "try again";
        case MEDIA_EOF: return "end of stream";
        case MEDIA_ERR_ARGUMENT: return "invalid argument";
        case MEDIA_ERR_STATE: return "invalid state";
        case MEDIA_ERR_MEMORY: return "out of memory";
        case MEDIA_ERR_DEVICE: return "device error";
        case MEDIA_ERR_FORMAT: return "invalid media format";
        case MEDIA_ERR_TIMEOUT: return "timeout";
        case MEDIA_ERR_BUSY: return "busy";
        case MEDIA_ERR_UNSUPPORTED: return "unsupported";
        case MEDIA_ERR_IO: return "I/O error";
        default: return "unknown media status";
    }
}
