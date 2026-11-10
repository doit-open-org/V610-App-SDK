#include "media_internal.h"

#include "ot_common_aio.h"
#include "securec.h"
#include "ss_mpi_audio.h"
#include "ss_opusdec.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MEDIA_AUDIO_DECODE_CHUNK 640U
/* Bound control API preemption while allowing several retries under AO backpressure. */
#define MEDIA_AUDIO_SEND_TIMEOUT_MS 40U
#define MEDIA_AUDIO_SEND_RETRY_COUNT 4U
#define MEDIA_AUDIO_SEND_RETRY_NS 5000000L
#define MEDIA_AUDIO_DECODER_DRAIN_POLLS 50U
#define MEDIA_AUDIO_WORKER_WAIT_NS 5000000L
#define MEDIA_AUDIO_CAPTURE_TIMEOUT_MS 100
#define MEDIA_AUDIO_ENCODE_TIMEOUT_MS 100
#define MEDIA_OPUS_PRIVATE_HEADER_SIZE 8U
#define MEDIA_AUDIO_CAPTURE_FRAME_DEPTH 15U

static uint16_t media_read_le16(const uint8_t *data)
{
    return (uint16_t)data[0] | ((uint16_t)data[1] << 8);
}

static uint32_t media_read_le32(const uint8_t *data)
{
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8) |
        ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
}

static uint32_t media_read_be32(const uint8_t *data)
{
    return ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) |
        ((uint32_t)data[2] << 8) | (uint32_t)data[3];
}

static media_status media_validate_opus_packets(const uint8_t *data, size_t size)
{
    size_t offset = 0;
    while (offset < size) {
        if (size - offset < 4) {
            return MEDIA_ERR_FORMAT;
        }
        size_t packet_size = media_read_be32(data + offset);
        offset += 4;
        if (packet_size == 0 || packet_size > MEDIA_OPUS_MAX_PACKET_SIZE ||
            packet_size > size - offset) {
            return MEDIA_ERR_FORMAT;
        }
        offset += packet_size;
    }
    return offset == size ? MEDIA_OK : MEDIA_ERR_FORMAT;
}

static media_status media_extract_wav(const media_context *context, const uint8_t *data,
    size_t size, const uint8_t **pcm, size_t *pcm_size)
{
    if (size < 12 || memcmp(data, "RIFF", 4) != 0 || memcmp(data + 8, "WAVE", 4) != 0) {
        return MEDIA_ERR_FORMAT;
    }
    bool format_found = false;
    size_t offset = 12;
    while (offset <= size && size - offset >= 8) {
        const uint8_t *chunk = data + offset;
        uint32_t chunk_size_u32 = media_read_le32(chunk + 4);
        size_t chunk_size = chunk_size_u32;
        offset += 8;
        if (chunk_size > size - offset) {
            return MEDIA_ERR_FORMAT;
        }
        if (memcmp(chunk, "fmt ", 4) == 0) {
            if (chunk_size < 16 || media_read_le16(data + offset) != 1 ||
                media_read_le16(data + offset + 2) != 1 ||
                media_read_le32(data + offset + 4) != context->config.audio_output.sample_rate ||
                media_read_le16(data + offset + 14) != 16) {
                return MEDIA_ERR_UNSUPPORTED;
            }
            format_found = true;
        } else if (memcmp(chunk, "data", 4) == 0) {
            if (!format_found || chunk_size == 0 || (chunk_size & 1U) != 0) {
                return MEDIA_ERR_FORMAT;
            }
            *pcm = data + offset;
            *pcm_size = chunk_size;
            return MEDIA_OK;
        }
        size_t padding = chunk_size & 1U;
        if (chunk_size > SIZE_MAX - padding || chunk_size + padding > size - offset) {
            return MEDIA_ERR_FORMAT;
        }
        offset += chunk_size + padding;
    }
    return MEDIA_ERR_FORMAT;
}

static media_status media_normalize_audio(const media_context *context, media_audio_format format,
    const void *data, size_t size, media_audio_format *normalized_format,
    uint8_t **normalized_data, size_t *normalized_size)
{
    if (data == NULL || size == 0 || normalized_format == NULL ||
        normalized_data == NULL || normalized_size == NULL) {
        return MEDIA_ERR_ARGUMENT;
    }
    const uint8_t *source = data;
    size_t source_size = size;
    media_audio_format output_format = format;
    if (format == MEDIA_AUDIO_FORMAT_OGG_OPUS) {
        media_status status = media_ogg_opus_to_packets(data, size,
            normalized_data, normalized_size);
        if (status == MEDIA_OK) {
            *normalized_format = MEDIA_AUDIO_FORMAT_OPUS;
        }
        return status;
    }
    if (format == MEDIA_AUDIO_FORMAT_WAV) {
        media_status status = media_extract_wav(context, source, source_size, &source, &source_size);
        if (status != MEDIA_OK) {
            return status;
        }
        output_format = MEDIA_AUDIO_FORMAT_PCM_S16LE;
    } else if (format == MEDIA_AUDIO_FORMAT_PCM_S16LE && (size & 1U) != 0) {
        return MEDIA_ERR_FORMAT;
    } else if (format == MEDIA_AUDIO_FORMAT_OPUS) {
        media_status status = media_validate_opus_packets(source, source_size);
        if (status != MEDIA_OK) {
            return status;
        }
    } else if (format < MEDIA_AUDIO_FORMAT_PCM_S16LE ||
        format > MEDIA_AUDIO_FORMAT_OGG_OPUS) {
        return MEDIA_ERR_UNSUPPORTED;
    }
    uint8_t *copy = malloc(source_size);
    if (copy == NULL) {
        return MEDIA_ERR_MEMORY;
    }
    if (memcpy_s(copy, source_size, source, source_size) != EOK) {
        free(copy);
        return MEDIA_ERR_MEMORY;
    }
    *normalized_format = output_format;
    *normalized_data = copy;
    *normalized_size = source_size;
    return MEDIA_OK;
}

static hi_mapi_audio_format media_decoder_format(media_audio_format format)
{
    return format == MEDIA_AUDIO_FORMAT_MP3 ?
        HI_MAPI_AUDIO_FORMAT_MP3 : HI_MAPI_AUDIO_FORMAT_AACLC;
}

static media_audio_format media_codec_decoder_format(media_audio_codec codec)
{
    switch (codec) {
        case MEDIA_AUDIO_CODEC_AAC: return MEDIA_AUDIO_FORMAT_AAC_ADTS;
        case MEDIA_AUDIO_CODEC_MP3: return MEDIA_AUDIO_FORMAT_MP3;
        case MEDIA_AUDIO_CODEC_OPUS: return MEDIA_AUDIO_FORMAT_OPUS;
        default: return MEDIA_AUDIO_FORMAT_OGG_OPUS;
    }
}

static void media_audio_source_reset(media_audio_source *source)
{
    if (source->opus_decoder != NULL) {
        ss_opusdec_destroy(source->opus_decoder);
    }
    if (source->decoder_initialized) {
        (void)hi_mapi_adec_deinit(source->decoder);
    }
    if (source->kind == MEDIA_SOURCE_MEMORY) {
        free(source->data);
    }
    *source = (media_audio_source) {0};
}

static media_status media_audio_source_set_memory(media_audio_source *source,
    td_handle decoder, media_audio_format format, uint8_t *data, size_t size)
{
    if (source == NULL || data == NULL || size == 0) {
        return MEDIA_ERR_ARGUMENT;
    }
    media_audio_source_reset(source);
    *source = (media_audio_source) {
        .kind = MEDIA_SOURCE_MEMORY,
        .format = format,
        .data = data,
        .size = size,
        .decoder = decoder,
    };
    return MEDIA_OK;
}

static media_status media_audio_decoder_init(media_audio_source *source)
{
    hi_mapi_adec_attr attributes = {0};
    if (hi_mapi_adec_get_default_attr(media_decoder_format(source->format), &attributes) != TD_SUCCESS ||
        hi_mapi_adec_init(source->decoder, &attributes) != TD_SUCCESS) {
        return MEDIA_ERR_DEVICE;
    }
    source->decoder_initialized = true;
    return MEDIA_OK;
}

static bool media_decoder_copy_locked(const media_audio_decoder *decoder,
    size_t offset, uint8_t *buffer, size_t count)
{
    if (offset > decoder->count || count > decoder->count - offset || count > decoder->capacity) {
        return false;
    }
    size_t read_index = decoder->read_index + offset;
    if (read_index >= decoder->capacity) {
        read_index -= decoder->capacity;
    }
    size_t first = count;
    if (first > decoder->capacity - read_index) {
        first = decoder->capacity - read_index;
    }
    if (first > 0 && memcpy_s(buffer, count, decoder->buffer + read_index, first) != EOK) {
        return false;
    }
    size_t second = count - first;
    if (second > 0 && memcpy_s(buffer + first, count - first,
        decoder->buffer, second) != EOK) {
        return false;
    }
    return true;
}

static size_t media_decoder_peek_locked(const media_audio_decoder *decoder,
    uint8_t *buffer, size_t capacity)
{
    size_t count = decoder->count < capacity ? decoder->count : capacity;
    return media_decoder_copy_locked(decoder, 0, buffer, count) ? count : 0;
}

