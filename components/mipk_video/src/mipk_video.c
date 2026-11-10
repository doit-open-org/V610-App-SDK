#define _POSIX_C_SOURCE 200809L

#include "mipk_video.h"

#include "mipk_platform_hisilicon.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

struct mipk_video {
    /*
     * The board's platform: MMZ buffers, cache maintenance and one NPU
     * arbiter shared by every pipeline in the process.  Hardware scaling is
     * not wired in yet; when the media layer offers a VGS path it goes into
     * mipk_hisilicon_platform_options.image_backend here, not into mipk.
     */
    mipk_hisilicon_platform *platform;
    mipk_pipeline *pipeline;
    mipk_output *output;
    const mipk_pipeline_info *info;
    /* Why the last open, run or update was refused; see mipk_diag(). */
    char diagnostic[1024];
};

static bool pipeline_accepts_video(const mipk_pipeline_info *info)
{
    if (info == NULL || info->input_format != MIPK_FORMAT_NV12 ||
        info->input_width == 0 || info->input_height == 0 ||
        (info->input_width & 1U) != 0 || (info->input_height & 1U) != 0) {
        return false;
    }
    if ((size_t)info->input_width > SIZE_MAX / info->input_height) {
        return false;
    }
    const size_t pixels = (size_t)info->input_width * info->input_height;
    return pixels <= SIZE_MAX - pixels / 2U &&
        info->input_size == pixels + pixels / 2U;
}

static double elapsed_ms(const struct timespec *begin,
    const struct timespec *end)
{
    const double seconds = (double)(end->tv_sec - begin->tv_sec);
    const double nanoseconds = (double)(end->tv_nsec - begin->tv_nsec);
    return seconds * 1000.0 + nanoseconds / 1000000.0;
}

void mipk_video_config_init(mipk_video_config *config)
{
    if (config == NULL) {
        return;
    }
    *config = (mipk_video_config) {0};
}

mipk_video_status mipk_video_open(const mipk_video_config *config,
    mipk_video **output)
{
    if (config == NULL || config->package_path == NULL ||
        config->package_path[0] == '\0' || output == NULL) {
        return MIPK_VIDEO_ERR_ARGUMENT;
    }
    *output = NULL;
    mipk_video *video = calloc(1, sizeof(*video));
    if (video == NULL) {
        return MIPK_VIDEO_ERR_MEMORY;
    }
    const mipk_hisilicon_platform_options platform_options = {
        .struct_size = sizeof(platform_options),
    };
    mipk_status status = mipk_platform_hisilicon_create(&platform_options,
        &video->platform);
    if (status != MIPK_OK) {
        fprintf(stderr, "create hisilicon platform failed: %s\n",
            mipk_status_string(status));
        mipk_video_close(video);
        return MIPK_VIDEO_ERR_MEMORY;
    }
    const mipk_pipeline_open_options open_options = {
        .struct_size = sizeof(open_options),
        .max_output_handles = 1,
        .platform = mipk_platform_hisilicon_context(video->platform),
        .diagnostic = video->diagnostic,
        .diagnostic_size = sizeof(video->diagnostic),
    };
    status = mipk_pipeline_open(config->package_path,
        &open_options, &video->pipeline);
    if (status != MIPK_OK) {
        fprintf(stderr, "cannot open %s: %s\n%s\n", config->package_path,
            mipk_status_string(status), video->diagnostic);
        mipk_video_close(video);
        return MIPK_VIDEO_ERR_MODEL;
    }
    video->info = mipk_pipeline_get_info(video->pipeline);
    if (!pipeline_accepts_video(video->info)) {
        mipk_video_close(video);
        return MIPK_VIDEO_ERR_MODEL;
    }
    status = mipk_pipeline_output_create(video->pipeline, &video->output);
    if (status != MIPK_OK) {
        mipk_video_close(video);
        return status == MIPK_ERR_MEMORY ? MIPK_VIDEO_ERR_MEMORY :
            MIPK_VIDEO_ERR_MODEL;
    }
    *output = video;
    return MIPK_VIDEO_OK;
}

void mipk_video_close(mipk_video *video)
{
    if (video == NULL) {
        return;
    }
    mipk_output_destroy(video->output);
    (void)mipk_pipeline_close(video->pipeline);
    if (video->platform != NULL) {
        (void)mipk_platform_hisilicon_destroy(video->platform);
    }
    free(video);
}

const mipk_pipeline_info *mipk_video_get_pipeline_info(const mipk_video *video)
{
    return video == NULL ? NULL : video->info;
}

