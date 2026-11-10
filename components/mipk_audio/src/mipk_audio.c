#include "mipk_audio.h"

#include "mipk_platform_hisilicon.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct mipk_audio {
    /* Board platform: NPU arbiter shared with any video pipeline. */
    mipk_hisilicon_platform *platform;
    char diagnostic[1024];
    mipk_pipeline *pipeline;
    mipk_output *output;
    int16_t capture[MIPK_AUDIO_MAX_CAPTURE_SAMPLES];
    int16_t *model_frame;
    size_t model_frame_used;
    size_t model_frame_capture_offset;
    uint64_t model_frame_pts;
    const mipk_pipeline_info *info;
};

struct mipk_audio_prompt {
    media_audio_prompt *media_prompt;
};

static bool pipeline_accepts_audio(const mipk_pipeline_info *info)
{
    if (info == NULL || info->input_format != MIPK_FORMAT_PCM_S16LE ||
        info->input_sample_rate == 0 || info->input_channels != 1 ||
        info->input_frame_samples == 0 ||
        (size_t)info->input_frame_samples > SIZE_MAX / sizeof(int16_t)) {
        return false;
    }
    return info->input_size ==
        (size_t)info->input_frame_samples * sizeof(int16_t);
}

static mipk_audio_status media_result(media_status status)
{
    if (status == MEDIA_OK) {
        return MIPK_AUDIO_OK;
    }
    if (status == MEDIA_AGAIN || status == MEDIA_ERR_TIMEOUT) {
        return MIPK_AUDIO_AGAIN;
    }
    return status == MEDIA_ERR_MEMORY ? MIPK_AUDIO_ERR_MEMORY :
        MIPK_AUDIO_ERR_MEDIA;
}

void mipk_audio_config_init(mipk_audio_config *config)
{
    if (config == NULL) {
        return;
    }
    *config = (mipk_audio_config) {0};
}

mipk_audio_status mipk_audio_open(const mipk_audio_config *config,
    mipk_audio **output)
{
    if (config == NULL || config->package_path == NULL ||
        config->package_path[0] == '\0' || output == NULL) {
        return MIPK_AUDIO_ERR_ARGUMENT;
    }
    *output = NULL;
    mipk_audio *audio = calloc(1, sizeof(*audio));
    if (audio == NULL) {
        return MIPK_AUDIO_ERR_MEMORY;
    }
    const mipk_hisilicon_platform_options platform_options = {
        .struct_size = sizeof(platform_options),
    };
    mipk_status status = mipk_platform_hisilicon_create(&platform_options,
        &audio->platform);
    if (status != MIPK_OK) {
        fprintf(stderr, "create hisilicon platform failed: %s\n",
            mipk_status_string(status));
        mipk_audio_close(audio);
        return MIPK_AUDIO_ERR_MEMORY;
    }
    const mipk_pipeline_open_options open_options = {
        .struct_size = sizeof(open_options),
        .max_output_handles = 1,
        .platform = mipk_platform_hisilicon_context(audio->platform),
        .diagnostic = audio->diagnostic,
        .diagnostic_size = sizeof(audio->diagnostic),
    };
    status = mipk_pipeline_open(config->package_path, &open_options,
        &audio->pipeline);
    if (status != MIPK_OK) {
        fprintf(stderr, "cannot open %s: %s\n%s\n", config->package_path,
            mipk_status_string(status), audio->diagnostic);
        mipk_audio_close(audio);
        return MIPK_AUDIO_ERR_MODEL;
    }
    audio->info = mipk_pipeline_get_info(audio->pipeline);
    if (!pipeline_accepts_audio(audio->info)) {
        mipk_audio_close(audio);
        return MIPK_AUDIO_ERR_MODEL;
    }
    mipk_status result = mipk_pipeline_output_create(audio->pipeline,
        &audio->output);
    if (result != MIPK_OK) {
        mipk_audio_close(audio);
        return result == MIPK_ERR_MEMORY ? MIPK_AUDIO_ERR_MEMORY :
            MIPK_AUDIO_ERR_MODEL;
    }
    audio->model_frame = malloc(audio->info->input_size);
    if (audio->model_frame == NULL) {
        mipk_audio_close(audio);
        return MIPK_AUDIO_ERR_MEMORY;
    }
    *output = audio;
    return MIPK_AUDIO_OK;
}

void mipk_audio_close(mipk_audio *audio)
{
    if (audio == NULL) {
        return;
    }
    free(audio->model_frame);
    mipk_output_destroy(audio->output);
    (void)mipk_pipeline_close(audio->pipeline);
    if (audio->platform != NULL) {
        (void)mipk_platform_hisilicon_destroy(audio->platform);
    }
    free(audio);
}

const mipk_pipeline_info *mipk_audio_get_pipeline_info(const mipk_audio *audio)
{
    return audio == NULL ? NULL : audio->info;
}

size_t mipk_audio_runtime_option_count(const mipk_audio *audio)
{
    return audio == NULL ? 0 :
        mipk_pipeline_runtime_option_count(audio->pipeline);
}