static void media_decoder_consume_locked(media_player_state *player,
    media_audio_decoder *decoder, size_t count)
{
    if (count > decoder->count) {
        return;
    }
    size_t tail = decoder->capacity - decoder->read_index;
    decoder->read_index = count >= tail ? count - tail : decoder->read_index + count;
    decoder->count -= count;
    (void)pthread_cond_broadcast(&player->space);
}

static size_t media_player_input_peek_locked(const media_player_input *input,
    uint8_t *buffer, size_t capacity)
{
    size_t count = input->count < capacity ? input->count : capacity;
    size_t first = count;
    if (first > input->capacity - input->read_index) {
        first = input->capacity - input->read_index;
    }
    if (first > 0 && memcpy_s(buffer, capacity,
        input->buffer + input->read_index, first) != EOK) {
        return 0;
    }
    size_t second = count - first;
    if (second > 0 && memcpy_s(buffer + first, capacity - first,
        input->buffer, second) != EOK) {
        return 0;
    }
    return count;
}

static void media_player_input_consume_locked(media_player_state *player, size_t count)
{
    media_player_input *input = &player->input;
    if (count > input->count) {
        return;
    }
    size_t tail = input->capacity - input->read_index;
    input->read_index = count >= tail ? count - tail : input->read_index + count;
    input->count -= count;
    (void)pthread_cond_broadcast(&player->space);
}

static void media_player_input_reset_locked(media_player_state *player)
{
    player->input.read_index = 0;
    player->input.write_index = 0;
    player->input.count = 0;
    player->input.finished = false;
    (void)pthread_cond_broadcast(&player->space);
}

static td_s32 media_ao_send_frame(const hi_mapi_audio_frame *frame)
{
    const struct timespec retry = {
        .tv_sec = 0,
        .tv_nsec = MEDIA_AUDIO_SEND_RETRY_NS,
    };
    td_s32 result = TD_FAILURE;
    for (unsigned int attempt = 0; attempt < MEDIA_AUDIO_SEND_RETRY_COUNT; attempt++) {
        result = hi_mapi_ao_send_frame(MEDIA_AUDIO_DEVICE, MEDIA_AUDIO_CHANNEL,
            frame, MEDIA_AUDIO_SEND_TIMEOUT_MS);
        if (result != HI_MAPI_AO_ERR_BUF_FULL) {
            break;
        }
        (void)nanosleep(&retry, NULL);
    }
    return result;
}

static media_status media_send_pcm(media_context *context, const uint8_t *data, size_t size)
{
    media_player_state *player = &context->player;
    size_t frame_bytes = (size_t)context->config.audio_output.frame_samples * sizeof(int16_t);
    if (frame_bytes == 0 || frame_bytes > sizeof(player->pcm_staging) || size == 0 || size > frame_bytes) {
        return MEDIA_ERR_FORMAT;
    }
    if (memset_s(player->pcm_staging, sizeof(player->pcm_staging), 0, frame_bytes) != EOK ||
        memcpy_s(player->pcm_staging, frame_bytes, data, size) != EOK) {
        return MEDIA_ERR_MEMORY;
    }
    hi_mapi_audio_frame frame = {0};
    frame.bit_width = OT_AUDIO_BIT_WIDTH_16;
    frame.snd_mode = OT_AUDIO_SOUND_MODE_MONO;
    frame.virt_addr[0] = player->pcm_staging;
    frame.len = (td_u32)frame_bytes;
    return media_ao_send_frame(&frame) == TD_SUCCESS ? MEDIA_OK : MEDIA_ERR_DEVICE;
}

static media_status media_source_queue_pcm(media_context *context,
    media_audio_source *source, const uint8_t *data, size_t size)
{
    size_t frame_bytes = (size_t)context->config.audio_output.frame_samples * sizeof(int16_t);
    if (source == NULL || data == NULL || size == 0 || (size & 1U) != 0 ||
        frame_bytes == 0 || frame_bytes > sizeof(source->pcm_pending) ||
        source->pcm_pending_size >= frame_bytes) {
        return MEDIA_ERR_FORMAT;
    }

    size_t offset = 0;
    if (source->pcm_pending_size > 0) {
        size_t copy_size = frame_bytes - source->pcm_pending_size;
        if (copy_size > size) {
            copy_size = size;
        }
        if (memcpy_s(source->pcm_pending + source->pcm_pending_size,
                sizeof(source->pcm_pending) - source->pcm_pending_size,
                data, copy_size) != EOK) {
            return MEDIA_ERR_MEMORY;
        }
        source->pcm_pending_size += copy_size;
        offset += copy_size;
        if (source->pcm_pending_size == frame_bytes) {
            media_status status = media_send_pcm(context,
                source->pcm_pending, frame_bytes);
            if (status != MEDIA_OK) {
                return status;
            }
            source->pcm_pending_size = 0;
        }
    }

    while (offset < size) {
        size_t remaining = size - offset;
        if (remaining < frame_bytes) {
            if (memcpy_s(source->pcm_pending, sizeof(source->pcm_pending),
                    data + offset, remaining) != EOK) {
                return MEDIA_ERR_MEMORY;
            }
            source->pcm_pending_size = remaining;
            break;
        }
        media_status status = media_send_pcm(context, data + offset, frame_bytes);
        if (status != MEDIA_OK) {
            return status;
        }
        offset += frame_bytes;
    }
    return MEDIA_OK;
}

static media_status media_ao_drain_status(void)
{
    ot_ao_chn_state state = {0};
    if (ss_mpi_ao_query_chn_status(MEDIA_AUDIO_DEVICE,
        MEDIA_AUDIO_CHANNEL, &state) != TD_SUCCESS) {
        return MEDIA_ERR_DEVICE;
    }
    return state.chn_busy_num == 0 ? MEDIA_EOF : MEDIA_AGAIN;
}

static media_status media_source_finish_pcm(media_context *context,
    media_audio_source *source)
{
    if (source->pcm_pending_size > 0) {
        media_status status = media_send_pcm(context,
            source->pcm_pending, source->pcm_pending_size);
        if (status != MEDIA_OK) {
            return status;
        }
        source->pcm_pending_size = 0;
        return MEDIA_OK;
    }
    return media_ao_drain_status();
}

static media_status media_source_pcm_step_locked(media_context *context, media_audio_source *source)
{
    media_player_state *player = &context->player;
    size_t frame_bytes = (size_t)context->config.audio_output.frame_samples * sizeof(int16_t);
    size_t count = 0;
    if (source->kind == MEDIA_SOURCE_MEMORY) {
        if (source->offset >= source->size) {
            return media_ao_drain_status();
        }
        count = source->size - source->offset;
        if (count > frame_bytes) {
            count = frame_bytes;
        }
        media_status status = media_send_pcm(context, source->data + source->offset, count);
        if (status == MEDIA_OK) {
            source->offset += count;
        }
        return status;
    }
    if (source->kind != MEDIA_SOURCE_PLAYER) {
        return MEDIA_EOF;
    }
    media_player_input *input = &player->input;
    if (input->count == 0) {
        return input->finished ? media_ao_drain_status() : MEDIA_AGAIN;
    }
    uint8_t samples[MEDIA_AUDIO_MAX_FRAME_SAMPLES * sizeof(int16_t)];
    count = media_player_input_peek_locked(input, samples, frame_bytes);
    if (count == 0) {
        return MEDIA_ERR_MEMORY;
    }
    media_status status = media_send_pcm(context, samples, count);
    if (status == MEDIA_OK) {
        media_player_input_consume_locked(player, count);
    }
    return status;
}

static media_status media_source_get_compressed_locked(media_context *context,
    media_audio_source *source, uint8_t *buffer, size_t capacity,
    size_t *count, size_t *source_span)
{
    media_player_state *player = &context->player;
    *count = 0;
    *source_span = 0;
    if (source->kind == MEDIA_SOURCE_MEMORY) {
        if (source->offset >= source->size) {
            return MEDIA_EOF;
        }
        size_t available = source->size - source->offset;
        if (source->format == MEDIA_AUDIO_FORMAT_OPUS) {
            if (available < 4) {
                return MEDIA_ERR_FORMAT;
            }
            size_t packet_size = media_read_be32(source->data + source->offset);
            if (packet_size == 0 || packet_size > capacity || packet_size > available - 4) {
                return MEDIA_ERR_FORMAT;
            }
            if (memcpy_s(buffer, capacity, source->data + source->offset + 4,
                packet_size) != EOK) {
                return MEDIA_ERR_MEMORY;
            }
            *count = packet_size;
            *source_span = packet_size + 4;
            return MEDIA_OK;
        }
        *count = available < capacity ? available : capacity;
        if (memcpy_s(buffer, capacity, source->data + source->offset, *count) != EOK) {
            return MEDIA_ERR_MEMORY;
        }
        *source_span = *count;
        return MEDIA_OK;
    }
    media_audio_decoder *decoder = player->decoder;
    if (decoder == NULL || decoder->generation != player->decoder_generation || decoder->closed) {
        return MEDIA_EOF;
    }
    if (decoder->count == 0) {
        return decoder->finished ? MEDIA_EOF : MEDIA_AGAIN;
    }
    if (source->format == MEDIA_AUDIO_FORMAT_OPUS) {
        uint8_t header[4];
        if (decoder->count < sizeof(header)) {
            return decoder->finished ? MEDIA_ERR_FORMAT : MEDIA_AGAIN;
        }
        if (!media_decoder_copy_locked(decoder, 0, header, sizeof(header))) {
            return MEDIA_ERR_MEMORY;
        }
        size_t packet_size = media_read_be32(header);
        if (packet_size == 0 || packet_size > capacity) {
            return MEDIA_ERR_FORMAT;
        }
        size_t framed_size = sizeof(header) + packet_size;
        if (decoder->count < framed_size) {
            return decoder->finished ? MEDIA_ERR_FORMAT : MEDIA_AGAIN;
        }
        if (!media_decoder_copy_locked(decoder, sizeof(header), buffer, packet_size)) {
            return MEDIA_ERR_MEMORY;
        }
        *count = packet_size;
        *source_span = framed_size;
        return MEDIA_OK;
    }
    *count = media_decoder_peek_locked(decoder, buffer, capacity);
    if (*count == 0) {
        return MEDIA_ERR_MEMORY;
    }
    *source_span = *count;
    return MEDIA_OK;
}

