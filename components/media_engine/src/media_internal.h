#ifndef MEDIA_ENGINE_INTERNAL_H
#define MEDIA_ENGINE_INTERNAL_H

#include "media_engine/media.h"
#include "media_ogg.h"

#include "hi_mapi_acap.h"
#include "hi_mapi_aenc.h"
#include "hi_mapi_aenc_adpt.h"
#include "hi_mapi_adec.h"
#include "hi_mapi_ao.h"
#include "hi_mapi_sys.h"
#include "hi_mapi_vcap.h"
#include "hi_mapi_venc.h"
#include "hi_mapi_vpss.h"
#include "mapi_vpss_inner.h"

#include <pthread.h>
#include <stdatomic.h>

#define MEDIA_SENSOR_WIDTH 2560U
#define MEDIA_SENSOR_HEIGHT 1440U
#define MEDIA_CAMERA0_I2C_ADDRESS 0x52U
#define MEDIA_CAMERA1_I2C_ADDRESS 0x42U
#define MEDIA_VCAP_CHANNEL 0
#define MEDIA_AUDIO_DEVICE 0
#define MEDIA_AUDIO_CHANNEL 0
#define MEDIA_AUDIO_ENCODER 0
#define MEDIA_AUDIO_MAIN_DECODER 0
#define MEDIA_AUDIO_PROMPT_DECODER 1
#define MEDIA_PACKET_QUEUE_DEPTH 8U
/* Captions take a private block of the 128 global region handles. */
#define MEDIA_LABEL_HANDLE_BASE 96U
#define MEDIA_LABEL_CANVAS_NUM 2U
#define MEDIA_LABEL_FG_ALPHA 255U
#define MEDIA_CAPTURE_QUEUE_DEPTH 16U
#define MEDIA_AUDIO_ENCODE_QUEUE_DEPTH 32U
#define MEDIA_PROMPT_QUEUE_DEPTH 4U
#define MEDIA_AUDIO_MAX_FRAME_SAMPLES 2048U

typedef struct {
    media_video_packet packets[MEDIA_PACKET_QUEUE_DEPTH];
    size_t read_index;
    size_t write_index;
    size_t count;
    bool stopped;
    pthread_mutex_t lock;
    pthread_cond_t ready;
} media_packet_queue;

typedef struct {
    media_video_stream_config config;
    uint32_t id;
    td_handle vpss_group;
    td_handle vpss_channel;
    td_handle venc_channel;
    bool vpss_started;
    bool venc_initialized;
    bool venc_started;
    bool callback_registered;
    bool sync_initialized;
    bool worker_started;
    atomic_bool worker_running;
    /* Set while media_video_save_jpeg() needs the stream; the worker idles. */
    atomic_bool worker_paused;
    bool frame_outstanding;
    pthread_t worker;
    pthread_mutex_t frame_lock;
    pthread_mutex_t overlay_lock;
    media_video_box boxes[MEDIA_VIDEO_MAX_BOXES];
    size_t box_count;
    media_video_line lines[MEDIA_VIDEO_MAX_LINES];
    size_t line_count;
    /*
     * Captions live in hardware regions rather than the frame, so the last
     * applied set is kept to tell which regions actually need redrawing.
     */
    media_video_label labels[MEDIA_VIDEO_MAX_LABELS];
    bool label_used[MEDIA_VIDEO_MAX_LABELS];
    size_t label_count;
    bool labels_created;
    media_packet_queue packet_queue;
    hi_mapi_venc_call_back callback;
    struct media_context *context;
} media_video_stream_state;

typedef struct {
    td_handle device;
    td_handle pipe;
    td_handle vpss_group;
    bool sensor_initialized;
    bool device_started;
    bool channel_started;
    bool isp_initialized;
    bool pipe_started;
    bool isp_started;
    bool vpss_initialized;
    bool bound;
} media_video_camera_state;

typedef struct {
    bool initialized;
    bool jpeg_lock_initialized;
    pthread_mutex_t jpeg_lock;
    size_t camera_count;
    media_video_camera_state cameras[MEDIA_VIDEO_MAX_CAMERAS];
    size_t stream_count;
    media_video_stream_state streams[MEDIA_VIDEO_MAX_STREAMS];
} media_video_state;

