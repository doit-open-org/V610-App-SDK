/*
 * Overlay rasterising, kept free of SDK headers so it can be unit tested
 * on the host.  Callers supply a plain description of the frame planes.
 */

#include "media_overlay.h"

/*
 * Chroma for the overlay.  The VPSS frame is 4:2:0 semi-planar, so one chroma
 * sample covers a 2x2 luma block; the two byte orders differ only in whether
 * Cb or Cr comes first, which the frame's own pixel format settles.
 */
void media_rgb_to_chroma(uint8_t red, uint8_t green, uint8_t blue,
    uint8_t *cb, uint8_t *cr)
{
    const int32_t r = (int32_t)red;
    const int32_t g = (int32_t)green;
    const int32_t b = (int32_t)blue;
    const int32_t u = 128 + ((-43 * r - 85 * g + 128 * b) >> 8);
    const int32_t v = 128 + ((128 * r - 107 * g - 21 * b) >> 8);
    *cb = (uint8_t)(u < 0 ? 0 : (u > 255 ? 255 : u));
    *cr = (uint8_t)(v < 0 ? 0 : (v > 255 ? 255 : v));
}

void media_plot(const media_overlay_target *target, int32_t x, int32_t y,
    uint8_t luma_value, uint8_t cb, uint8_t cr)
{
    if (x < 0 || y < 0 || (uint32_t)x >= target->width ||
        (uint32_t)y >= target->height) {
        return;
    }
    target->luma[(size_t)y * target->luma_stride + (uint32_t)x] = luma_value;
    if (target->chroma == NULL) {
        return;
    }
    const size_t offset = (size_t)((uint32_t)y / 2U) * target->chroma_stride +
        ((uint32_t)x / 2U) * 2U;
    target->chroma[offset] = target->cr_first ? cr : cb;
    target->chroma[offset + 1U] = target->cr_first ? cb : cr;
}

/*
 * A filled disc, for the round dot a keypoint wants.  The comparison is
 * against radius^2 + radius rather than radius^2, which is the usual midpoint
 * form: it takes in the pixels whose centres fall within half a pixel of the
 * circle, and without it a small disc comes out visibly cornered.
 */
void media_draw_disc(const media_overlay_target *target, int32_t x, int32_t y,
    uint32_t diameter, uint8_t luma_value, uint8_t cb, uint8_t cr)
{
    const int32_t radius = (int32_t)(diameter == 0U ? 1U : diameter) / 2;
    const int32_t inside = radius * radius + radius;
    for (int32_t oy = -radius; oy <= radius; oy++) {
        for (int32_t ox = -radius; ox <= radius; ox++) {
            if (ox * ox + oy * oy <= inside) {
                media_plot(target, x + ox, y + oy, luma_value, cb, cr);
            }
        }
    }
}

void media_draw_line(const media_overlay_target *target,
    int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint32_t thickness,
    uint8_t luma_value, uint8_t cb, uint8_t cr)
{
    /* A line that starts where it ends is a dot, and a dot is round. */
    if (x1 == x2 && y1 == y2) {
        media_draw_disc(target, x1, y1, thickness, luma_value, cb, cr);
        return;
    }
    /* Bresenham, widened by stamping a square at every step. */
    const int32_t dx = x2 > x1 ? x2 - x1 : x1 - x2;
    const int32_t dy = y2 > y1 ? y2 - y1 : y1 - y2;
    const int32_t step_x = x1 < x2 ? 1 : -1;
    const int32_t step_y = y1 < y2 ? 1 : -1;
    const int32_t half = (int32_t)(thickness == 0U ? 1U : thickness) / 2;
    int32_t error = (dx > dy ? dx : -dy) / 2;
    int32_t x = x1;
    int32_t y = y1;
    for (uint32_t guard = 0; guard < MEDIA_OVERLAY_MAX_STEPS; ++guard) {
        for (int32_t oy = -half; oy <= half; ++oy) {
            for (int32_t ox = -half; ox <= half; ++ox) {
                media_plot(target, x + ox, y + oy, luma_value, cb, cr);
            }
        }
        if (x == x2 && y == y2) {
            break;
        }
        const int32_t previous = error;
        if (previous > -dx) {
            error -= dy;
            x += step_x;
        }
        if (previous < dy) {
            error += dx;
            y += step_y;
        }
    }
}