static void media_source_consume_compressed_locked(media_context *context,
    media_audio_source *source, size_t count)
{
    if (source->kind == MEDIA_SOURCE_MEMORY) {
        source->offset += count;
        return;
    }
    media_audio_decoder *decoder = context->player.decoder;
    if (decoder != NULL && decoder->generation == context->player.decoder_generation &&
        !decoder->closed) {
        media_decoder_consume_locked(&context->player, decoder, count);
    }
}

static media_status media_source_decode_step_locked(media_context *context, media_audio_source *source)
{
    if (!source->decoder_initialized) {
        media_status status = media_audio_decoder_init(source);
        if (status != MEDIA_OK) {
            return status;
        }
    }
    if (!source->eos_sent) {
        uint8_t compressed[MEDIA_AUDIO_DECODE_CHUNK];
        size_t count = 0;
        size_t source_span = 0;
        media_status input_status = media_source_get_compressed_locked(context,
            source, compressed, sizeof(compressed), &count, &source_span);
        if (input_status == MEDIA_OK) {
            hi_mapi_audio_stream stream = {
                .stream = compressed,
                .len = (td_u32)count,
            };
            td_s32 result = hi_mapi_adec_send_stream(source->decoder, &stream, TD_FALSE);
            if (result != HI_MAPI_ADEC_ERR_BUF_FULL) {
                if (result != TD_SUCCESS) {
                    return MEDIA_ERR_DEVICE;
                }
                media_source_consume_compressed_locked(context, source, source_span);
                return MEDIA_OK;
            }
        } else if (input_status != MEDIA_AGAIN && input_status != MEDIA_EOF) {
            return input_status;
        } else if (input_status == MEDIA_EOF) {
            if (hi_mapi_adec_send_end_of_stream(source->decoder) != TD_SUCCESS) {
                return MEDIA_ERR_DEVICE;
            }
            source->eos_sent = true;
            return MEDIA_OK;
        }
    }

    bool drain_probe = false;
    if (source->eos_sent && source->idle_count > 0) {
        source->idle_count++;
        if (source->idle_count < MEDIA_AUDIO_DECODER_DRAIN_POLLS) {
            return MEDIA_AGAIN;
        }
        source->idle_count = 0;
        drain_probe = true;
    }

    hi_mapi_audio_frame_info frame_info = {0};
    td_s32 result = hi_mapi_adec_get_frame(source->decoder, &frame_info, TD_FALSE);
    if (result == TD_SUCCESS) {
        media_status status = media_source_queue_pcm(context, source,
            frame_info.audio_frame.virt_addr[0], frame_info.audio_frame.len);
        if (hi_mapi_adec_release_frame(source->decoder, &frame_info) != TD_SUCCESS) {
            return MEDIA_ERR_DEVICE;
        }
        source->idle_count = 0;
        return status;
    }
    if (result != HI_MAPI_ADEC_ERR_BUF_EMPTY) {
        return MEDIA_ERR_DEVICE;
    }
    if (drain_probe) {
        return media_source_finish_pcm(context, source);
    }
    if (source->eos_sent) {
        source->idle_count = 1;
    }
    return MEDIA_AGAIN;
}

static media_status media_source_opus_step_locked(media_context *context,
    media_audio_source *source)
{
    if (source->opus_decoder == NULL) {
        const ot_opusdec_config config = {
            .sample_rate = (td_s32)context->config.audio_output.sample_rate,
            .channels = 1,
        };
        source->opus_decoder = ss_opusdec_create(&config);
        if (source->opus_decoder == NULL) {
            return MEDIA_ERR_DEVICE;
        }
    }

    uint8_t framed[8U + MEDIA_OPUS_MAX_PACKET_SIZE] = {0};
    size_t packet_size = 0;
    size_t source_span = 0;
    media_status status = media_source_get_compressed_locked(context, source,
        framed + 8U, MEDIA_OPUS_MAX_PACKET_SIZE, &packet_size, &source_span);
    if (status == MEDIA_EOF) {
        return media_source_finish_pcm(context, source);
    }
    if (status != MEDIA_OK) {
        return status;
    }
    framed[0] = (uint8_t)(packet_size >> 24);
    framed[1] = (uint8_t)(packet_size >> 16);
    framed[2] = (uint8_t)(packet_size >> 8);
    framed[3] = (uint8_t)packet_size;

    uint8_t *input = framed;
    td_s32 input_size = (td_s32)(packet_size + 8U);
    int16_t pcm[OT_OPUSDEC_MAX_OUT_SAMPLES];
    td_u32 pcm_size = 0;
    if (ss_opusdec_process_frame(source->opus_decoder, &input, &input_size,
        pcm, &pcm_size) != TD_SUCCESS || pcm_size == 0 ||
        pcm_size > sizeof(pcm) || (pcm_size & 1U) != 0) {
        return MEDIA_ERR_FORMAT;
    }

    media_source_consume_compressed_locked(context, source, source_span);
    return media_source_queue_pcm(context, source, (const uint8_t *)pcm, pcm_size);
}

static media_status media_source_step_locked(media_context *context, media_audio_source *source)
{
    if (source->kind == MEDIA_SOURCE_NONE) {
        return MEDIA_EOF;
    }
    if (source->format == MEDIA_AUDIO_FORMAT_PCM_S16LE) {
        return media_source_pcm_step_locked(context, source);
    }
    if (source->format == MEDIA_AUDIO_FORMAT_OPUS) {
        return media_source_opus_step_locked(context, source);
    }
    return media_source_decode_step_locked(context, source);
}

static void media_prompt_entry_clear(media_prompt_entry *entry)
{
    free(entry->data);
    *entry = (media_prompt_entry) {0};
}

static bool media_prompt_pop_locked(media_player_state *player)
{
    if (player->prompt_count == 0) {
        return false;
    }
    media_prompt_entry *entry = &player->prompt_queue[player->prompt_read_index];
    (void)media_audio_source_set_memory(&player->prompt_source,
        MEDIA_AUDIO_PROMPT_DECODER, entry->format, entry->data, entry->size);
    entry->data = NULL;
    entry->size = 0;
    player->prompt_read_index = (player->prompt_read_index + 1) % MEDIA_PROMPT_QUEUE_DEPTH;
    player->prompt_count--;
    player->state = MEDIA_AUDIO_PLAY_PAUSED_BY_PROMPT;
    return true;
}

static bool media_source_waits_for_input_locked(const media_context *context,
    const media_audio_source *source)
{
    const media_player_state *player = &context->player;
    if (source->kind == MEDIA_SOURCE_PLAYER &&
        source->format == MEDIA_AUDIO_FORMAT_PCM_S16LE) {
        return player->input.count == 0 && !player->input.finished;
    }
    if (source->kind != MEDIA_SOURCE_DECODER ||
        source->format != MEDIA_AUDIO_FORMAT_OPUS) {
        return false;
    }
    const media_audio_decoder *decoder = player->decoder;
    return decoder != NULL && decoder->generation == player->decoder_generation &&
        !decoder->closed && decoder->count == 0 && !decoder->finished;
}

