#ifndef MIPK_VIDEO_H
#define MIPK_VIDEO_H

#include "media_engine/media.h"
#include "mipk.h"
#include "mipk_bytetrack.h"
#include "mipk_nms.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MIPK_VIDEO_OK = 0,
    MIPK_VIDEO_AGAIN = 1,
    MIPK_VIDEO_ERR_ARGUMENT = -1,
    MIPK_VIDEO_ERR_MEMORY = -2,
    MIPK_VIDEO_ERR_MODEL = -3,
    MIPK_VIDEO_ERR_MEDIA = -4,
} mipk_video_status;

typedef struct {
    const char *package_path;
} mipk_video_config;

typedef struct {
    const mipk_pipeline *pipeline;
    const mipk_output *output;
    uint64_t pts_us;
    uint32_t width;
    uint32_t height;
    double inference_ms;
} mipk_video_event;

typedef struct mipk_video mipk_video;

void mipk_video_config_init(mipk_video_config *config);
mipk_video_status mipk_video_open(const mipk_video_config *config,
    mipk_video **video);
void mipk_video_close(mipk_video *video);
const mipk_pipeline_info *mipk_video_get_pipeline_info(const mipk_video *video);
size_t mipk_video_runtime_option_count(const mipk_video *video);
mipk_status mipk_video_runtime_option_at(const mipk_video *video, size_t index,
    mipk_runtime_option_info *info);
mipk_status mipk_video_runtime_option_value_at(const mipk_video *video,
    size_t index, char *buffer, size_t capacity, size_t *required_size);
mipk_status mipk_video_update_node_options(mipk_video *video,
    const char *node_id, const char *options_json);
mipk_status mipk_video_update_nms(mipk_video *video, const char *node_id,
    const mipk_nms_runtime_options *options);
mipk_status mipk_video_update_bytetrack(mipk_video *video,
    const char *node_id, const mipk_bytetrack_runtime_options *options);
mipk_status mipk_video_apply_overrides_json(mipk_video *video,
    const char *overrides_json);
mipk_status mipk_video_load_overrides(mipk_video *video, const char *path);

mipk_video_status mipk_video_configure_stream(const mipk_video *video,
    media_video_stream_config *stream, uint32_t camera_id,
    uint32_t frame_rate, media_video_output output);
mipk_video_status mipk_video_process(mipk_video *video,
    media_context *media, uint32_t stream_id, int timeout_ms,
    mipk_video_event *event);
size_t mipk_video_results_to_boxes(const mipk_video_event *event,
    media_video_box *boxes, size_t capacity);

const char *mipk_video_status_string(mipk_video_status status);

#ifdef __cplusplus
}
#endif
#endif
