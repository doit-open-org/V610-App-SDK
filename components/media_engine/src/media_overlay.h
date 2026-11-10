#ifndef MEDIA_OVERLAY_H
#define MEDIA_OVERLAY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MEDIA_OVERLAY_MAX_STEPS 8192U

/*
 * A frame to draw into.  chroma may be NULL, in which case only luma is
 * written; cr_first selects the byte order of the interleaved chroma plane.
 */
typedef struct {
    uint8_t *luma;
    uint8_t *chroma;
    uint32_t width;
    uint32_t height;
    uint32_t luma_stride;
    uint32_t chroma_stride;
    bool cr_first;
} media_overlay_target;

void media_rgb_to_chroma(uint8_t red, uint8_t green, uint8_t blue,
    uint8_t *cb, uint8_t *cr);
void media_plot(const media_overlay_target *target, int32_t x, int32_t y,
    uint8_t luma_value, uint8_t cb, uint8_t cr);
void media_draw_disc(const media_overlay_target *target, int32_t x, int32_t y,
    uint32_t diameter, uint8_t luma_value, uint8_t cb, uint8_t cr);
void media_draw_line(const media_overlay_target *target,
    int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint32_t thickness,
    uint8_t luma_value, uint8_t cb, uint8_t cr);

#endif