static void *media_player_worker(void *argument)
{
    media_context *context = argument;
    media_player_state *player = &context->player;
    struct timespec retry = {
        .tv_sec = 0,
        .tv_nsec = MEDIA_AUDIO_WORKER_WAIT_NS,
    };
    (void)pthread_mutex_lock(&player->lock);
    while (player->worker_running) {
        if (player->state == MEDIA_AUDIO_PLAY_ERROR) {
            (void)pthread_cond_wait(&player->changed, &player->lock);
            continue;
        }
        if (player->prompt_source.kind == MEDIA_SOURCE_NONE) {
            (void)media_prompt_pop_locked(player);
        }
        media_audio_source *active = player->prompt_source.kind != MEDIA_SOURCE_NONE ?
            &player->prompt_source : &player->main_source;
        if (active->kind == MEDIA_SOURCE_NONE) {
            player->state = MEDIA_AUDIO_PLAY_IDLE;
            (void)pthread_cond_wait(&player->changed, &player->lock);
            continue;
        }
        if (active == &player->main_source) {
            player->state = MEDIA_AUDIO_PLAY_PLAYING;
        }
        media_status status = media_source_step_locked(context, active);
        if (status == MEDIA_EOF) {
            if (active == &player->main_source && active->kind == MEDIA_SOURCE_PLAYER) {
                media_player_input_reset_locked(player);
            }
            media_audio_source_reset(active);
            if (active == &player->prompt_source && player->prompt_count == 0 &&
                player->main_source.kind != MEDIA_SOURCE_NONE) {
                player->state = MEDIA_AUDIO_PLAY_PLAYING;
            }
            continue;
        }
        if (status < 0) {
            if (active == &player->main_source && active->kind == MEDIA_SOURCE_PLAYER) {
                media_player_input_reset_locked(player);
            }
            media_audio_source_reset(active);
            player->state = MEDIA_AUDIO_PLAY_ERROR;
            continue;
        }
        if (status == MEDIA_AGAIN) {
            if (media_source_waits_for_input_locked(context, active)) {
                (void)pthread_cond_wait(&player->changed, &player->lock);
            } else {
                (void)pthread_mutex_unlock(&player->lock);
                (void)nanosleep(&retry, NULL);
                (void)pthread_mutex_lock(&player->lock);
            }
        } else {
            /* Let control APIs preempt a continuously-fed output stream. */
            (void)pthread_mutex_unlock(&player->lock);
            (void)pthread_mutex_lock(&player->lock);
        }
    }
    media_audio_source_reset(&player->prompt_source);
    media_audio_source_reset(&player->main_source);
    (void)pthread_mutex_unlock(&player->lock);
    return NULL;
}

media_status media_player_start(media_context *context)
{
    if (!context->config.audio_output.enabled) {
        return MEDIA_OK;
    }
    media_player_state *player = &context->player;
    if (pthread_mutex_init(&player->lock, NULL) != 0) {
        return MEDIA_ERR_DEVICE;
    }
    if (pthread_cond_init(&player->changed, NULL) != 0) {
        (void)pthread_mutex_destroy(&player->lock);
        return MEDIA_ERR_DEVICE;
    }
    if (pthread_cond_init(&player->space, NULL) != 0) {
        (void)pthread_cond_destroy(&player->changed);
        (void)pthread_mutex_destroy(&player->lock);
        return MEDIA_ERR_DEVICE;
    }
    player->input.capacity = context->config.audio_output.max_buffer_size;
    player->input.buffer = malloc(player->input.capacity);
    if (player->input.buffer == NULL) {
        (void)pthread_cond_destroy(&player->space);
        (void)pthread_cond_destroy(&player->changed);
        (void)pthread_mutex_destroy(&player->lock);
        return MEDIA_ERR_MEMORY;
    }
    player->prompt_read_index = 0;
    player->prompt_write_index = 0;
    player->prompt_count = 0;
    player->state = MEDIA_AUDIO_PLAY_IDLE;
    player->initialized = true;
    hi_mapi_ao_attr attributes = {0};
    if (hi_mapi_ao_get_default_attr((hi_mapi_audio_sample_rate)context->config.audio_output.sample_rate,
        HI_MAPI_AUDIO_SOUND_MODE_MONO, &attributes) != TD_SUCCESS) {
        media_player_stop(context);
        return MEDIA_ERR_DEVICE;
    }
    attributes.in_sample_rate = (hi_mapi_audio_sample_rate)context->config.audio_output.sample_rate;
    /* TalkV2 requires the AO reference cadence to match the AI frame cadence. */
    attributes.pt_num_per_frm = context->config.audio_output.frame_samples;
    if (hi_mapi_ao_init(MEDIA_AUDIO_DEVICE, &attributes) != TD_SUCCESS) {
        media_player_stop(context);
        return MEDIA_ERR_DEVICE;
    }
    player->ao_initialized = true;
    if (hi_mapi_ao_start(MEDIA_AUDIO_DEVICE, MEDIA_AUDIO_CHANNEL) != TD_SUCCESS) {
        media_player_stop(context);
        return MEDIA_ERR_DEVICE;
    }
    player->ao_started = true;
    if (hi_mapi_ao_set_volume(MEDIA_AUDIO_DEVICE,
        context->config.audio_output.gain_db) != TD_SUCCESS) {
        media_player_stop(context);
        return MEDIA_ERR_DEVICE;
    }
    player->worker_running = true;
    if (pthread_create(&player->worker, NULL, media_player_worker, context) != 0) {
        player->worker_running = false;
        media_player_stop(context);
        return MEDIA_ERR_DEVICE;
    }
    player->worker_started = true;
    return MEDIA_OK;
}

void media_player_stop(media_context *context)
{
    media_player_state *player = &context->player;
    if (!player->initialized) {
        return;
    }
    (void)pthread_mutex_lock(&player->lock);
    player->worker_running = false;
    (void)pthread_cond_broadcast(&player->changed);
    (void)pthread_cond_broadcast(&player->space);
    (void)pthread_mutex_unlock(&player->lock);
    if (player->worker_started) {
        (void)pthread_join(player->worker, NULL);
        player->worker_started = false;
    }
    for (size_t i = 0; i < MEDIA_PROMPT_QUEUE_DEPTH; i++) {
        media_prompt_entry_clear(&player->prompt_queue[i]);
    }
    player->prompt_read_index = 0;
    player->prompt_write_index = 0;
    player->prompt_count = 0;
    player->state = MEDIA_AUDIO_PLAY_IDLE;
    if (player->decoder != NULL) {
        player->decoder->closed = true;
        player->decoder->context = NULL;
        player->decoder = NULL;
    }
    free(player->input.buffer);
    player->input = (media_player_input) {0};
    if (player->ao_started) {
        (void)hi_mapi_ao_stop(MEDIA_AUDIO_DEVICE, MEDIA_AUDIO_CHANNEL);
        player->ao_started = false;
    }
    if (player->ao_initialized) {
        (void)hi_mapi_ao_deinit(MEDIA_AUDIO_DEVICE);
        player->ao_initialized = false;
    }
    (void)pthread_cond_destroy(&player->space);
    (void)pthread_cond_destroy(&player->changed);
    (void)pthread_mutex_destroy(&player->lock);
    player->initialized = false;
}

media_status media_audio_play_memory(media_context *context, media_audio_format format,
    const void *data, size_t size)
{
    if (context == NULL || !context->player.initialized) {
        return MEDIA_ERR_STATE;
    }
    media_audio_format normalized_format;
    uint8_t *copy = NULL;
    size_t copy_size = 0;
    media_status status = media_normalize_audio(context, format, data, size,
        &normalized_format, &copy, &copy_size);
    if (status != MEDIA_OK) {
        return status;
    }
    media_player_state *player = &context->player;
    (void)pthread_mutex_lock(&player->lock);
    if (player->decoder != NULL) {
        player->decoder->closed = true;
        player->decoder->context = NULL;
        player->decoder = NULL;
    }
    media_player_input_reset_locked(player);
    status = media_audio_source_set_memory(&player->main_source,
        MEDIA_AUDIO_MAIN_DECODER, normalized_format, copy, copy_size);
    if (status == MEDIA_OK) {
        player->state = player->prompt_source.kind == MEDIA_SOURCE_NONE ?
            MEDIA_AUDIO_PLAY_PLAYING : MEDIA_AUDIO_PLAY_PAUSED_BY_PROMPT;
        (void)pthread_cond_broadcast(&player->changed);
    } else {
        free(copy);
    }
    (void)pthread_mutex_unlock(&player->lock);
    return status;
}

media_status media_audio_play_file(media_context *context, media_audio_format format, const char *path)
{
    uint8_t *data = NULL;
    size_t size = 0;
    media_status status = media_copy_file(path, &data, &size);
    if (status != MEDIA_OK) {
        return status;
    }
    status = media_audio_play_memory(context, format, data, size);
    free(data);
    return status;
}

media_status media_audio_play_stop(media_context *context)
{
    if (context == NULL || !context->player.initialized) {
        return MEDIA_ERR_STATE;
    }
    media_player_state *player = &context->player;
    (void)pthread_mutex_lock(&player->lock);
    media_audio_source_reset(&player->main_source);
    media_player_input_reset_locked(player);
    if (player->decoder != NULL) {
        player->decoder->closed = true;
        player->decoder->context = NULL;
        player->decoder = NULL;
    }
    player->state = player->prompt_source.kind == MEDIA_SOURCE_NONE ?
        MEDIA_AUDIO_PLAY_IDLE : MEDIA_AUDIO_PLAY_PAUSED_BY_PROMPT;
    (void)pthread_cond_broadcast(&player->changed);
    (void)pthread_cond_broadcast(&player->space);
    (void)pthread_mutex_unlock(&player->lock);
    return MEDIA_OK;
}

media_audio_play_state media_audio_play_get_state(media_context *context)
{
    if (context == NULL || !context->player.initialized) {
        return MEDIA_AUDIO_PLAY_ERROR;
    }
    media_player_state *player = &context->player;
    (void)pthread_mutex_lock(&player->lock);
    media_audio_play_state state = player->state;
    (void)pthread_mutex_unlock(&player->lock);
    return state;
}