size_t mipk_video_runtime_option_count(const mipk_video *video)
{
    return video == NULL ? 0 :
        mipk_pipeline_runtime_option_count(video->pipeline);
}

mipk_status mipk_video_runtime_option_at(const mipk_video *video, size_t index,
    mipk_runtime_option_info *info)
{
    return video == NULL ? MIPK_ERR_ARGUMENT :
        mipk_pipeline_runtime_option_at(video->pipeline, index, info);
}

mipk_status mipk_video_runtime_option_value_at(const mipk_video *video,
    size_t index, char *buffer, size_t capacity, size_t *required_size)
{
    return video == NULL ? MIPK_ERR_ARGUMENT :
        mipk_pipeline_runtime_option_value_at(video->pipeline, index, buffer,
            capacity, required_size);
}

mipk_status mipk_video_update_node_options(mipk_video *video,
    const char *node_id, const char *options_json)
{
    return video == NULL ? MIPK_ERR_ARGUMENT :
        mipk_pipeline_update_node_options(video->pipeline, node_id,
            options_json);
}

mipk_status mipk_video_update_nms(mipk_video *video, const char *node_id,
    const mipk_nms_runtime_options *options)
{
    return video == NULL ? MIPK_ERR_ARGUMENT :
        mipk_pipeline_update_nms(video->pipeline, node_id, options);
}

mipk_status mipk_video_update_bytetrack(mipk_video *video,
    const char *node_id, const mipk_bytetrack_runtime_options *options)
{
    return video == NULL ? MIPK_ERR_ARGUMENT :
        mipk_pipeline_update_bytetrack(video->pipeline, node_id, options);
}

mipk_status mipk_video_apply_overrides_json(mipk_video *video,
    const char *overrides_json)
{
    return video == NULL ? MIPK_ERR_ARGUMENT :
        mipk_pipeline_apply_overrides_json(video->pipeline, overrides_json);
}

mipk_status mipk_video_load_overrides(mipk_video *video, const char *path)
{
    return video == NULL ? MIPK_ERR_ARGUMENT :
        mipk_pipeline_load_overrides(video->pipeline, path);
}

mipk_video_status mipk_video_configure_stream(const mipk_video *video,
    media_video_stream_config *stream, uint32_t camera_id,
    uint32_t frame_rate, media_video_output output)
{
    if (video == NULL || stream == NULL || frame_rate == 0 ||
        (output != MEDIA_VIDEO_OUTPUT_FRAME &&
            output != MEDIA_VIDEO_OUTPUT_H264 &&
            output != MEDIA_VIDEO_OUTPUT_H265)) {
        return MIPK_VIDEO_ERR_ARGUMENT;
    }
    *stream = (media_video_stream_config) {
        .camera_id = camera_id,
        .output = output,
        .width = video->info->input_width,
        .height = video->info->input_height,
        .frame_rate = frame_rate,
    };
    return MIPK_VIDEO_OK;
}