mipk_status mipk_audio_runtime_option_at(const mipk_audio *audio, size_t index,
    mipk_runtime_option_info *info)
{
    return audio == NULL ? MIPK_ERR_ARGUMENT :
        mipk_pipeline_runtime_option_at(audio->pipeline, index, info);
}

mipk_status mipk_audio_runtime_option_value_at(const mipk_audio *audio,
    size_t index, char *buffer, size_t capacity, size_t *required_size)
{
    return audio == NULL ? MIPK_ERR_ARGUMENT :
        mipk_pipeline_runtime_option_value_at(audio->pipeline, index, buffer,
            capacity, required_size);
}

mipk_status mipk_audio_update_node_options(mipk_audio *audio,
    const char *node_id, const char *options_json)
{
    return audio == NULL ? MIPK_ERR_ARGUMENT :
        mipk_pipeline_update_node_options(audio->pipeline, node_id,
            options_json);
}

mipk_status mipk_audio_update_wake_word(mipk_audio *audio,
    const char *node_id, const mipk_wake_word_runtime_options *options)
{
    return audio == NULL ? MIPK_ERR_ARGUMENT :
        mipk_pipeline_update_wake_word(audio->pipeline, node_id, options);
}

mipk_status mipk_audio_apply_overrides_json(mipk_audio *audio,
    const char *overrides_json)
{
    return audio == NULL ? MIPK_ERR_ARGUMENT :
        mipk_pipeline_apply_overrides_json(audio->pipeline, overrides_json);
}

mipk_status mipk_audio_load_overrides(mipk_audio *audio, const char *path)
{
    return audio == NULL ? MIPK_ERR_ARGUMENT :
        mipk_pipeline_load_overrides(audio->pipeline, path);
}

mipk_audio_status mipk_audio_configure_input(const mipk_audio *audio,
    media_audio_input_config *input, uint32_t capture_frame_samples,
    uint32_t physical_channel, int gain_db, bool enable_aec)
{
    if (audio == NULL || input == NULL || physical_channel > 1U) {
        return MIPK_AUDIO_ERR_ARGUMENT;
    }
    if (capture_frame_samples == 0) {
        capture_frame_samples = audio->info->input_frame_samples;
    }
    if (capture_frame_samples > MIPK_AUDIO_MAX_CAPTURE_SAMPLES ||
        capture_frame_samples % audio->info->input_frame_samples != 0) {
        return MIPK_AUDIO_ERR_ARGUMENT;
    }
    media_audio_input_config_init(input);
    input->input = MEDIA_AUDIO_INPUT_IN1;
    input->physical_channel = physical_channel;
    input->gain_db = gain_db;
    input->sample_rate = audio->info->input_sample_rate;
    input->frame_samples = capture_frame_samples;
    input->enable_aec = enable_aec;
    input->use_talk_v2 = enable_aec;
    return MIPK_AUDIO_OK;
}

mipk_audio_status mipk_audio_configure_output(const mipk_audio *audio,
    media_audio_output_config *output, uint32_t output_frame_samples,
    int gain_db)
{
    if (audio == NULL || output == NULL) {
        return MIPK_AUDIO_ERR_ARGUMENT;
    }
    if (output_frame_samples == 0) {
        output_frame_samples = audio->info->input_frame_samples;
    }
    if (output_frame_samples > MIPK_AUDIO_MAX_CAPTURE_SAMPLES ||
        output_frame_samples % audio->info->input_frame_samples != 0) {
        return MIPK_AUDIO_ERR_ARGUMENT;
    }
    media_audio_output_config_init(output);
    output->gain_db = gain_db;
    output->sample_rate = audio->info->input_sample_rate;
    output->frame_samples = output_frame_samples;
    return MIPK_AUDIO_OK;
}

static mipk_audio_status process_model_frame(mipk_audio *audio,
    uint64_t pts, size_t capture_offset, mipk_audio_event_callback callback,
    void *user_data)
{
    const mipk_input input = {
        .type = MIPK_INPUT_AUDIO,
        .pts_us = pts,
        .value.audio = {
            .buffer = {
                .virtual_address = audio->model_frame,
                .physical_address = MIPK_PHYSICAL_ADDRESS_INVALID,
                .size = audio->info->input_size,
                .alignment = 1,
                .memory_type = MIPK_MEMORY_CPU,
                .owner = MIPK_BUFFER_OWNER_APPLICATION,
            },
            .initial_sync_state = MIPK_BUFFER_SYNCED,
            .sample_rate = audio->info->input_sample_rate,
            .channels = audio->info->input_channels,
            .sample_count = audio->info->input_frame_samples,
        },
    };
    const mipk_status status = mipk_pipeline_run_output(audio->pipeline,
        &input, audio->output);
    if (status == MIPK_AGAIN) {
        return MIPK_AUDIO_OK;
    }
    if (status != MIPK_OK && status != MIPK_ERR_CAPACITY) {
        return MIPK_AUDIO_ERR_MODEL;
    }
    if (callback != NULL) {
        const mipk_audio_event event = {
            .pipeline = audio->pipeline,
            .output = audio->output,
            .samples = audio->model_frame,
            .sample_count = audio->info->input_frame_samples,
            .capture_offset_samples = capture_offset,
            .pts_us = pts,
        };
        callback(&event, user_data);
    }
    return MIPK_AUDIO_OK;
}

