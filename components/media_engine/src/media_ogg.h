#ifndef MEDIA_ENGINE_OGG_H
#define MEDIA_ENGINE_OGG_H

#include "media_engine/media.h"

#define MEDIA_OPUS_MAX_PACKET_SIZE 1500U

media_status media_ogg_opus_to_packets(const void *data, size_t size,
    uint8_t **packets, size_t *packets_size);

#endif
