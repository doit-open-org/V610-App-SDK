#ifndef MEDIA_ENGINE_MEDIA_H
#define MEDIA_ENGINE_MEDIA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MEDIA_VIDEO_MAX_CAMERAS 2U
#define MEDIA_VIDEO_MAX_STREAMS_PER_CAMERA 3U
#define MEDIA_VIDEO_MAX_STREAMS (MEDIA_VIDEO_MAX_CAMERAS * MEDIA_VIDEO_MAX_STREAMS_PER_CAMERA)
#define MEDIA_VIDEO_MAX_BOXES 32U
#define MEDIA_VIDEO_MAX_LINES 64U
#define MEDIA_VIDEO_MAX_LABELS 8U
#define MEDIA_VIDEO_LABEL_CAPACITY 32U
#define MEDIA_VIDEO_LABEL_MAX_SCALE 6U

typedef struct media_context media_context;
typedef struct media_video_frame media_video_frame;
typedef struct media_audio_decoder media_audio_decoder;
typedef struct media_audio_prompt media_audio_prompt;
typedef struct media_audio_encoder media_audio_encoder;

typedef enum {
    MEDIA_OK = 0,
    MEDIA_AGAIN = 1,
    MEDIA_EOF = 2,
    MEDIA_ERR_ARGUMENT = -1,
    MEDIA_ERR_STATE = -2,
    MEDIA_ERR_MEMORY = -3,
    MEDIA_ERR_DEVICE = -4,
    MEDIA_ERR_FORMAT = -5,
    MEDIA_ERR_TIMEOUT = -6,
    MEDIA_ERR_BUSY = -7,
    MEDIA_ERR_UNSUPPORTED = -8,
    MEDIA_ERR_IO = -9,
} media_status;

typedef enum {
    MEDIA_PIXEL_FORMAT_NV12 = 0,
} media_pixel_format;

typedef enum {
    MEDIA_VIDEO_OUTPUT_FRAME = 0,
    MEDIA_VIDEO_OUTPUT_H264,
    MEDIA_VIDEO_OUTPUT_H265,
} media_video_output;

typedef enum {
    MEDIA_VIDEO_ROTATION_0 = 0,
    MEDIA_VIDEO_ROTATION_90 = 90,
    MEDIA_VIDEO_ROTATION_180 = 180,
    MEDIA_VIDEO_ROTATION_270 = 270,
} media_video_rotation;

typedef enum {
    MEDIA_VIDEO_SENSOR_GC4023 = 0,
    MEDIA_VIDEO_SENSOR_TYPE_COUNT,
} media_video_sensor_type;

typedef struct {
    /* I2C controller index. */
    uint32_t bus_id;
    /* Vendor SDK 8-bit address; zero keeps the sensor driver's default. */
    uint8_t address;
} media_video_i2c_config;

typedef struct {
    uint32_t frame_rate;
    media_video_rotation rotation;
    /* Physical camera connector (0 or 1). */
    uint32_t device_id;
    media_video_i2c_config i2c;
    /* Sensor type installed on this input. */
    media_video_sensor_type sensor_type;
} media_video_camera_config;

typedef struct {
    /* Dense camera index in [0, camera_count). */
    uint32_t camera_id;
    media_video_output output;
    uint32_t width;
    uint32_t height;
    uint32_t frame_rate;
    uint32_t bit_rate_kbps;
    uint32_t gop;
} media_video_stream_config;

typedef struct {
    bool enabled;
    /* Dense logical inputs; each entry selects a physical connector. */
    size_t camera_count;
    media_video_camera_config cameras[MEDIA_VIDEO_MAX_CAMERAS];
    /* Dense application-defined outputs with IDs in [0, stream_count). */
    size_t stream_count;
    media_video_stream_config streams[MEDIA_VIDEO_MAX_STREAMS];
} media_video_config;

typedef enum {
    MEDIA_AUDIO_INPUT_IN0 = 0,
    MEDIA_AUDIO_INPUT_IN1,
    MEDIA_AUDIO_INPUT_DIFFERENTIAL,
} media_audio_input;

typedef struct {
    bool enabled;
    media_audio_input input;
    uint32_t physical_channel;
    int gain_db;
    uint32_t sample_rate;
    uint32_t frame_samples;
    bool enable_aec;
    /* TalkV2 supports stereo capture and is required for ADCR selection. */
    bool use_talk_v2;
} media_audio_input_config;

typedef struct {
    bool enabled;
    int gain_db;
    uint32_t sample_rate;
    uint32_t frame_samples;
    size_t max_buffer_size;
} media_audio_output_config;