mipk_audio_status mipk_audio_poll(mipk_audio *audio, media_context *media,
    int timeout_ms, mipk_audio_event_callback callback, void *user_data)
{
    if (audio == NULL || media == NULL || timeout_ms < 0) {
        return MIPK_AUDIO_ERR_ARGUMENT;
    }
    size_t sample_count = 0;
    uint64_t pts = 0;
    const media_status status = media_audio_input_read(media, audio->capture,
        MIPK_AUDIO_MAX_CAPTURE_SAMPLES, timeout_ms, &sample_count, &pts);
    if (status != MEDIA_OK) {
        return media_result(status);
    }
    const size_t frame_samples = audio->info->input_frame_samples;
    size_t source = 0;
    while (source < sample_count) {
        if (audio->model_frame_used == 0) {
            const uint64_t sample_offset_us =
                (uint64_t)source * UINT64_C(1000000) /
                    audio->info->input_sample_rate;
            if (pts > UINT64_MAX - sample_offset_us) {
                return MIPK_AUDIO_ERR_MEDIA;
            }
            audio->model_frame_capture_offset = source;
            audio->model_frame_pts = pts + sample_offset_us;
        }
        const size_t available = frame_samples - audio->model_frame_used;
        const size_t remaining = sample_count - source;
        const size_t count = remaining < available ? remaining : available;
        memcpy(audio->model_frame + audio->model_frame_used,
            audio->capture + source, count * sizeof(audio->capture[0]));
        audio->model_frame_used += count;
        source += count;
        if (audio->model_frame_used != frame_samples) {
            continue;
        }
        audio->model_frame_used = 0;
        const mipk_audio_status result = process_model_frame(audio,
            audio->model_frame_pts, audio->model_frame_capture_offset,
            callback, user_data);
        if (result != MIPK_AUDIO_OK) {
            return result;
        }
    }
    return MIPK_AUDIO_OK;
}

mipk_audio_status mipk_audio_play_pcm(media_context *media,
    const int16_t *samples, size_t sample_count, int timeout_ms)
{
    return media_result(media_audio_output_write(media, samples,
        sample_count, timeout_ms));
}

mipk_audio_status mipk_audio_play_memory(media_context *media,
    media_audio_format format, const void *data, size_t size)
{
    return media_result(media_audio_play_memory(media, format, data, size));
}

mipk_audio_status mipk_audio_play_file(media_context *media,
    media_audio_format format, const char *path)
{
    return media_result(media_audio_play_file(media, format, path));
}

mipk_audio_status mipk_audio_stop_playback(media_context *media)
{
    return media_result(media_audio_play_stop(media));
}

mipk_audio_status mipk_audio_prompt_load(media_context *media,
    media_audio_format format, const void *data, size_t size,
    mipk_audio_prompt **output)
{
    if (output == NULL) {
        return MIPK_AUDIO_ERR_ARGUMENT;
    }
    *output = NULL;
    mipk_audio_prompt *prompt = calloc(1, sizeof(*prompt));
    if (prompt == NULL) {
        return MIPK_AUDIO_ERR_MEMORY;
    }
    const media_status status = media_audio_prompt_load(media, format, data,
        size, &prompt->media_prompt);
    if (status != MEDIA_OK) {
        free(prompt);
        return media_result(status);
    }
    *output = prompt;
    return MIPK_AUDIO_OK;
}

mipk_audio_status mipk_audio_prompt_play(media_context *media,
    const mipk_audio_prompt *prompt, media_prompt_policy policy)
{
    if (prompt == NULL) {
        return MIPK_AUDIO_ERR_ARGUMENT;
    }
    return media_result(media_audio_prompt_play(media,
        prompt->media_prompt, policy));
}

void mipk_audio_prompt_close(mipk_audio_prompt *prompt)
{
    if (prompt == NULL) {
        return;
    }
    media_audio_prompt_destroy(prompt->media_prompt);
    free(prompt);
}

const char *mipk_audio_status_string(mipk_audio_status status)
{
    switch (status) {
        case MIPK_AUDIO_OK: return "success";
        case MIPK_AUDIO_AGAIN: return "no audio available";
        case MIPK_AUDIO_ERR_ARGUMENT: return "invalid argument";
        case MIPK_AUDIO_ERR_MEMORY: return "out of memory";
        case MIPK_AUDIO_ERR_MODEL: return "model error";
        case MIPK_AUDIO_ERR_MEDIA: return "media error";
        default: return "unknown error";
    }
}
