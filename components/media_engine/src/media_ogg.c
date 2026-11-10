#include "media_ogg.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    uint8_t *data;
    size_t size;
    size_t capacity;
} media_byte_buffer;

static uint32_t media_ogg_read_le32(const uint8_t *data)
{
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8) |
        ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
}

static media_status media_buffer_append(media_byte_buffer *buffer,
    const void *data, size_t size)
{
    if (size > SIZE_MAX - buffer->size) {
        return MEDIA_ERR_MEMORY;
    }
    size_t required = buffer->size + size;
    if (required > buffer->capacity) {
        size_t capacity = buffer->capacity == 0 ? 256U : buffer->capacity;
        while (capacity < required) {
            if (capacity > SIZE_MAX / 2U) {
                capacity = required;
                break;
            }
            capacity *= 2U;
        }
        uint8_t *resized = realloc(buffer->data, capacity);
        if (resized == NULL) {
            return MEDIA_ERR_MEMORY;
        }
        buffer->data = resized;
        buffer->capacity = capacity;
    }
    if (size > 0) {
        memcpy(buffer->data + buffer->size, data, size);
    }
    buffer->size = required;
    return MEDIA_OK;
}

static media_status media_append_opus_packet(media_byte_buffer *output,
    const media_byte_buffer *packet)
{
    if (packet->size == 0 || packet->size > MEDIA_OPUS_MAX_PACKET_SIZE) {
        return MEDIA_ERR_FORMAT;
    }
    uint8_t header[4] = {
        (uint8_t)(packet->size >> 24),
        (uint8_t)(packet->size >> 16),
        (uint8_t)(packet->size >> 8),
        (uint8_t)packet->size,
    };
    media_status status = media_buffer_append(output, header, sizeof(header));
    return status == MEDIA_OK ?
        media_buffer_append(output, packet->data, packet->size) : status;
}

static media_status media_validate_opus_header(const media_byte_buffer *packet)
{
    if (packet->size < 19 || memcmp(packet->data, "OpusHead", 8) != 0 ||
        (packet->data[8] & 0xf0U) != 0 || packet->data[9] != 1 || packet->data[18] != 0) {
        return MEDIA_ERR_UNSUPPORTED;
    }
    return MEDIA_OK;
}

media_status media_ogg_opus_to_packets(const void *input, size_t size,
    uint8_t **packets, size_t *packets_size)
{
    if (input == NULL || packets == NULL || packets_size == NULL || size < 27) {
        return MEDIA_ERR_ARGUMENT;
    }
    *packets = NULL;
    *packets_size = 0;
    const uint8_t *data = input;
    media_byte_buffer packet = {0};
    media_byte_buffer output = {0};
    uint32_t stream_serial = 0;
    uint32_t expected_sequence = 0;
    size_t packet_index = 0;
    size_t offset = 0;
    bool stream_started = false;
    media_status status = MEDIA_OK;

    while (offset < size) {
        if (size - offset < 27 || memcmp(data + offset, "OggS", 4) != 0 ||
            data[offset + 4] != 0) {
            status = MEDIA_ERR_FORMAT;
            break;
        }
        uint8_t header_type = data[offset + 5];
        uint32_t serial = media_ogg_read_le32(data + offset + 14);
        uint32_t sequence = media_ogg_read_le32(data + offset + 18);
        size_t segment_count = data[offset + 26];
        if (size - offset - 27 < segment_count) {
            status = MEDIA_ERR_FORMAT;
            break;
        }
        if (!stream_started) {
            stream_started = true;
            stream_serial = serial;
            expected_sequence = sequence;
        }
        if (serial != stream_serial || sequence != expected_sequence ||
            (((header_type & 0x01U) != 0) != (packet.size != 0))) {
            status = MEDIA_ERR_UNSUPPORTED;
            break;
        }
        expected_sequence++;
        const uint8_t *segments = data + offset + 27;
        size_t body_size = 0;
        for (size_t i = 0; i < segment_count; i++) {
            if (segments[i] > SIZE_MAX - body_size) {
                status = MEDIA_ERR_FORMAT;
                break;
            }
            body_size += segments[i];
        }
        if (status != MEDIA_OK || body_size > size - offset - 27 - segment_count) {
            status = MEDIA_ERR_FORMAT;
            break;
        }
        const uint8_t *body = segments + segment_count;
        size_t body_offset = 0;
        for (size_t i = 0; i < segment_count; i++) {
            size_t part_size = segments[i];
            status = media_buffer_append(&packet, body + body_offset, part_size);
            body_offset += part_size;
            if (status != MEDIA_OK) {
                break;
            }
            if (part_size < 255U) {
                if (packet_index == 0) {
                    status = media_validate_opus_header(&packet);
                } else if (packet_index == 1) {
                    status = packet.size >= 8 && memcmp(packet.data, "OpusTags", 8) == 0 ?
                        MEDIA_OK : MEDIA_ERR_FORMAT;
                } else {
                    status = media_append_opus_packet(&output, &packet);
                }
                packet.size = 0;
                packet_index++;
                if (status != MEDIA_OK) {
                    break;
                }
            }
        }
        if (status != MEDIA_OK) {
            break;
        }
        offset += 27 + segment_count + body_size;
    }

    if (status == MEDIA_OK && (packet.size != 0 || packet_index < 3 || output.size == 0)) {
        status = MEDIA_ERR_FORMAT;
    }
    free(packet.data);
    if (status != MEDIA_OK) {
        free(output.data);
        return status;
    }
    *packets = output.data;
    *packets_size = output.size;
    return MEDIA_OK;
}