typedef struct {
    int16_t samples[MEDIA_AUDIO_MAX_FRAME_SAMPLES];
    size_t sample_count;
    uint64_t pts;
} media_capture_block;

typedef struct {
    uint8_t data[MEDIA_OPUS_MAX_PACKET_SIZE];
    size_t size;
    uint64_t pts;
    uint32_t sequence;
} media_encoded_block;

struct media_audio_encoder {
    media_context *context;
    media_audio_encoder_config config;
    td_handle handle;
    pthread_cond_t ready;
    media_encoded_block blocks[MEDIA_AUDIO_ENCODE_QUEUE_DEPTH];
    size_t read_index;
    size_t write_index;
    size_t count;
    uint32_t next_sequence;
    pthread_t worker;
    bool worker_started;
    bool worker_running;
    bool initialized;
    bool started;
    bool closed;
};

typedef struct {
    bool initialized;
    bool device_initialized;
    bool started;
    bool vqe_enabled;
    bool worker_started;
    bool worker_running;
    pthread_t worker;
    pthread_mutex_t lock;
    pthread_cond_t ready;
    media_capture_block blocks[MEDIA_CAPTURE_QUEUE_DEPTH];
    size_t read_index;
    size_t write_index;
    size_t count;
    media_audio_encoder *encoder;
} media_capture_state;

typedef enum {
    MEDIA_SOURCE_NONE = 0,
    MEDIA_SOURCE_MEMORY,
    MEDIA_SOURCE_PLAYER,
    MEDIA_SOURCE_DECODER,
} media_source_kind;

typedef struct {
    media_source_kind kind;
    media_audio_format format;
    uint8_t *data;
    size_t size;
    size_t offset;
    bool finished;
    bool eos_sent;
    unsigned int idle_count;
    td_handle decoder;
    bool decoder_initialized;
    void *opus_decoder;
    /* Preserve decoded samples across Opus packets until one AO frame is full. */
    uint8_t pcm_pending[MEDIA_AUDIO_MAX_FRAME_SAMPLES * sizeof(int16_t)];
    size_t pcm_pending_size;
} media_audio_source;

struct media_audio_decoder {
    media_context *context;
    media_audio_decoder_config config;
    uint8_t *buffer;
    size_t capacity;
    size_t read_index;
    size_t write_index;
    size_t count;
    bool finished;
    bool closed;
    uint64_t generation;
};

struct media_audio_prompt {
    media_audio_format format;
    uint8_t *data;
    size_t size;
};

typedef struct {
    media_audio_format format;
    uint8_t *data;
    size_t size;
} media_prompt_entry;

typedef struct {
    uint8_t *buffer;
    size_t capacity;
    size_t read_index;
    size_t write_index;
    size_t count;
    bool finished;
} media_player_input;

typedef struct {
    bool initialized;
    bool ao_initialized;
    bool ao_started;
    bool worker_started;
    bool worker_running;
    pthread_t worker;
    pthread_mutex_t lock;
    pthread_cond_t changed;
    pthread_cond_t space;
    media_audio_play_state state;
    media_audio_source main_source;
    media_audio_source prompt_source;
    media_player_input input;
    media_audio_decoder *decoder;
    uint64_t decoder_generation;
    media_prompt_entry prompt_queue[MEDIA_PROMPT_QUEUE_DEPTH];
    size_t prompt_read_index;
    size_t prompt_write_index;
    size_t prompt_count;
    uint8_t pcm_staging[MEDIA_AUDIO_MAX_FRAME_SAMPLES * sizeof(int16_t)];
} media_player_state;

struct media_context {
    media_config config;
    bool sys_initialized;
    bool media_initialized;
    bool closing;
    media_video_state video;
    media_capture_state capture;
    media_player_state player;
};

struct media_video_frame {
    media_context *context;
    uint32_t stream_id;
    hi_mapi_frame_data data;
};

media_status media_video_prepare_system(const media_config *config, hi_mapi_sys_base_attr *base);
media_status media_video_start(media_context *context);
void media_video_stop(media_context *context);

media_status media_player_start(media_context *context);
void media_player_stop(media_context *context);
media_status media_capture_start(media_context *context);
void media_capture_stop(media_context *context);

bool media_make_deadline(int timeout_ms, struct timespec *deadline);
media_status media_copy_file(const char *path, uint8_t **data, size_t *size);
#endif
