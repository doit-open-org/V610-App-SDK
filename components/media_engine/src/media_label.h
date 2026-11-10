#ifndef MEDIA_LABEL_H
#define MEDIA_LABEL_H

#include "media_internal.h"

/* Pure rendering, free of MPP so it can be exercised on a host. */
/* Produces CLUT2 packed pixels: index 0 transparent, index 1 the text. */
media_status media_label_render(const media_video_label *label,
    uint8_t *canvas, uint32_t canvas_width, uint32_t canvas_height,
    uint32_t *used_width, uint32_t *used_height);
size_t media_label_canvas_bytes(void);
bool media_label_changed(const media_video_label *a,
    const media_video_label *b);
uint32_t media_label_canvas_width(void);
uint32_t media_label_canvas_height(void);

/* Validates then hands the set to the region engine. */
media_status media_label_apply(media_video_stream_state *stream,
    const media_video_label *labels, size_t label_count);
/* Implemented next to the rest of the MPP plumbing. */
media_status media_label_commit(media_video_stream_state *stream,
    const media_video_label *labels, size_t label_count);
void media_label_destroy_regions(media_video_stream_state *stream);

#endif