typedef struct {
    media_video_config video;
    media_audio_input_config audio_input;
    media_audio_output_config audio_output;
} media_config;

typedef struct {
    media_pixel_format format;
    uint32_t width;
    uint32_t height;
    uint32_t stride[2];
    const uint8_t *planes[2];
    uint64_t pts;
} media_video_frame_info;

typedef struct {
    uint32_t exposure_time_us;
    uint32_t analog_gain;
    uint32_t digital_gain;
    uint32_t isp_digital_gain;
    uint32_t iso;
    uint8_t average_luminance;
    bool exposure_is_max;
    int16_t histogram_error;
    uint32_t first_stable_frame;
} media_video_exposure_info;

typedef struct {
    uint8_t *data;
    size_t size;
    uint64_t pts;
    uint32_t sequence;
    bool key_frame;
    media_video_output format;
} media_video_packet;

typedef struct {
    float x1;
    float y1;
    float x2;
    float y2;
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    uint8_t thickness;
} media_video_box;

/*
 * A straight overlay segment in normalised frame coordinates.  Both boxes
 * and lines write luma and chroma, so the requested colour survives into
 * the encoded stream.
 *
 * A segment that ends where it starts is a dot, drawn as a filled circle of
 * the given thickness.  Thickness is in pixels of the encoded frame while the
 * endpoints are fractions of it, so a caller that wants the same look at
 * several resolutions scales the thickness itself.
 */
typedef struct {
    float x1;
    float y1;
    float x2;
    float y2;
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    uint8_t thickness;
} media_video_line;

typedef enum {
    MEDIA_AUDIO_CODEC_AAC = 0,
    MEDIA_AUDIO_CODEC_MP3,
    MEDIA_AUDIO_CODEC_OPUS,
} media_audio_codec;

typedef enum {
    MEDIA_AUDIO_FORMAT_PCM_S16LE = 0,
    MEDIA_AUDIO_FORMAT_WAV,
    MEDIA_AUDIO_FORMAT_MP3,
    MEDIA_AUDIO_FORMAT_AAC_ADTS,
    /* Big-endian uint32 packet length followed by one RFC 7587 Opus packet. */
    MEDIA_AUDIO_FORMAT_OPUS,
    MEDIA_AUDIO_FORMAT_OGG_OPUS,
} media_audio_format;

typedef struct {
    media_audio_codec codec;
    uint32_t sample_rate;
    uint32_t channels;
    uint32_t bit_rate;
    uint32_t frame_samples;
} media_audio_encoder_config;

typedef struct {
    media_audio_codec codec;
    uint32_t sample_rate;
    uint32_t channels;
    size_t buffer_size;
} media_audio_decoder_config;

typedef enum {
    MEDIA_AUDIO_PLAY_IDLE = 0,
    MEDIA_AUDIO_PLAY_PLAYING,
    MEDIA_AUDIO_PLAY_PAUSED_BY_PROMPT,
    MEDIA_AUDIO_PLAY_ERROR,
} media_audio_play_state;

typedef enum {
    MEDIA_PROMPT_QUEUE = 0,
    MEDIA_PROMPT_REPLACE,
} media_prompt_policy;

/* The top-level initializer disables every subsystem. Each subsystem initializer enables itself. */
void media_config_init(media_config *config);
void media_video_config_init(media_video_config *config);
void media_audio_input_config_init(media_audio_input_config *config);
void media_audio_output_config_init(media_audio_output_config *config);
void media_audio_encoder_config_init(media_audio_encoder_config *config);
void media_audio_decoder_config_init(media_audio_decoder_config *config);

/* Initializes shared CV610 MAPI/MPP resources without starting a subsystem. */
media_status media_engine_open(const media_config *config, media_context **context);
/*
 * Stops remaining subsystems and releases shared resources. Do not call this
 * concurrently with another API; first release video frames and close audio
 * encoders and decoders.
 */
void media_engine_close(media_context *context);

media_status media_video_open(media_context *context);
media_status media_video_close(media_context *context);
media_status media_audio_output_open(media_context *context);
media_status media_audio_output_close(media_context *context);
media_status media_audio_input_open(media_context *context);
media_status media_audio_input_close(media_context *context);
const char *media_status_string(media_status status);

media_status media_video_acquire_frame(media_context *context, uint32_t stream_id,
    int timeout_ms, media_video_frame **frame);
void media_video_release_frame(media_context *context, media_video_frame *frame);
media_status media_video_frame_get_info(const media_video_frame *frame, media_video_frame_info *info);
size_t media_video_frame_packed_size(const media_video_frame *frame);
media_status media_video_frame_copy(const media_video_frame *frame, void *buffer, size_t buffer_size);
media_status media_video_set_ae_compensation(media_context *context, uint32_t camera_id,
    uint8_t compensation);