media_status media_audio_output_write(media_context *context, const int16_t *samples,
    size_t sample_count, int timeout_ms)
{
    if (context == NULL || samples == NULL || sample_count == 0 ||
        sample_count > SIZE_MAX / sizeof(*samples) || timeout_ms < 0 ||
        !context->player.initialized) {
        return MEDIA_ERR_ARGUMENT;
    }
    size_t size = sample_count * sizeof(*samples);
    media_player_state *player = &context->player;
    if (size > player->input.capacity) {
        return MEDIA_ERR_ARGUMENT;
    }
    struct timespec deadline;
    if (!media_make_deadline(timeout_ms, &deadline)) {
        return MEDIA_ERR_ARGUMENT;
    }

    (void)pthread_mutex_lock(&player->lock);
    if (player->main_source.kind == MEDIA_SOURCE_NONE) {
        media_player_input_reset_locked(player);
        player->main_source = (media_audio_source) {
            .kind = MEDIA_SOURCE_PLAYER,
            .format = MEDIA_AUDIO_FORMAT_PCM_S16LE,
        };
    } else if (player->main_source.kind != MEDIA_SOURCE_PLAYER) {
        (void)pthread_mutex_unlock(&player->lock);
        return MEDIA_ERR_BUSY;
    }
    media_player_input *input = &player->input;
    while (!input->finished && input->capacity - input->count < size) {
        int result = pthread_cond_timedwait(&player->space, &player->lock, &deadline);
        if (result == ETIMEDOUT) {
            (void)pthread_mutex_unlock(&player->lock);
            return MEDIA_ERR_TIMEOUT;
        }
        if (result != 0) {
            (void)pthread_mutex_unlock(&player->lock);
            return MEDIA_ERR_DEVICE;
        }
        if (player->main_source.kind != MEDIA_SOURCE_PLAYER) {
            (void)pthread_mutex_unlock(&player->lock);
            return MEDIA_ERR_STATE;
        }
    }
    if (input->finished) {
        (void)pthread_mutex_unlock(&player->lock);
        return MEDIA_ERR_STATE;
    }
    size_t first = size;
    if (first > input->capacity - input->write_index) {
        first = input->capacity - input->write_index;
    }
    if (memcpy_s(input->buffer + input->write_index,
        input->capacity - input->write_index, samples, first) != EOK) {
        (void)pthread_mutex_unlock(&player->lock);
        return MEDIA_ERR_MEMORY;
    }
    size_t second = size - first;
    if (second > 0 && memcpy_s(input->buffer, input->capacity,
        (const uint8_t *)samples + first, second) != EOK) {
        (void)pthread_mutex_unlock(&player->lock);
        return MEDIA_ERR_MEMORY;
    }
    size_t tail = input->capacity - input->write_index;
    input->write_index = size >= tail ? size - tail : input->write_index + size;
    input->count += size;
    player->state = player->prompt_source.kind == MEDIA_SOURCE_NONE ?
        MEDIA_AUDIO_PLAY_PLAYING : MEDIA_AUDIO_PLAY_PAUSED_BY_PROMPT;
    (void)pthread_cond_broadcast(&player->changed);
    (void)pthread_mutex_unlock(&player->lock);
    return MEDIA_OK;
}

media_status media_audio_output_finish(media_context *context)
{
    if (context == NULL || !context->player.initialized) {
        return MEDIA_ERR_STATE;
    }
    media_player_state *player = &context->player;
    (void)pthread_mutex_lock(&player->lock);
    if (player->main_source.kind != MEDIA_SOURCE_PLAYER) {
        (void)pthread_mutex_unlock(&player->lock);
        return MEDIA_ERR_STATE;
    }
    player->input.finished = true;
    (void)pthread_cond_broadcast(&player->changed);
    (void)pthread_mutex_unlock(&player->lock);
    return MEDIA_OK;
}

media_status media_audio_decoder_open(media_context *context,
    const media_audio_decoder_config *config, media_audio_decoder **output)
{
    if (context == NULL || config == NULL || output == NULL || !context->player.initialized ||
        config->sample_rate != context->config.audio_output.sample_rate || config->channels != 1 ||
        config->buffer_size < 4096 ||
        config->buffer_size > context->config.audio_output.max_buffer_size ||
        config->codec < MEDIA_AUDIO_CODEC_AAC || config->codec > MEDIA_AUDIO_CODEC_OPUS) {
        return MEDIA_ERR_ARGUMENT;
    }
    *output = NULL;
    media_audio_decoder *decoder = calloc(1, sizeof(*decoder));
    if (decoder == NULL) {
        return MEDIA_ERR_MEMORY;
    }
    decoder->buffer = malloc(config->buffer_size);
    if (decoder->buffer == NULL) {
        free(decoder);
        return MEDIA_ERR_MEMORY;
    }
    decoder->capacity = config->buffer_size;
    decoder->config = *config;
    decoder->context = context;
    media_player_state *player = &context->player;
    (void)pthread_mutex_lock(&player->lock);
    if (player->decoder != NULL) {
        player->decoder->closed = true;
        player->decoder->context = NULL;
    }
    media_player_input_reset_locked(player);
    media_audio_source_reset(&player->main_source);
    player->decoder_generation++;
    decoder->generation = player->decoder_generation;
    player->decoder = decoder;
    player->main_source = (media_audio_source) {
        .kind = MEDIA_SOURCE_DECODER,
        .format = media_codec_decoder_format(config->codec),
        .decoder = MEDIA_AUDIO_MAIN_DECODER,
    };
    player->state = player->prompt_source.kind == MEDIA_SOURCE_NONE ?
        MEDIA_AUDIO_PLAY_PLAYING : MEDIA_AUDIO_PLAY_PAUSED_BY_PROMPT;
    (void)pthread_cond_broadcast(&player->changed);
    (void)pthread_mutex_unlock(&player->lock);
    *output = decoder;
    return MEDIA_OK;
}

static media_status media_audio_decoder_write_bytes(media_audio_decoder *decoder,
    const void *data, size_t size, int timeout_ms)
{
    if (decoder == NULL || data == NULL || size == 0 || size > decoder->capacity || timeout_ms < 0 ||
        decoder->context == NULL) {
        return MEDIA_ERR_ARGUMENT;
    }
    media_player_state *player = &decoder->context->player;
    struct timespec deadline;
    if (!media_make_deadline(timeout_ms, &deadline)) {
        return MEDIA_ERR_ARGUMENT;
    }
    (void)pthread_mutex_lock(&player->lock);
    while (!decoder->closed && decoder->capacity - decoder->count < size) {
        int result = pthread_cond_timedwait(&player->space, &player->lock, &deadline);
        if (result == ETIMEDOUT) {
            (void)pthread_mutex_unlock(&player->lock);
            return MEDIA_ERR_TIMEOUT;
        }
        if (result != 0) {
            (void)pthread_mutex_unlock(&player->lock);
            return MEDIA_ERR_DEVICE;
        }
    }
    if (decoder->closed || decoder->finished) {
        (void)pthread_mutex_unlock(&player->lock);
        return MEDIA_ERR_STATE;
    }
    size_t first = size;
    if (first > decoder->capacity - decoder->write_index) {
        first = decoder->capacity - decoder->write_index;
    }
    if (memcpy_s(decoder->buffer + decoder->write_index,
        decoder->capacity - decoder->write_index, data, first) != EOK) {
        (void)pthread_mutex_unlock(&player->lock);
        return MEDIA_ERR_MEMORY;
    }
    size_t second = size - first;
    if (second > 0 && memcpy_s(decoder->buffer, decoder->capacity,
        (const uint8_t *)data + first, second) != EOK) {
        (void)pthread_mutex_unlock(&player->lock);
        return MEDIA_ERR_MEMORY;
    }
    size_t tail = decoder->capacity - decoder->write_index;
    decoder->write_index = size >= tail ? size - tail : decoder->write_index + size;
    decoder->count += size;
    (void)pthread_cond_broadcast(&player->changed);
    (void)pthread_mutex_unlock(&player->lock);
    return MEDIA_OK;
}

media_status media_audio_decoder_write(media_audio_decoder *decoder,
    const void *data, size_t size, int timeout_ms)
{
    if (decoder == NULL || decoder->config.codec == MEDIA_AUDIO_CODEC_OPUS) {
        return MEDIA_ERR_ARGUMENT;
    }
    return media_audio_decoder_write_bytes(decoder, data, size, timeout_ms);
}

media_status media_audio_decoder_write_packet(media_audio_decoder *decoder,
    const void *packet, size_t size, int timeout_ms)
{
    if (decoder == NULL || decoder->config.codec != MEDIA_AUDIO_CODEC_OPUS ||
        packet == NULL || size == 0 || size > MEDIA_OPUS_MAX_PACKET_SIZE) {
        return MEDIA_ERR_ARGUMENT;
    }
    uint8_t framed[4 + MEDIA_OPUS_MAX_PACKET_SIZE] = {
        (uint8_t)(size >> 24),
        (uint8_t)(size >> 16),
        (uint8_t)(size >> 8),
        (uint8_t)size,
    };
    if (memcpy_s(framed + 4, sizeof(framed) - 4, packet, size) != EOK) {
        return MEDIA_ERR_MEMORY;
    }
    return media_audio_decoder_write_bytes(decoder, framed, size + 4, timeout_ms);
}

