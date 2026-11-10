#ifndef MIPK_AUDIO_H
#define MIPK_AUDIO_H

#include "media_engine/media.h"
#include "mipk.h"
#include "mipk_wake_word_trigger.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MIPK_AUDIO_MAX_CAPTURE_SAMPLES 960U

typedef enum {
    MIPK_AUDIO_OK = 0,
    MIPK_AUDIO_AGAIN = 1,
    MIPK_AUDIO_ERR_ARGUMENT = -1,
    MIPK_AUDIO_ERR_MEMORY = -2,
    MIPK_AUDIO_ERR_MODEL = -3,
    MIPK_AUDIO_ERR_MEDIA = -4,
} mipk_audio_status;

typedef struct {
    const char *package_path;
} mipk_audio_config;

typedef struct {
    const mipk_pipeline *pipeline;
    const mipk_output *output;
    const int16_t *samples;
    size_t sample_count;
    size_t capture_offset_samples;
    uint64_t pts_us;
} mipk_audio_event;

typedef struct mipk_audio mipk_audio;
typedef struct mipk_audio_prompt mipk_audio_prompt;
typedef void (*mipk_audio_event_callback)(const mipk_audio_event *event,
    void *user_data);

void mipk_audio_config_init(mipk_audio_config *config);
mipk_audio_status mipk_audio_open(const mipk_audio_config *config,
    mipk_audio **audio);
void mipk_audio_close(mipk_audio *audio);
const mipk_pipeline_info *mipk_audio_get_pipeline_info(const mipk_audio *audio);
size_t mipk_audio_runtime_option_count(const mipk_audio *audio);
mipk_status mipk_audio_runtime_option_at(const mipk_audio *audio, size_t index,
    mipk_runtime_option_info *info);
mipk_status mipk_audio_runtime_option_value_at(const mipk_audio *audio,
    size_t index, char *buffer, size_t capacity, size_t *required_size);
mipk_status mipk_audio_update_node_options(mipk_audio *audio,
    const char *node_id, const char *options_json);
mipk_status mipk_audio_update_wake_word(mipk_audio *audio,
    const char *node_id, const mipk_wake_word_runtime_options *options);
mipk_status mipk_audio_apply_overrides_json(mipk_audio *audio,
    const char *overrides_json);
mipk_status mipk_audio_load_overrides(mipk_audio *audio, const char *path);

mipk_audio_status mipk_audio_configure_input(const mipk_audio *audio,
    media_audio_input_config *input, uint32_t capture_frame_samples,
    uint32_t physical_channel, int gain_db, bool enable_aec);
mipk_audio_status mipk_audio_configure_output(const mipk_audio *audio,
    media_audio_output_config *output, uint32_t output_frame_samples,
    int gain_db);
mipk_audio_status mipk_audio_poll(mipk_audio *audio, media_context *media,
    int timeout_ms, mipk_audio_event_callback callback, void *user_data);

mipk_audio_status mipk_audio_play_pcm(media_context *media,
    const int16_t *samples, size_t sample_count, int timeout_ms);
mipk_audio_status mipk_audio_play_memory(media_context *media,
    media_audio_format format, const void *data, size_t size);
mipk_audio_status mipk_audio_play_file(media_context *media,
    media_audio_format format, const char *path);
mipk_audio_status mipk_audio_stop_playback(media_context *media);
mipk_audio_status mipk_audio_prompt_load(media_context *media,
    media_audio_format format, const void *data, size_t size,
    mipk_audio_prompt **prompt);
mipk_audio_status mipk_audio_prompt_play(media_context *media,
    const mipk_audio_prompt *prompt, media_prompt_policy policy);
void mipk_audio_prompt_close(mipk_audio_prompt *prompt);

const char *mipk_audio_status_string(mipk_audio_status status);

#ifdef __cplusplus
}
#endif
#endif