media_status media_video_get_exposure_info(media_context *context, uint32_t camera_id,
    media_video_exposure_info *info);

media_status media_video_read_packet(media_context *context, uint32_t stream_id,
    int timeout_ms, media_video_packet *packet);
void media_video_packet_release(media_video_packet *packet);
media_status media_video_request_key_frame(media_context *context, uint32_t stream_id);
media_status media_video_set_boxes(media_context *context, uint32_t stream_id,
    const media_video_box *boxes, size_t box_count);
/*
 * Replaces the stream's overlay segments.  They are drawn after the boxes and
 * persist until the next call; pass line_count 0 to clear them.
 */
media_status media_video_set_lines(media_context *context, uint32_t stream_id,
    const media_video_line *lines, size_t line_count);

/*
 * A short caption drawn over the encoded picture.  Unlike boxes and lines,
 * which are rasterised into the frame by the CPU, labels are handed to the
 * hardware region engine, so they cost nothing per frame and are not limited
 * by the line budget.  text is copied; anything past the capacity is dropped.
 */
typedef struct {
    /* Top left corner in normalised frame coordinates. */
    float x;
    float y;
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    /*
     * 1 draws the font at its own 5x7, and each step up repeats every
     * lit pixel into a square block of that side.  The region canvas is
     * sized for the largest scale, so raising the cap costs region
     * memory whether or not a caption uses it.
     */
    uint8_t scale;
    char text[MEDIA_VIDEO_LABEL_CAPACITY];
} media_video_label;

/*
 * Replaces the stream's captions.  They persist until the next call; pass
 * label_count 0 to clear them.  Redrawing only happens when the text or its
 * placement actually changes, so calling this every frame is cheap.
 */
media_status media_video_set_labels(media_context *context, uint32_t stream_id,
    const media_video_label *labels, size_t label_count);
media_status media_video_save_jpeg(media_context *context, uint32_t stream_id,
    const char *path, int timeout_ms);

media_status media_audio_input_read(media_context *context, int16_t *samples,
    size_t capacity, int timeout_ms, size_t *sample_count, uint64_t *pts);

/*
 * The audio input must be open before opening an encoder. The encoder sample
 * rate and frame_samples must match the audio input configuration.
 */
media_status media_audio_encoder_open(media_context *context,
    const media_audio_encoder_config *config, media_audio_encoder **encoder);
media_status media_audio_encoder_read_packet(media_audio_encoder *encoder,
    void *packet, size_t capacity, int timeout_ms, size_t *packet_size,
    uint64_t *pts, uint32_t *sequence);
/* Do not call close concurrently with read_packet on the same encoder. */
void media_audio_encoder_close(media_audio_encoder *encoder);

media_status media_audio_play_file(media_context *context, media_audio_format format, const char *path);
media_status media_audio_play_memory(media_context *context, media_audio_format format,
    const void *data, size_t size);
media_status media_audio_play_stop(media_context *context);
media_audio_play_state media_audio_play_get_state(media_context *context);
/* Writes 16-bit mono PCM samples to the main audio output. */
media_status media_audio_output_write(media_context *context, const int16_t *samples,
    size_t sample_count, int timeout_ms);
/* Marks PCM writes complete; queued samples continue playing. */
media_status media_audio_output_finish(media_context *context);

media_status media_audio_decoder_open(media_context *context,
    const media_audio_decoder_config *config, media_audio_decoder **decoder);
media_status media_audio_decoder_write(media_audio_decoder *decoder,
    const void *data, size_t size, int timeout_ms);
/* Writes one bare RFC 7587 packet to an Opus decoder. */
media_status media_audio_decoder_write_packet(media_audio_decoder *decoder,
    const void *packet, size_t size, int timeout_ms);
media_status media_audio_decoder_finish(media_audio_decoder *decoder);
/* Do not call close concurrently with write or finish on the same decoder. */
void media_audio_decoder_close(media_audio_decoder *decoder);

media_status media_audio_prompt_load(media_context *context, media_audio_format format,
    const void *data, size_t size, media_audio_prompt **prompt);
/* REPLACE interrupts the active prompt and clears queued AO frames before playback. */
media_status media_audio_prompt_play(media_context *context, const media_audio_prompt *prompt,
    media_prompt_policy policy);
void media_audio_prompt_destroy(media_audio_prompt *prompt);

#ifdef __cplusplus
}
#endif
#endif