mipk_video_status mipk_video_process(mipk_video *video,
    media_context *media, uint32_t stream_id, int timeout_ms,
    mipk_video_event *event)
{
    if (video == NULL || media == NULL || timeout_ms < 0 || event == NULL) {
        return MIPK_VIDEO_ERR_ARGUMENT;
    }
    *event = (mipk_video_event) {0};
    media_video_frame *frame = NULL;
    media_status media_result = media_video_acquire_frame(media, stream_id,
        timeout_ms, &frame);
    if (media_result == MEDIA_ERR_TIMEOUT) {
        return MIPK_VIDEO_AGAIN;
    }
    if (media_result != MEDIA_OK) {
        return MIPK_VIDEO_ERR_MEDIA;
    }
    media_video_frame_info frame_info;
    media_result = media_video_frame_get_info(frame, &frame_info);
    if (media_result == MEDIA_OK &&
        (frame_info.width != video->info->input_width ||
            frame_info.height != video->info->input_height ||
            media_video_frame_packed_size(frame) != video->info->input_size ||
            frame_info.planes[0] == NULL || frame_info.planes[1] == NULL ||
            frame_info.stride[0] < frame_info.width ||
            frame_info.stride[1] < frame_info.width ||
            frame_info.stride[0] > SIZE_MAX / frame_info.height ||
            frame_info.stride[1] > SIZE_MAX / (frame_info.height / 2U))) {
        media_result = MEDIA_ERR_FORMAT;
    }
    if (media_result != MEDIA_OK) {
        media_video_release_frame(media, frame);
        return MIPK_VIDEO_ERR_MEDIA;
    }

    struct timespec begin;
    struct timespec end;
    (void)clock_gettime(CLOCK_MONOTONIC, &begin);
    const mipk_input input = {
        .type = MIPK_INPUT_VIDEO_FRAME,
        .pts_us = frame_info.pts,
        .value.video = {
            .format = MIPK_FORMAT_NV12,
            .width = frame_info.width,
            .height = frame_info.height,
            .plane_count = 2,
            .planes = {{
                .buffer = {
                    .virtual_address = (void *)frame_info.planes[0],
                    .physical_address = MIPK_PHYSICAL_ADDRESS_INVALID,
                    .size = (size_t)frame_info.stride[0] * frame_info.height,
                    .alignment = 1,
                    .memory_type = MIPK_MEMORY_CPU,
                    .owner = MIPK_BUFFER_OWNER_APPLICATION,
                },
                .initial_sync_state = MIPK_BUFFER_SYNCED,
                .stride = frame_info.stride[0],
            }, {
                .buffer = {
                    .virtual_address = (void *)frame_info.planes[1],
                    .physical_address = MIPK_PHYSICAL_ADDRESS_INVALID,
                    .size = (size_t)frame_info.stride[1] *
                        (frame_info.height / 2U),
                    .alignment = 1,
                    .memory_type = MIPK_MEMORY_CPU,
                    .owner = MIPK_BUFFER_OWNER_APPLICATION,
                },
                .initial_sync_state = MIPK_BUFFER_SYNCED,
                .stride = frame_info.stride[1],
            }},
        },
    };
    const mipk_status model_result = mipk_pipeline_run_output(video->pipeline,
        &input, video->output);
    media_video_release_frame(media, frame);
    (void)clock_gettime(CLOCK_MONOTONIC, &end);
    if (model_result != MIPK_OK && model_result != MIPK_ERR_CAPACITY) {
        return MIPK_VIDEO_ERR_MODEL;
    }
    *event = (mipk_video_event) {
        .pipeline = video->pipeline,
        .output = video->output,
        .pts_us = frame_info.pts,
        .width = video->info->input_width,
        .height = video->info->input_height,
        .inference_ms = elapsed_ms(&begin, &end),
    };
    return MIPK_VIDEO_OK;
}

static float normalize_coordinate(float value, uint32_t extent)
{
    if (value <= 0.0F) {
        return 0.0F;
    }
    if (value >= (float)extent) {
        return 1.0F;
    }
    return value / (float)extent;
}

size_t mipk_video_results_to_boxes(const mipk_video_event *event,
    media_video_box *boxes, size_t capacity)
{
    if (event == NULL || event->output == NULL ||
        event->width == 0 || event->height == 0 ||
        (capacity != 0 && boxes == NULL)) {
        return 0;
    }
    size_t count = 0;
    const size_t result_count = mipk_output_result_count(event->output);
    for (size_t index = 0; index < result_count &&
        count < capacity; ++index) {
        const mipk_result *item = mipk_output_result_at(event->output, index);
        if (item->type != MIPK_RESULT_DETECTION &&
            item->type != MIPK_RESULT_TRACK) {
            continue;
        }
        const mipk_bbox *bbox = item->type == MIPK_RESULT_TRACK ?
            &item->value.track.bbox : &item->value.bbox;
        const float x1 = normalize_coordinate(bbox->x1,
            event->width);
        const float y1 = normalize_coordinate(bbox->y1,
            event->height);
        const float x2 = normalize_coordinate(bbox->x2,
            event->width);
        const float y2 = normalize_coordinate(bbox->y2,
            event->height);
        if (x1 >= x2 || y1 >= y2) {
            continue;
        }
        boxes[count++] = (media_video_box) {
            .x1 = x1,
            .y1 = y1,
            .x2 = x2,
            .y2 = y2,
            .red = 0,
            .green = 255,
            .blue = 0,
            .thickness = 2,
        };
    }
    return count;
}

const char *mipk_video_status_string(mipk_video_status status)
{
    switch (status) {
        case MIPK_VIDEO_OK: return "success";
        case MIPK_VIDEO_AGAIN: return "no frame available";
        case MIPK_VIDEO_ERR_ARGUMENT: return "invalid argument";
        case MIPK_VIDEO_ERR_MEMORY: return "out of memory";
        case MIPK_VIDEO_ERR_MODEL: return "model error";
        case MIPK_VIDEO_ERR_MEDIA: return "media error";
        default: return "unknown error";
    }
}