media_status media_audio_decoder_finish(media_audio_decoder *decoder)
{
    if (decoder == NULL || decoder->context == NULL) {
        return MEDIA_ERR_STATE;
    }
    media_player_state *player = &decoder->context->player;
    (void)pthread_mutex_lock(&player->lock);
    decoder->finished = true;
    (void)pthread_cond_broadcast(&player->changed);
    (void)pthread_mutex_unlock(&player->lock);
    return MEDIA_OK;
}

void media_audio_decoder_close(media_audio_decoder *decoder)
{
    if (decoder == NULL) {
        return;
    }
    media_context *context = decoder->context;
    if (context != NULL && context->player.initialized) {
        media_player_state *player = &context->player;
        (void)pthread_mutex_lock(&player->lock);
        decoder->closed = true;
        if (player->decoder == decoder) {
            media_audio_source_reset(&player->main_source);
            player->decoder = NULL;
        }
        (void)pthread_cond_broadcast(&player->changed);
        (void)pthread_cond_broadcast(&player->space);
        (void)pthread_mutex_unlock(&player->lock);
    }
    free(decoder->buffer);
    free(decoder);
}

media_status media_audio_prompt_load(media_context *context, media_audio_format format,
    const void *data, size_t size, media_audio_prompt **output)
{
    if (context == NULL || output == NULL || !context->player.initialized) {
        return MEDIA_ERR_STATE;
    }
    *output = NULL;
    media_audio_prompt *prompt = calloc(1, sizeof(*prompt));
    if (prompt == NULL) {
        return MEDIA_ERR_MEMORY;
    }
    media_status status = media_normalize_audio(context, format, data, size,
        &prompt->format, &prompt->data, &prompt->size);
    if (status != MEDIA_OK) {
        free(prompt);
        return status;
    }
    *output = prompt;
    return MEDIA_OK;
}

static void media_prompt_queue_clear_locked(media_player_state *player)
{
    for (size_t i = 0; i < MEDIA_PROMPT_QUEUE_DEPTH; i++) {
        media_prompt_entry_clear(&player->prompt_queue[i]);
    }
    player->prompt_read_index = 0;
    player->prompt_write_index = 0;
    player->prompt_count = 0;
}

media_status media_audio_prompt_play(media_context *context, const media_audio_prompt *prompt,
    media_prompt_policy policy)
{
    if (context == NULL || prompt == NULL || prompt->data == NULL || prompt->size == 0 ||
        !context->player.initialized || (policy != MEDIA_PROMPT_QUEUE && policy != MEDIA_PROMPT_REPLACE)) {
        return MEDIA_ERR_ARGUMENT;
    }
    uint8_t *copy = malloc(prompt->size);
    if (copy == NULL) {
        return MEDIA_ERR_MEMORY;
    }
    if (memcpy_s(copy, prompt->size, prompt->data, prompt->size) != EOK) {
        free(copy);
        return MEDIA_ERR_MEMORY;
    }
    media_player_state *player = &context->player;
    (void)pthread_mutex_lock(&player->lock);
    if (policy == MEDIA_PROMPT_REPLACE) {
        if (ss_mpi_ao_clr_chn_buf(MEDIA_AUDIO_DEVICE,
                MEDIA_AUDIO_CHANNEL) != TD_SUCCESS) {
            (void)pthread_mutex_unlock(&player->lock);
            free(copy);
            return MEDIA_ERR_DEVICE;
        }
        media_audio_source_reset(&player->prompt_source);
        media_prompt_queue_clear_locked(player);
    } else if (player->prompt_count == MEDIA_PROMPT_QUEUE_DEPTH) {
        (void)pthread_mutex_unlock(&player->lock);
        free(copy);
        return MEDIA_ERR_BUSY;
    }
    media_prompt_entry *entry = &player->prompt_queue[player->prompt_write_index];
    entry->format = prompt->format;
    entry->data = copy;
    entry->size = prompt->size;
    player->prompt_write_index = (player->prompt_write_index + 1) % MEDIA_PROMPT_QUEUE_DEPTH;
    player->prompt_count++;
    player->state = MEDIA_AUDIO_PLAY_PAUSED_BY_PROMPT;
    (void)pthread_cond_broadcast(&player->changed);
    (void)pthread_mutex_unlock(&player->lock);
    return MEDIA_OK;
}

void media_audio_prompt_destroy(media_audio_prompt *prompt)
{
    if (prompt == NULL) {
        return;
    }
    free(prompt->data);
    free(prompt);
}

static bool media_encoder_submit_frame_locked(media_audio_encoder *encoder,
    const hi_mapi_audio_frame *frame)
{
    if (encoder == NULL || encoder->closed || !encoder->started) {
        return false;
    }
    hi_mapi_audio_frame mono = *frame;
    mono.snd_mode = OT_AUDIO_SOUND_MODE_MONO;
    mono.virt_addr[1] = NULL;
    mono.phys_addr[1] = 0;
    return hi_mapi_aenc_send_frame(encoder->handle, &mono) == TD_SUCCESS;
}

static void media_encoder_queue_stream_locked(media_audio_encoder *encoder,
    const ot_audio_stream *stream)
{
    if (stream->stream != NULL && stream->len > MEDIA_OPUS_PRIVATE_HEADER_SIZE) {
        size_t payload_size = stream->len - MEDIA_OPUS_PRIVATE_HEADER_SIZE;
        uint32_t declared_size = media_read_be32(stream->stream);
        if (declared_size == payload_size &&
            payload_size <= MEDIA_OPUS_MAX_PACKET_SIZE) {
            if (encoder->count == MEDIA_AUDIO_ENCODE_QUEUE_DEPTH) {
                encoder->read_index = (encoder->read_index + 1U) %
                    MEDIA_AUDIO_ENCODE_QUEUE_DEPTH;
                encoder->count--;
            }
            media_encoded_block *encoded =
                &encoder->blocks[encoder->write_index];
            if (memcpy_s(encoded->data, sizeof(encoded->data),
                    stream->stream + MEDIA_OPUS_PRIVATE_HEADER_SIZE,
                    payload_size) == EOK) {
                encoded->size = payload_size;
                encoded->pts = stream->time_stamp;
                encoded->sequence = encoder->next_sequence++;
                encoder->write_index = (encoder->write_index + 1U) %
                    MEDIA_AUDIO_ENCODE_QUEUE_DEPTH;
                encoder->count++;
                (void)pthread_cond_signal(&encoder->ready);
            }
        }
    }
}

static void *media_encoder_worker(void *argument)
{
    media_audio_encoder *encoder = argument;
    media_capture_state *capture = &encoder->context->capture;
    while (true) {
        (void)pthread_mutex_lock(&capture->lock);
        bool running = encoder->worker_running && !encoder->closed;
        (void)pthread_mutex_unlock(&capture->lock);
        if (!running) {
            break;
        }
        ot_audio_stream stream = {0};
        if (ss_mpi_aenc_get_stream(encoder->handle, &stream,
                MEDIA_AUDIO_ENCODE_TIMEOUT_MS) != TD_SUCCESS) {
            continue;
        }
        (void)pthread_mutex_lock(&capture->lock);
        if (encoder->worker_running && !encoder->closed) {
            media_encoder_queue_stream_locked(encoder, &stream);
        }
        (void)pthread_mutex_unlock(&capture->lock);
        (void)ss_mpi_aenc_release_stream(encoder->handle, &stream);
    }
    return NULL;
}

static void media_encoder_stop_worker(media_audio_encoder *encoder)
{
    if (encoder->worker_started) {
        (void)pthread_join(encoder->worker, NULL);
        encoder->worker_started = false;
    }
}

static void media_encoder_stop(media_audio_encoder *encoder)
{
    if (encoder->started) {
        (void)hi_mapi_aenc_stop(encoder->handle);
        encoder->started = false;
    }
    if (encoder->initialized) {
        (void)hi_mapi_aenc_deinit(encoder->handle);
        encoder->initialized = false;
    }
}

static void *media_capture_worker(void *argument)
{
    media_context *context = argument;
    media_capture_state *capture = &context->capture;
    while (true) {
        (void)pthread_mutex_lock(&capture->lock);
        bool running = capture->worker_running;
        (void)pthread_mutex_unlock(&capture->lock);
        if (!running) {
            break;
        }
        ot_audio_frame frame = {0};
        ot_aec_frame aec = {0};
        if (ss_mpi_ai_get_frame(MEDIA_AUDIO_DEVICE, MEDIA_AUDIO_CHANNEL,
                &frame, &aec, MEDIA_AUDIO_CAPTURE_TIMEOUT_MS) != TD_SUCCESS) {
            continue;
        }
        size_t sample_count = frame.len / sizeof(int16_t);
        if (sample_count > MEDIA_AUDIO_MAX_FRAME_SAMPLES) {
            sample_count = MEDIA_AUDIO_MAX_FRAME_SAMPLES;
        }
        if (frame.virt_addr[0] != NULL && sample_count > 0) {
            (void)pthread_mutex_lock(&capture->lock);
            if (capture->count == MEDIA_CAPTURE_QUEUE_DEPTH) {
                capture->read_index = (capture->read_index + 1) % MEDIA_CAPTURE_QUEUE_DEPTH;
                capture->count--;
            }
            media_capture_block *block = &capture->blocks[capture->write_index];
            if (memcpy_s(block->samples, sizeof(block->samples),
                frame.virt_addr[0], sample_count * sizeof(int16_t)) == EOK) {
                block->sample_count = sample_count;
                block->pts = frame.time_stamp;
                capture->write_index = (capture->write_index + 1) % MEDIA_CAPTURE_QUEUE_DEPTH;
                capture->count++;
                (void)pthread_cond_signal(&capture->ready);
            }
            bool frame_submitted = media_encoder_submit_frame_locked(
                capture->encoder, &frame);
            if (capture->encoder != NULL && !frame_submitted) {
                fprintf(stderr, "media_engine AENC rejected AI frame: "
                    "seq=%u pts=%llu\n", frame.seq,
                    (unsigned long long)frame.time_stamp);
            }
            (void)pthread_mutex_unlock(&capture->lock);
        }
        (void)ss_mpi_ai_release_frame(MEDIA_AUDIO_DEVICE,
            MEDIA_AUDIO_CHANNEL, &frame, &aec);
    }
    return NULL;
}

