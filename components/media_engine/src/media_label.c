/*
 * Captions drawn by the hardware region engine.
 *
 * Boxes and lines are rasterised into the frame by the CPU because they change
 * every frame and are many.  Text is the opposite: a handful of short strings
 * that change rarely, so it goes to RGN instead.  The bitmap is uploaded only
 * when the caption actually changes, and from then on the hardware composites
 * it during encode at no per-frame cost.
 *
 * One region per caption; the region engine allows eight overlays on a channel,
 * which is the cap in MEDIA_VIDEO_MAX_LABELS.
 *
 * The bitmap is CLUT2: two bits per pixel indexing a palette the region owns.
 * Index 0 is left transparent and index 1 carries the text, so the colour lives
 * in the palette and the pixels never have to be redrawn just to recolour a
 * caption.  At four pixels per byte a full width caption costs a few kilobytes.
 */

#include "media_label.h"
#include "media_font.h"

#include <string.h>

#define LABEL_PAD 2U
#define LABEL_PIXELS_PER_BYTE 4U
#define LABEL_BITS_PER_PIXEL 2U
#define LABEL_INDEX_TEXT 1U
/*
 * Region geometry must be even, and the engine wants a little room, so the
 * canvas is sized from the longest caption the API accepts rather than resized
 * per string.
 */
#define LABEL_MAX_WIDTH (((MEDIA_VIDEO_LABEL_CAPACITY * \
    (MEDIA_FONT_WIDTH + 1U) * MEDIA_VIDEO_LABEL_MAX_SCALE) + \
    (LABEL_PAD * 2U) + 3U) & ~3U)
#define LABEL_MAX_HEIGHT (((MEDIA_FONT_HEIGHT * \
    MEDIA_VIDEO_LABEL_MAX_SCALE) + (LABEL_PAD * 2U) + 1U) & ~1U)

/* Writes one palette index into the packed two bit per pixel canvas. */
static void put_index(uint8_t *canvas, uint32_t canvas_width, uint32_t x,
    uint32_t y, uint8_t index)
{
    const size_t stride = canvas_width / LABEL_PIXELS_PER_BYTE;
    const size_t offset = (size_t)y * stride + x / LABEL_PIXELS_PER_BYTE;
    /* The leftmost pixel of a byte occupies its most significant bits. */
    const uint32_t shift = (LABEL_PIXELS_PER_BYTE - 1U -
        (x % LABEL_PIXELS_PER_BYTE)) * LABEL_BITS_PER_PIXEL;
    canvas[offset] = (uint8_t)((canvas[offset] & ~(0x3U << shift)) |
        ((index & 0x3U) << shift));
}

/* Renders one caption into a CLUT2 canvas, returning the used extent. */
static void draw_label(const media_video_label *label, uint8_t *canvas,
    uint32_t canvas_width, uint32_t *used_width, uint32_t *used_height)
{
    const uint32_t scale = label->scale == 0 ? 1U : label->scale;
    const size_t length = strnlen(label->text, MEDIA_VIDEO_LABEL_CAPACITY);
    uint32_t pen = LABEL_PAD;
    for (size_t index = 0; index < length; ++index) {
        unsigned char character = (unsigned char)label->text[index];
        if (character < MEDIA_FONT_FIRST || character > MEDIA_FONT_LAST) {
            character = '?';
        }
        const uint8_t *glyph = g_media_font[character - MEDIA_FONT_FIRST];
        for (uint32_t row = 0; row < MEDIA_FONT_HEIGHT; ++row) {
            for (uint32_t column = 0; column < MEDIA_FONT_WIDTH; ++column) {
                if ((glyph[row] & (1U << (MEDIA_FONT_WIDTH - 1U - column)))
                    == 0) continue;
                /* Scaling repeats each lit pixel into a square block. */
                for (uint32_t dy = 0; dy < scale; ++dy) {
                    for (uint32_t dx = 0; dx < scale; ++dx) {
                        const uint32_t x = pen + column * scale + dx;
                        const uint32_t y = LABEL_PAD + row * scale + dy;
                        if (x >= canvas_width) continue;
                        put_index(canvas, canvas_width, x, y,
                            LABEL_INDEX_TEXT);
                    }
                }
            }
        }
        pen += (MEDIA_FONT_WIDTH + 1U) * scale;
    }
    *used_width = pen + LABEL_PAD;
    *used_height = LABEL_PAD * 2U + MEDIA_FONT_HEIGHT * scale;
}

static bool same_label(const media_video_label *a, const media_video_label *b)
{
    return a->scale == b->scale && a->red == b->red && a->green == b->green &&
        a->blue == b->blue && a->x == b->x && a->y == b->y &&
        strncmp(a->text, b->text, MEDIA_VIDEO_LABEL_CAPACITY) == 0;
}

media_status media_label_apply(media_video_stream_state *stream,
    const media_video_label *labels, size_t label_count)
{
    if (label_count > MEDIA_VIDEO_MAX_LABELS) return MEDIA_ERR_ARGUMENT;
    for (size_t index = 0; index < label_count; ++index) {
        const media_video_label *label = &labels[index];
        if (label->x < 0.0F || label->x > 1.0F || label->y < 0.0F ||
            label->y > 1.0F ||
            label->scale > MEDIA_VIDEO_LABEL_MAX_SCALE) {
            return MEDIA_ERR_ARGUMENT;
        }
    }
    return media_label_commit(stream, labels, label_count);
}

media_status media_label_render(const media_video_label *label,
    uint8_t *canvas, uint32_t canvas_width, uint32_t canvas_height,
    uint32_t *used_width, uint32_t *used_height)
{
    if (label == NULL || canvas == NULL || used_width == NULL ||
        used_height == NULL) return MEDIA_ERR_ARGUMENT;
    if (canvas_width < LABEL_MAX_WIDTH || canvas_height < LABEL_MAX_HEIGHT ||
        canvas_width % LABEL_PIXELS_PER_BYTE != 0) {
        return MEDIA_ERR_ARGUMENT;
    }
    /* Index 0 everywhere leaves the caption transparent until glyphs land. */
    memset(canvas, 0,
        (size_t)canvas_height * (canvas_width / LABEL_PIXELS_PER_BYTE));
    draw_label(label, canvas, canvas_width, used_width, used_height);
    return MEDIA_OK;
}

size_t media_label_canvas_bytes(void)
{
    return (size_t)LABEL_MAX_HEIGHT *
        (LABEL_MAX_WIDTH / LABEL_PIXELS_PER_BYTE);
}

bool media_label_changed(const media_video_label *a,
    const media_video_label *b)
{
    return !same_label(a, b);
}

uint32_t media_label_canvas_width(void)
{
    return LABEL_MAX_WIDTH;
}

uint32_t media_label_canvas_height(void)
{
    return LABEL_MAX_HEIGHT;
}