media_status media_capture_start(media_context *context)
{
    if (!context->config.audio_input.enabled) {
        return MEDIA_OK;
    }
    media_capture_state *capture = &context->capture;
    if (pthread_mutex_init(&capture->lock, NULL) != 0) {
        return MEDIA_ERR_DEVICE;
    }
    if (pthread_cond_init(&capture->ready, NULL) != 0) {
        (void)pthread_mutex_destroy(&capture->lock);
        return MEDIA_ERR_DEVICE;
    }
    capture->read_index = 0;
    capture->write_index = 0;
    capture->count = 0;
    capture->initialized = true;
    hi_mapi_acap_attr attributes = {0};
    const media_audio_input_config *config = &context->config.audio_input;
    const hi_mapi_audio_sound_mode sound_mode = config->use_talk_v2 ?
        HI_MAPI_AUDIO_SOUND_MODE_STEREO : HI_MAPI_AUDIO_SOUND_MODE_MONO;
    if (hi_mapi_acap_get_default_attr((hi_mapi_audio_sample_rate)config->sample_rate,
        sound_mode, &attributes) != TD_SUCCESS) {
        media_capture_stop(context);
        return MEDIA_ERR_DEVICE;
    }
    attributes.pt_num_per_frm = config->frame_samples;
    attributes.track_mode = config->physical_channel == 1 ?
        HI_MAPI_AUDIO_TRACK_EXCHANGE : HI_MAPI_AUDIO_TRACK_NORMAL;
    switch (config->input) {
        case MEDIA_AUDIO_INPUT_IN0:
            attributes.mixer_mic_mode = HI_MAPI_ACODEC_MIXER_IN0;
            break;
        case MEDIA_AUDIO_INPUT_IN1:
            attributes.mixer_mic_mode = HI_MAPI_ACODEC_MIXER_IN1;
            break;
        case MEDIA_AUDIO_INPUT_DIFFERENTIAL:
            attributes.mixer_mic_mode = HI_MAPI_ACODEC_MIXER_IN_D;
            break;
        default:
            media_capture_stop(context);
            return MEDIA_ERR_ARGUMENT;
    }
    if (hi_mapi_acap_init(MEDIA_AUDIO_DEVICE, &attributes) != TD_SUCCESS) {
        media_capture_stop(context);
        return MEDIA_ERR_DEVICE;
    }
    capture->device_initialized = true;
    if (hi_mapi_acap_start(MEDIA_AUDIO_DEVICE, MEDIA_AUDIO_CHANNEL) != TD_SUCCESS) {
        media_capture_stop(context);
        return MEDIA_ERR_DEVICE;
    }
    capture->started = true;
    ot_ai_chn_param channel_param = {0};
    if (ss_mpi_ai_get_chn_param(MEDIA_AUDIO_DEVICE, MEDIA_AUDIO_CHANNEL,
            &channel_param) != TD_SUCCESS) {
        media_capture_stop(context);
        return MEDIA_ERR_DEVICE;
    }
    channel_param.usr_frame_depth = MEDIA_AUDIO_CAPTURE_FRAME_DEPTH;
    if (ss_mpi_ai_set_chn_param(MEDIA_AUDIO_DEVICE, MEDIA_AUDIO_CHANNEL,
            &channel_param) != TD_SUCCESS) {
        media_capture_stop(context);
        return MEDIA_ERR_DEVICE;
    }
    if (hi_mapi_acap_set_volume(MEDIA_AUDIO_DEVICE, config->gain_db) != TD_SUCCESS) {
        media_capture_stop(context);
        return MEDIA_ERR_DEVICE;
    }
    td_s32 effective_gain = 0;
    if (hi_mapi_acap_get_volume(MEDIA_AUDIO_DEVICE, &effective_gain) != TD_SUCCESS) {
        media_capture_stop(context);
        return MEDIA_ERR_DEVICE;
    }
    fprintf(stderr, "media_engine microphone gain: requested=%d dB effective=%d dB\n",
        config->gain_db, effective_gain);
    if (config->enable_aec) {
        const hi_mapi_acap_vqe_type vqe_type = config->use_talk_v2 ?
            HI_MAPI_ACAP_VQE_TYPE_TALKV2 : HI_MAPI_ACAP_VQE_TYPE_TALK;
        if (hi_mapi_acap_enable_vqe(MEDIA_AUDIO_DEVICE, MEDIA_AUDIO_CHANNEL,
            vqe_type, MEDIA_AUDIO_DEVICE) != TD_SUCCESS) {
            media_capture_stop(context);
            return MEDIA_ERR_DEVICE;
        }
        capture->vqe_enabled = true;
    }
    (void)pthread_mutex_lock(&capture->lock);
    capture->worker_running = true;
    (void)pthread_mutex_unlock(&capture->lock);
    if (pthread_create(&capture->worker, NULL, media_capture_worker, context) != 0) {
        (void)pthread_mutex_lock(&capture->lock);
        capture->worker_running = false;
        (void)pthread_mutex_unlock(&capture->lock);
        media_capture_stop(context);
        return MEDIA_ERR_DEVICE;
    }
    capture->worker_started = true;
    return MEDIA_OK;
}

void media_capture_stop(media_context *context)
{
    media_capture_state *capture = &context->capture;
    if (!capture->initialized) {
        return;
    }
    media_audio_encoder *detached_encoder = NULL;
    (void)pthread_mutex_lock(&capture->lock);
    capture->worker_running = false;
    if (capture->encoder != NULL) {
        detached_encoder = capture->encoder;
        detached_encoder->closed = true;
        detached_encoder->worker_running = false;
        (void)pthread_cond_broadcast(&detached_encoder->ready);
        capture->encoder = NULL;
    }
    (void)pthread_cond_broadcast(&capture->ready);
    (void)pthread_mutex_unlock(&capture->lock);
    if (capture->worker_started) {
        (void)pthread_join(capture->worker, NULL);
        capture->worker_started = false;
    }
    if (detached_encoder != NULL) {
        media_encoder_stop_worker(detached_encoder);
        media_encoder_stop(detached_encoder);
        detached_encoder->context = NULL;
    }
    if (capture->vqe_enabled) {
        (void)hi_mapi_acap_disable_vqe(MEDIA_AUDIO_DEVICE, MEDIA_AUDIO_CHANNEL);
        capture->vqe_enabled = false;
    }
    if (capture->started) {
        (void)hi_mapi_acap_stop(MEDIA_AUDIO_DEVICE, MEDIA_AUDIO_CHANNEL);
        capture->started = false;
    }
    if (capture->device_initialized) {
        (void)hi_mapi_acap_deinit(MEDIA_AUDIO_DEVICE);
        capture->device_initialized = false;
    }
    (void)pthread_cond_destroy(&capture->ready);
    (void)pthread_mutex_destroy(&capture->lock);
    capture->initialized = false;
}

media_status media_audio_input_read(media_context *context, int16_t *samples,
    size_t capacity, int timeout_ms, size_t *sample_count, uint64_t *pts)
{
    if (context == NULL || samples == NULL || capacity == 0 ||
        capacity > SIZE_MAX / sizeof(*samples) || timeout_ms < 0 ||
        sample_count == NULL || pts == NULL || !context->capture.initialized) {
        return MEDIA_ERR_ARGUMENT;
    }
    *sample_count = 0;
    *pts = 0;
    struct timespec deadline;
    if (!media_make_deadline(timeout_ms, &deadline)) {
        return MEDIA_ERR_ARGUMENT;
    }
    media_capture_state *capture = &context->capture;
    (void)pthread_mutex_lock(&capture->lock);
    while (capture->count == 0 && capture->worker_running) {
        int result = pthread_cond_timedwait(&capture->ready, &capture->lock, &deadline);
        if (result == ETIMEDOUT) {
            (void)pthread_mutex_unlock(&capture->lock);
            return MEDIA_ERR_TIMEOUT;
        }
        if (result != 0) {
            (void)pthread_mutex_unlock(&capture->lock);
            return MEDIA_ERR_DEVICE;
        }
    }
    if (capture->count == 0) {
        (void)pthread_mutex_unlock(&capture->lock);
        return MEDIA_ERR_STATE;
    }
    media_capture_block *block = &capture->blocks[capture->read_index];
    if (capacity < block->sample_count ||
        memcpy_s(samples, capacity * sizeof(*samples), block->samples,
            block->sample_count * sizeof(*samples)) != EOK) {
        (void)pthread_mutex_unlock(&capture->lock);
        return MEDIA_ERR_ARGUMENT;
    }
    *sample_count = block->sample_count;
    *pts = block->pts;
    capture->read_index = (capture->read_index + 1) % MEDIA_CAPTURE_QUEUE_DEPTH;
    capture->count--;
    (void)pthread_mutex_unlock(&capture->lock);
    return MEDIA_OK;
}

static bool media_opus_bit_rate_supported(uint32_t bit_rate)
{
    switch (bit_rate) {
        case HI_MAPI_OPUS_BPS_16K:
        case HI_MAPI_OPUS_BPS_22K:
        case HI_MAPI_OPUS_BPS_24K:
        case HI_MAPI_OPUS_BPS_32K:
        case HI_MAPI_OPUS_BPS_44K:
        case HI_MAPI_OPUS_BPS_48K:
        case HI_MAPI_OPUS_BPS_64K:
        case HI_MAPI_OPUS_BPS_96K:
        case HI_MAPI_OPUS_BPS_128K:
        case HI_MAPI_OPUS_BPS_132K:
        case HI_MAPI_OPUS_BPS_144K:
        case HI_MAPI_OPUS_BPS_192K:
        case HI_MAPI_OPUS_BPS_256K:
        case HI_MAPI_OPUS_BPS_288K:
        case HI_MAPI_OPUS_BPS_320K:
            return true;
        default:
            return false;
    }
}

media_status media_audio_encoder_open(media_context *context,
    const media_audio_encoder_config *config, media_audio_encoder **output)
{
    if (output == NULL) {
        return MEDIA_ERR_ARGUMENT;
    }
    *output = NULL;
    if (context == NULL || config == NULL) {
        return MEDIA_ERR_ARGUMENT;
    }
    if (!context->capture.initialized) {
        return MEDIA_ERR_STATE;
    }
    if (config->codec != MEDIA_AUDIO_CODEC_OPUS) {
        return MEDIA_ERR_UNSUPPORTED;
    }
    if (config->sample_rate != context->config.audio_input.sample_rate ||
        config->channels != 1 ||
        config->frame_samples != context->config.audio_input.frame_samples ||
        !media_opus_bit_rate_supported(config->bit_rate)) {
        return MEDIA_ERR_ARGUMENT;
    }
    if (config->frame_samples != 40 && config->frame_samples != 80 &&
        config->frame_samples != 160 && config->frame_samples != 320 &&
        config->frame_samples != 640 && config->frame_samples != 960) {
        return MEDIA_ERR_UNSUPPORTED;
    }
    media_audio_encoder *encoder = calloc(1, sizeof(*encoder));
    if (encoder == NULL) {
        return MEDIA_ERR_MEMORY;
    }
    if (pthread_cond_init(&encoder->ready, NULL) != 0) {
        free(encoder);
        return MEDIA_ERR_DEVICE;
    }
    encoder->context = context;
    encoder->config = *config;
    encoder->handle = MEDIA_AUDIO_ENCODER;

    hi_mapi_aenc_opus_attr opus = {
        .sample_rate = (hi_mapi_audio_sample_rate)config->sample_rate,
        .bit_width = HI_MAPI_AUDIO_BIT_WIDTH_16,
        .sound_mode = HI_MAPI_AUDIO_SOUND_MODE_MONO,
        .bit_rate = (hi_mapi_aenc_opus_bps)config->bit_rate,
        .application = HI_MAPI_OPUS_APPLICATION_VOIP,
    };
    hi_mapi_aenc_attr attributes = {
        .aenc_format = HI_MAPI_AUDIO_FORMAT_OPUS,
        .pt_num_per_frm = config->frame_samples,
        .value = &opus,
        .value_len = sizeof(opus),
    };
    if (hi_mapi_aenc_init(encoder->handle, &attributes) != TD_SUCCESS) {
        (void)pthread_cond_destroy(&encoder->ready);
        free(encoder);
        return MEDIA_ERR_DEVICE;
    }
    encoder->initialized = true;
    const hi_mapi_audio_payload_format payload =
        HI_MAPI_AUDIO_PAYLOAD_STANDARD;
    if (hi_mapi_aenc_set_param(encoder->handle,
            HI_MAPI_AENC_CMD_PAYLOAD_FORMAT_CONFIG, &payload,
            sizeof(payload)) != TD_SUCCESS) {
        media_encoder_stop(encoder);
        (void)pthread_cond_destroy(&encoder->ready);
        free(encoder);
        return MEDIA_ERR_DEVICE;
    }
    if (hi_mapi_aenc_start(encoder->handle) != TD_SUCCESS) {
        media_encoder_stop(encoder);
        (void)pthread_cond_destroy(&encoder->ready);
        free(encoder);
        return MEDIA_ERR_DEVICE;
    }
    encoder->started = true;

    media_capture_state *capture = &context->capture;
    (void)pthread_mutex_lock(&capture->lock);
    if (capture->encoder != NULL || !capture->worker_running) {
        (void)pthread_mutex_unlock(&capture->lock);
        media_encoder_stop(encoder);
        (void)pthread_cond_destroy(&encoder->ready);
        free(encoder);
        return MEDIA_ERR_BUSY;
    }
    capture->encoder = encoder;
    encoder->worker_running = true;
    (void)pthread_mutex_unlock(&capture->lock);
    if (pthread_create(&encoder->worker, NULL,
            media_encoder_worker, encoder) != 0) {
        (void)pthread_mutex_lock(&capture->lock);
        if (capture->encoder == encoder) {
            capture->encoder = NULL;
        }
        encoder->worker_running = false;
        encoder->closed = true;
        (void)pthread_mutex_unlock(&capture->lock);
        media_encoder_stop(encoder);
        (void)pthread_cond_destroy(&encoder->ready);
        free(encoder);
        return MEDIA_ERR_DEVICE;
    }
    encoder->worker_started = true;
    *output = encoder;
    return MEDIA_OK;
}

media_status media_audio_encoder_read_packet(media_audio_encoder *encoder,
    void *packet, size_t capacity, int timeout_ms, size_t *packet_size,
    uint64_t *pts, uint32_t *sequence)
{
    if (encoder == NULL || packet == NULL || capacity == 0 || timeout_ms < 0 ||
        packet_size == NULL || pts == NULL || sequence == NULL) {
        return MEDIA_ERR_ARGUMENT;
    }
    if (encoder->context == NULL || encoder->closed) {
        return MEDIA_ERR_STATE;
    }
    *packet_size = 0;
    *pts = 0;
    *sequence = 0;
    struct timespec deadline;
    if (!media_make_deadline(timeout_ms, &deadline)) {
        return MEDIA_ERR_ARGUMENT;
    }
    media_capture_state *capture = &encoder->context->capture;
    (void)pthread_mutex_lock(&capture->lock);
    while (encoder->count == 0 && !encoder->closed && capture->worker_running) {
        int result = pthread_cond_timedwait(&encoder->ready, &capture->lock, &deadline);
        if (result == ETIMEDOUT) {
            (void)pthread_mutex_unlock(&capture->lock);
            return MEDIA_ERR_TIMEOUT;
        }
        if (result != 0) {
            (void)pthread_mutex_unlock(&capture->lock);
            return MEDIA_ERR_DEVICE;
        }
    }
    if (encoder->closed || encoder->count == 0) {
        (void)pthread_mutex_unlock(&capture->lock);
        return MEDIA_ERR_STATE;
    }
    media_encoded_block *block = &encoder->blocks[encoder->read_index];
    if (capacity < block->size ||
        memcpy_s(packet, capacity, block->data, block->size) != EOK) {
        (void)pthread_mutex_unlock(&capture->lock);
        return MEDIA_ERR_ARGUMENT;
    }
    *packet_size = block->size;
    *pts = block->pts;
    *sequence = block->sequence;
    encoder->read_index = (encoder->read_index + 1U) % MEDIA_AUDIO_ENCODE_QUEUE_DEPTH;
    encoder->count--;
    (void)pthread_mutex_unlock(&capture->lock);
    return MEDIA_OK;
}

void media_audio_encoder_close(media_audio_encoder *encoder)
{
    if (encoder == NULL) {
        return;
    }
    media_context *context = encoder->context;
    if (context != NULL && context->capture.initialized) {
        media_capture_state *capture = &context->capture;
        (void)pthread_mutex_lock(&capture->lock);
        if (capture->encoder == encoder) {
            capture->encoder = NULL;
        }
        encoder->closed = true;
        encoder->worker_running = false;
        (void)pthread_cond_broadcast(&encoder->ready);
        (void)pthread_mutex_unlock(&capture->lock);
    } else {
        encoder->closed = true;
        encoder->worker_running = false;
    }
    media_encoder_stop_worker(encoder);
    media_encoder_stop(encoder);
    encoder->context = NULL;
    (void)pthread_cond_destroy(&encoder->ready);
    free(encoder);
}
