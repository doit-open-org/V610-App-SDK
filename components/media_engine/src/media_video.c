#include "media_internal.h"
#include "media_label.h"
#include "media_overlay.h"

#include "gc4023_cmos.h"
#include "securec.h"
#include "ss_mpi_ae.h"
#include "ss_mpi_region.h"

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MEDIA_JPEG_ENCODER 7
#define MEDIA_JPEG_QUALITY 90U
#define MEDIA_ENCODER_TIMEOUT_MS 100
#define MEDIA_AUDIO_ONLY_VB_DIMENSION 64U

typedef struct {
    pthread_mutex_t lock;
    pthread_cond_t ready;
    bool waiting;
    media_status status;
    uint8_t *data;
    size_t size;
} media_jpeg_waiter;

typedef struct {
    media_video_sensor_type type;
    ot_isp_sns_obj *(*get_object)(td_void);
} media_sensor_mapping;

static const media_sensor_mapping g_media_sensor_mappings[] = {
    {
        .type = MEDIA_VIDEO_SENSOR_GC4023,
        .get_object = gc4023_get_obj,
    },
};

static void media_make_channel(hi_mapi_mpp_chn *channel,
    hi_mapi_module_id module, td_s32 device, td_s32 id)
{
    channel->mod_id = module;
    channel->dev_id = device;
    channel->chn_id = id;
}

static media_status media_bind_vcap(td_handle pipe, td_handle vpss_group)
{
    hi_mapi_mpp_chn source = {0};
    hi_mapi_mpp_chn destination = {0};
    media_make_channel(&source, HI_MAPI_MOD_VCAP, pipe, MEDIA_VCAP_CHANNEL);
    media_make_channel(&destination, HI_MAPI_MOD_VPSS, vpss_group, 0);
    return hi_mapi_sys_bind(&source, &destination) == TD_SUCCESS ? MEDIA_OK : MEDIA_ERR_DEVICE;
}

static void media_unbind_vcap(td_handle pipe, td_handle vpss_group)
{
    hi_mapi_mpp_chn source = {0};
    hi_mapi_mpp_chn destination = {0};
    media_make_channel(&source, HI_MAPI_MOD_VCAP, pipe, MEDIA_VCAP_CHANNEL);
    media_make_channel(&destination, HI_MAPI_MOD_VPSS, vpss_group, 0);
    (void)hi_mapi_sys_unbind(&source, &destination);
}

static const media_sensor_mapping *media_find_sensor_mapping(media_video_sensor_type type)
{
    for (size_t i = 0; i < sizeof(g_media_sensor_mappings) /
        sizeof(g_media_sensor_mappings[0]); i++) {
        if (g_media_sensor_mappings[i].type == type) {
            return &g_media_sensor_mappings[i];
        }
    }
    return NULL;
}

static media_status media_configure_sensor(hi_mapi_vcap_sensor_attr *sensor,
    const media_video_camera_config *camera)
{
    const media_sensor_mapping *mapping = media_find_sensor_mapping(camera->sensor_type);
    if (mapping == NULL || mapping->get_object == NULL) {
        return MEDIA_ERR_UNSUPPORTED;
    }

    uint32_t device_id = camera->device_id;
    sensor->sns_mode = 0;
    sensor->wdr_mode = HI_MAPI_VCAP_WDR_MODE_NONE;
    sensor->size.width = MEDIA_SENSOR_WIDTH;
    sensor->size.height = MEDIA_SENSOR_HEIGHT;
    sensor->input_dev_attr.input_mode = HI_MAPI_VCAP_INPUT_MODE_MIPI;
    sensor->input_dev_attr.input_data_type = HI_MAPI_VCAP_INPUT_DATA_TYPE_RAW_12BIT;
    sensor->sns_dev_attr.sensor_obj = mapping->get_object();
    if (sensor->sns_dev_attr.sensor_obj == NULL) {
        return MEDIA_ERR_DEVICE;
    }
    sensor->sns_dev_attr.data_rate = HI_MAPI_VCAP_MIPI_DATA_RATE_X1;
    sensor->sns_dev_attr.input_attr.bayer_format = HI_MAPI_VCAP_BAYER_FORMAT_RGGB;
    sensor->sns_dev_attr.input_attr.comm_bus_type = HI_MAPI_SENSOR_COMMBUS_TYPE_I2C;
    if (camera->i2c.address != 0U) {
        sensor->sns_dev_attr.input_attr.sensor_dev_addr = camera->i2c.address;
    }
    for (size_t i = 0; i < sizeof(sensor->sns_dev_attr.lane_id) /
        sizeof(sensor->sns_dev_attr.lane_id[0]); i++) {
        sensor->sns_dev_attr.lane_id[i] = -1;
    }
    sensor->sns_dev_attr.lane_id[0] = device_id == 0 ? 0 : 1;
    sensor->sns_dev_attr.lane_id[1] = device_id == 0 ? 2 : 3;
    sensor->combo_dev_attr.lane_id_mode = HI_MAPI_VCAP_LANE_DIVIDE_MODE_1;
    sensor->combo_dev_attr.sns_clk_source = device_id;
    sensor->combo_dev_attr.sns_rst_source = device_id;
    sensor->combo_dev_attr.bus_id = (td_s32)camera->i2c.bus_id;
    return MEDIA_OK;
}

static void media_configure_vcap(const media_context *context, uint32_t camera_id,
    td_handle pipe_id, hi_mapi_vcap_attr *attributes)
{
    uint32_t frame_rate = context->config.video.cameras[camera_id].frame_rate;
    attributes->vcap_dev_attr.wdr_mode = HI_MAPI_VCAP_WDR_MODE_NONE;
    attributes->vcap_dev_attr.intf_mode = HI_MAPI_VCAP_MODE_MIPI;
    attributes->vcap_dev_attr.data_seq = HI_MAPI_DATA_SEQ_YUYV;
    attributes->vcap_dev_attr.data_type = HI_MAPI_VCAP_DATA_TYPE_RGB;
    attributes->pipe_bind_num = 1;
    attributes->pipe_id_bind[0] = pipe_id;
    hi_mapi_vcap_pipe_attr *pipe = &attributes->vcap_pipe_attr[pipe_id];
    pipe->pipe_type = HI_MAPI_VCAP_PIPE_TYPE_VIDEO;
    pipe->frame_rate_ctrl.src_frame_rate = -1;
    pipe->frame_rate_ctrl.dst_frame_rate = -1;
    pipe->isp_pub_attr.frame_rate = (td_float)frame_rate;
    pipe->isp_pub_attr.size.width = MEDIA_SENSOR_WIDTH;
    pipe->isp_pub_attr.size.height = MEDIA_SENSOR_HEIGHT;
    pipe->isp_pub_attr.bayer = HI_MAPI_VCAP_BAYER_RGGB;
    pipe->isp_pub_attr.sns_mirror_flip = HI_MAPI_VCAP_ISP_SNS_NORMAL;
    hi_mapi_vcap_pipe_chn_attr *channel = &pipe->pipe_chn_attr[MEDIA_VCAP_CHANNEL];
    channel->dest_size.width = MEDIA_SENSOR_WIDTH;
    channel->dest_size.height = MEDIA_SENSOR_HEIGHT;
    channel->frame_rate_ctrl.src_frame_rate = (td_s32)frame_rate;
    channel->frame_rate_ctrl.dst_frame_rate = (td_s32)frame_rate;
    channel->pixel_format = HI_MAPI_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    channel->video_format = HI_MAPI_VIDEO_FORMAT_LINEAR;
}

static media_status media_configure_rotation(const media_context *context, uint32_t camera_id)
{
    media_video_rotation rotation = context->config.video.cameras[camera_id].rotation;
    td_handle pipe = context->video.cameras[camera_id].pipe;
    hi_mapi_vcap_mirror_flip_attr mirror_flip = {
        .mirror = TD_TRUE,
        .flip = TD_TRUE,
    };
    hi_mapi_rotation mapi_rotation;

    switch (rotation) {
        case MEDIA_VIDEO_ROTATION_0:
            return MEDIA_OK;
        case MEDIA_VIDEO_ROTATION_90:
            mapi_rotation = HI_MAPI_ROTATION_90;
            break;
        case MEDIA_VIDEO_ROTATION_180:
            if (hi_mapi_vcap_set_param(pipe, MEDIA_VCAP_CHANNEL,
                HI_MAPI_VCAP_CMD_MIRRORFLIP, &mirror_flip, sizeof(mirror_flip)) != TD_SUCCESS) {
                return MEDIA_ERR_DEVICE;
            }
            return MEDIA_OK;
        case MEDIA_VIDEO_ROTATION_270:
            mapi_rotation = HI_MAPI_ROTATION_270;
            break;
        default:
            return MEDIA_ERR_ARGUMENT;
    }
    if (hi_mapi_vcap_set_param(pipe, MEDIA_VCAP_CHANNEL,
        HI_MAPI_VCAP_CMD_ROTATE, &mapi_rotation, sizeof(mapi_rotation)) != TD_SUCCESS) {
        return MEDIA_ERR_DEVICE;
    }
    return MEDIA_OK;
}

media_status media_video_prepare_system(const media_config *config, hi_mapi_sys_base_attr *base)
{
    if (config == NULL || base == NULL) {
        return MEDIA_ERR_ARGUMENT;
    }
    *base = (hi_mapi_sys_base_attr) {0};
    if (!config->video.enabled) {
        /* H264-enabled MAPI still initializes VENC and therefore needs MPP SYS/VB. */
        base->vpss_grp_num = 1;
        base->vpss_chn_num = 1;
        base->vpss_reso_size[0][0].width = MEDIA_AUDIO_ONLY_VB_DIMENSION;
        base->vpss_reso_size[0][0].height = MEDIA_AUDIO_ONLY_VB_DIMENSION;
        return MEDIA_OK;
    }
    base->vcap_dev_num = (td_u32)config->video.camera_count;
    base->raw_bit_width = 12;
    base->vpss_grp_num = (td_u32)config->video.camera_count;
    size_t channels[MEDIA_VIDEO_MAX_CAMERAS] = {0};
    size_t max_channels = 0;
    for (size_t i = 0; i < config->video.camera_count; i++) {
        base->vcap_reso_size[i].width = MEDIA_SENSOR_WIDTH;
        base->vcap_reso_size[i].height = MEDIA_SENSOR_HEIGHT;
        base->vcap_online[i] = TD_FALSE;
        base->vpss_online[i] = TD_FALSE;
        base->frame_rate[i] = config->video.cameras[i].frame_rate;
    }
    for (size_t i = 0; i < config->video.stream_count; i++) {
        const media_video_stream_config *stream = &config->video.streams[i];
        size_t channel = channels[stream->camera_id]++;
        base->vpss_reso_size[stream->camera_id][channel].width = stream->width;
        base->vpss_reso_size[stream->camera_id][channel].height = stream->height;
        if (channels[stream->camera_id] > max_channels) {
            max_channels = channels[stream->camera_id];
        }
    }
    base->vpss_chn_num = (td_u32)max_channels;
    for (size_t camera = 0; camera < config->video.camera_count; camera++) {
        for (size_t channel = channels[camera]; channel < max_channels; channel++) {
            base->vpss_reso_size[camera][channel].width = MEDIA_AUDIO_ONLY_VB_DIMENSION;
            base->vpss_reso_size[camera][channel].height = MEDIA_AUDIO_ONLY_VB_DIMENSION;
        }
    }
    return MEDIA_OK;
}

static media_status media_packet_queue_init(media_packet_queue *queue)
{
    if (pthread_mutex_init(&queue->lock, NULL) != 0) {
        return MEDIA_ERR_DEVICE;
    }
    if (pthread_cond_init(&queue->ready, NULL) != 0) {
        (void)pthread_mutex_destroy(&queue->lock);
        return MEDIA_ERR_DEVICE;
    }
    queue->read_index = 0;
    queue->write_index = 0;
    queue->count = 0;
    queue->stopped = false;
    return MEDIA_OK;
}

static void media_packet_queue_clear_locked(media_packet_queue *queue)
{
    while (queue->count > 0) {
        media_video_packet_release(&queue->packets[queue->read_index]);
        queue->read_index = (queue->read_index + 1) % MEDIA_PACKET_QUEUE_DEPTH;
        queue->count--;
    }
    queue->read_index = 0;
    queue->write_index = 0;
}

static void media_packet_queue_destroy(media_packet_queue *queue)
{
    (void)pthread_mutex_lock(&queue->lock);
    media_packet_queue_clear_locked(queue);
    (void)pthread_mutex_unlock(&queue->lock);
    (void)pthread_cond_destroy(&queue->ready);
    (void)pthread_mutex_destroy(&queue->lock);
}

static size_t media_encoded_pack_size(const hi_mapi_venc_data_pack_attr *pack)
{
    size_t size = 0;
    for (td_u32 i = 0; i < HI_MAPI_VENC_PACK_NUM; i++) {
        td_u32 offset = i == 0 ? pack->offset : 0;
        if (pack->vir_addr[i] != NULL && offset < pack->len[i]) {
            size_t part = (size_t)pack->len[i] - offset;
            if (part > SIZE_MAX - size) {
                return 0;
            }
            size += part;
        }
    }
    return size;
}

static bool media_packet_is_key_frame(media_video_output output,
    const hi_mapi_venc_data_pack_attr *pack)
{
    if (output == MEDIA_VIDEO_OUTPUT_H264) {
        hi_mapi_venc_h264e_nalu_type type = pack->data_type.h264e_type;
        return type == HI_MAPI_VENC_H264E_NALU_IDRSLICE ||
            type == HI_MAPI_VENC_H264E_NALU_ISLICE ||
            type == HI_MAPI_VENC_H264E_NALU_SPS;
    }
    hi_mapi_venc_h265e_nalu_type type = pack->data_type.h265e_type;
    return type == HI_MAPI_VENC_H265E_NALU_IDRSLICE ||
        type == HI_MAPI_VENC_H265E_NALU_ISLICE ||
        type == HI_MAPI_VENC_H265E_NALU_SPS;
}

static media_status media_copy_encoded_data(const hi_mapi_venc_data_attr *encoded,
    media_video_output format, media_video_packet *packet)
{
    size_t total = 0;
    bool key_frame = false;
    uint64_t pts = 0;
    for (td_u32 i = 0; i < encoded->pack_count; i++) {
        size_t pack_size = media_encoded_pack_size(&encoded->data_pack[i]);
        if (pack_size == 0 || pack_size > SIZE_MAX - total) {
            return MEDIA_ERR_FORMAT;
        }
        total += pack_size;
        pts = encoded->data_pack[i].time_stamp;
        key_frame = key_frame || media_packet_is_key_frame(format, &encoded->data_pack[i]);
    }
    if (total == 0) {
        return MEDIA_ERR_FORMAT;
    }
    uint8_t *data = malloc(total);
    if (data == NULL) {
        return MEDIA_ERR_MEMORY;
    }
    size_t written = 0;
    for (td_u32 i = 0; i < encoded->pack_count; i++) {
        const hi_mapi_venc_data_pack_attr *pack = &encoded->data_pack[i];
        for (td_u32 j = 0; j < HI_MAPI_VENC_PACK_NUM; j++) {
            td_u32 offset = j == 0 ? pack->offset : 0;
            if (pack->vir_addr[j] == NULL || offset >= pack->len[j]) {
                continue;
            }
            size_t part = (size_t)pack->len[j] - offset;
            if (memcpy_s(data + written, total - written,
                pack->vir_addr[j] + offset, part) != EOK) {
                free(data);
                return MEDIA_ERR_MEMORY;
            }
            written += part;
        }
    }
    *packet = (media_video_packet) {
        .data = data,
        .size = written,
        .pts = pts,
        .sequence = encoded->seq,
        .key_frame = key_frame,
        .format = format,
    };
    return MEDIA_OK;
}

static td_s32 media_encoded_callback(td_handle channel, hi_mapi_venc_data_attr *encoded,
    td_void *private_data)
{
    (void)channel;
    media_video_stream_state *stream = private_data;
    if (stream == NULL || encoded == NULL || !stream->sync_initialized) {
        return TD_FAILURE;
    }
    media_video_packet packet = {0};
    if (media_copy_encoded_data(encoded, stream->config.output, &packet) != MEDIA_OK) {
        return TD_FAILURE;
    }
    media_packet_queue *queue = &stream->packet_queue;
    (void)pthread_mutex_lock(&queue->lock);
    if (queue->stopped) {
        (void)pthread_mutex_unlock(&queue->lock);
        media_video_packet_release(&packet);
        return TD_SUCCESS;
    }
    if (queue->count == MEDIA_PACKET_QUEUE_DEPTH) {
        media_video_packet_release(&queue->packets[queue->read_index]);
        queue->read_index = (queue->read_index + 1) % MEDIA_PACKET_QUEUE_DEPTH;
        queue->count--;
    }
    queue->packets[queue->write_index] = packet;
    queue->write_index = (queue->write_index + 1) % MEDIA_PACKET_QUEUE_DEPTH;
    queue->count++;
    (void)pthread_cond_signal(&queue->ready);
    (void)pthread_mutex_unlock(&queue->lock);
    return TD_SUCCESS;
}

static uint8_t media_rgb_to_y(const media_video_box *box)
{
    unsigned int value = 77U * box->red + 150U * box->green + 29U * box->blue;
    return (uint8_t)(value >> 8);
}

static void media_draw_horizontal(const media_overlay_target *target,
    uint32_t y, uint32_t x1, uint32_t x2, uint32_t thickness,
    uint8_t value, uint8_t cb, uint8_t cr)
{
    for (uint32_t row = 0; row < thickness && y + row < UINT32_MAX; row++) {
        uint32_t current_y = y + row;
        for (uint32_t x = x1; x <= x2 && x < target->width; x++) {
            media_plot(target, (int32_t)x, (int32_t)current_y, value, cb, cr);
        }
    }
}

static void media_draw_vertical(const media_overlay_target *target,
    uint32_t x, uint32_t y1, uint32_t y2, uint32_t thickness,
    uint8_t value, uint8_t cb, uint8_t cr)
{
    for (uint32_t y = y1; y <= y2 && y < target->height; y++) {
        for (uint32_t column = 0; column < thickness && x + column < UINT32_MAX; column++) {
            media_plot(target, (int32_t)(x + column), (int32_t)y, value, cb, cr);
        }
    }
}

static bool media_overlay_target_init(media_overlay_target *target,
    hi_mapi_frame_data *frame)
{
    target->luma = (uint8_t *)(uintptr_t)frame->vir_addr[0];
    if (target->luma == NULL || frame->width == 0 || frame->height == 0) {
        return false;
    }
    target->width = frame->width;
    target->height = frame->height;
    target->luma_stride = frame->stride[0];
    target->chroma = NULL;
    target->chroma_stride = 0;
    target->cr_first = false;
    if (frame->pixel_format == HI_MAPI_PIXEL_FORMAT_YVU_SEMIPLANAR_420 ||
        frame->pixel_format == HI_MAPI_PIXEL_FORMAT_YUV_SEMIPLANAR_420) {
        target->chroma = (uint8_t *)(uintptr_t)frame->vir_addr[1];
        target->chroma_stride = frame->stride[1];
        target->cr_first = frame->pixel_format ==
            HI_MAPI_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
        if (target->chroma_stride == 0) {
            target->chroma = NULL;
        }
    }
    return true;
}

static void media_draw_lines(media_video_stream_state *stream,
    const media_overlay_target *target)
{
    for (size_t i = 0; i < stream->line_count; i++) {
        const media_video_line *line = &stream->lines[i];
        uint8_t cb = 128;
        uint8_t cr = 128;
        media_rgb_to_chroma(line->red, line->green, line->blue, &cb, &cr);
        const unsigned int luma_value =
            (77U * line->red + 150U * line->green + 29U * line->blue) >> 8;
        media_draw_line(target,
            (int32_t)(line->x1 * (float)(target->width - 1U)),
            (int32_t)(line->y1 * (float)(target->height - 1U)),
            (int32_t)(line->x2 * (float)(target->width - 1U)),
            (int32_t)(line->y2 * (float)(target->height - 1U)),
            line->thickness, (uint8_t)luma_value, cb, cr);
    }
}

static void media_draw_boxes(media_video_stream_state *stream, hi_mapi_frame_data *frame)
{
    media_overlay_target target;
    if (!media_overlay_target_init(&target, frame)) {
        return;
    }
    (void)pthread_mutex_lock(&stream->overlay_lock);
    for (size_t i = 0; i < stream->box_count; i++) {
        const media_video_box *box = &stream->boxes[i];
        uint32_t x1 = (uint32_t)(box->x1 * (float)(frame->width - 1));
        uint32_t y1 = (uint32_t)(box->y1 * (float)(frame->height - 1));
        uint32_t x2 = (uint32_t)(box->x2 * (float)(frame->width - 1));
        uint32_t y2 = (uint32_t)(box->y2 * (float)(frame->height - 1));
        uint32_t thickness = box->thickness == 0 ? 1 : box->thickness;
        if (thickness > x2 - x1 + 1) {
            thickness = x2 - x1 + 1;
        }
        if (thickness > y2 - y1 + 1) {
            thickness = y2 - y1 + 1;
        }
        uint8_t value = media_rgb_to_y(box);
        uint8_t cb = 128;
        uint8_t cr = 128;
        media_rgb_to_chroma(box->red, box->green, box->blue, &cb, &cr);
        media_draw_horizontal(&target, y1, x1, x2, thickness, value, cb, cr);
        media_draw_horizontal(&target, y2 - thickness + 1, x1, x2, thickness, value, cb, cr);
        media_draw_vertical(&target, x1, y1, y2, thickness, value, cb, cr);
        media_draw_vertical(&target, x2 - thickness + 1, y1, y2, thickness, value, cb, cr);
    }
    if (stream->line_count != 0) {
        media_draw_lines(stream, &target);
    }
    (void)pthread_mutex_unlock(&stream->overlay_lock);
}

static void *media_encoder_worker(void *argument)
{
    media_video_stream_state *stream = argument;
    const struct timespec retry = {
        .tv_sec = 0,
        .tv_nsec = 1000000L,
    };
    while (true) {
        if (!atomic_load_explicit(&stream->worker_running, memory_order_acquire)) {
            break;
        }
        if (atomic_load_explicit(&stream->worker_paused, memory_order_acquire)) {
            (void)nanosleep(&retry, NULL);
            continue;
        }
        (void)pthread_mutex_lock(&stream->frame_lock);
        if (!atomic_load_explicit(&stream->worker_running, memory_order_acquire)) {
            (void)pthread_mutex_unlock(&stream->frame_lock);
            break;
        }
        hi_mapi_frame_data frame = {0};
        if (mapi_vpss_get_chn_frame(stream->vpss_group, stream->vpss_channel,
            &frame, MEDIA_ENCODER_TIMEOUT_MS) != TD_SUCCESS) {
            (void)pthread_mutex_unlock(&stream->frame_lock);
            (void)nanosleep(&retry, NULL);
            continue;
        }
        media_draw_boxes(stream, &frame);
        (void)hi_mapi_venc_send_frame(stream->venc_channel, &frame, MEDIA_ENCODER_TIMEOUT_MS);
        (void)hi_mapi_vpss_release_chn_frame(stream->vpss_group, stream->vpss_channel, &frame);
        (void)pthread_mutex_unlock(&stream->frame_lock);
    }
    return NULL;
}

static media_status media_stream_sync_init(media_video_stream_state *stream)
{
    if (pthread_mutex_init(&stream->frame_lock, NULL) != 0) {
        return MEDIA_ERR_DEVICE;
    }
    if (pthread_mutex_init(&stream->overlay_lock, NULL) != 0) {
        (void)pthread_mutex_destroy(&stream->frame_lock);
        return MEDIA_ERR_DEVICE;
    }
    media_status status = media_packet_queue_init(&stream->packet_queue);
    if (status != MEDIA_OK) {
        (void)pthread_mutex_destroy(&stream->overlay_lock);
        (void)pthread_mutex_destroy(&stream->frame_lock);
        return status;
    }
    atomic_init(&stream->worker_running, false);
    atomic_init(&stream->worker_paused, false);
    stream->sync_initialized = true;
    return MEDIA_OK;
}

static void media_stream_sync_destroy(media_video_stream_state *stream)
{
    if (!stream->sync_initialized) {
        return;
    }
    media_packet_queue_destroy(&stream->packet_queue);
    (void)pthread_mutex_destroy(&stream->overlay_lock);
    (void)pthread_mutex_destroy(&stream->frame_lock);
    stream->sync_initialized = false;
}

static media_status media_start_vcap(media_context *context)
{
    media_video_state *video = &context->video;
    for (size_t i = 0; i < video->camera_count; i++) {
        const media_video_camera_config *config = &context->config.video.cameras[i];
        td_handle device = (td_handle)config->device_id;
        td_handle pipe = device;
        media_video_camera_state *state = &video->cameras[i];
        hi_mapi_vcap_sensor_attr sensor = {0};
        hi_mapi_vcap_attr attributes = {0};
        state->device = device;
        state->pipe = pipe;
        state->vpss_group = (td_handle)i;
        media_status status = media_configure_sensor(&sensor, config);
        if (status != MEDIA_OK) {
            return status;
        }
        media_configure_vcap(context, (uint32_t)i, pipe, &attributes);
        if (hi_mapi_vcap_init_sensor(device, &sensor, TD_TRUE) != TD_SUCCESS) {
            return MEDIA_ERR_DEVICE;
        }
        state->sensor_initialized = true;
        if (hi_mapi_vcap_set_attr(device, &attributes) != TD_SUCCESS ||
            hi_mapi_vcap_start_dev(device) != TD_SUCCESS) {
            return MEDIA_ERR_DEVICE;
        }
        state->device_started = true;
        if (hi_mapi_vcap_start_chn(pipe, MEDIA_VCAP_CHANNEL) != TD_SUCCESS) {
            return MEDIA_ERR_DEVICE;
        }
        state->channel_started = true;
        if (media_configure_rotation(context, (uint32_t)i) != MEDIA_OK) {
            return MEDIA_ERR_DEVICE;
        }
        if (hi_mapi_vcap_init_isp(pipe) != TD_SUCCESS) {
            return MEDIA_ERR_DEVICE;
        }
        state->isp_initialized = true;
        if (hi_mapi_vcap_start_pipe(pipe) != TD_SUCCESS) {
            return MEDIA_ERR_DEVICE;
        }
        state->pipe_started = true;
    }
    for (size_t i = 0; i < video->camera_count; i++) {
        if (hi_mapi_vcap_start_isp(video->cameras[i].pipe) != TD_SUCCESS) {
            return MEDIA_ERR_DEVICE;
        }
        video->cameras[i].isp_started = true;
    }
    return MEDIA_OK;
}

static media_status media_start_vpss(media_context *context)
{
    media_video_state *video = &context->video;
    hi_mapi_vpss_grp_attr group = {
        .max_width = MEDIA_SENSOR_WIDTH,
        .max_height = MEDIA_SENSOR_HEIGHT,
        .frame_rate_ctrl = {
            .src_frame_rate = -1,
            .dst_frame_rate = -1,
        },
        .pixel_format = HI_MAPI_PIXEL_FORMAT_YVU_SEMIPLANAR_420,
    };
    for (size_t i = 0; i < video->camera_count; i++) {
        media_video_camera_state *state = &video->cameras[i];
        if (hi_mapi_vpss_init(state->vpss_group, &group) != TD_SUCCESS) {
            return MEDIA_ERR_DEVICE;
        }
        state->vpss_initialized = true;
        media_status status = media_bind_vcap(state->pipe, state->vpss_group);
        if (status != MEDIA_OK) {
            return status;
        }
        state->bound = true;
    }
    for (size_t i = 0; i < video->stream_count; i++) {
        media_video_stream_state *stream = &video->streams[i];
        uint32_t camera_id = stream->config.camera_id;
        hi_mapi_vpss_chn_attr channel = {
            .width = stream->config.width,
            .height = stream->config.height,
            .frame_rate_ctrl = {
                .src_frame_rate = (td_s32)context->config.video.cameras[camera_id].frame_rate,
                .dst_frame_rate = (td_s32)stream->config.frame_rate,
            },
            .video_format = HI_MAPI_VIDEO_FORMAT_LINEAR,
            .pixel_format = HI_MAPI_PIXEL_FORMAT_YUV_SEMIPLANAR_420,
            .compress_mode = HI_MAPI_COMPRESS_MODE_NONE,
            .aspect_ratio = {
                .mode = HI_MAPI_ASPECT_RATIO_AUTO,
                .bg_color = 0,
            },
        };
        hi_mapi_vpss_depth_attr depth = {
            .depth = 2,
        };
        if (hi_mapi_vpss_set_chn_attr(stream->vpss_group,
            stream->vpss_channel, &channel) != TD_SUCCESS ||
            hi_mapi_vpss_start_chn(stream->vpss_group, stream->vpss_channel) != TD_SUCCESS) {
            return MEDIA_ERR_DEVICE;
        }
        stream->vpss_started = true;
        if (hi_mapi_vpss_set_param(stream->vpss_group, stream->vpss_channel,
            HI_MAPI_VPSS_CMD_CHN_DEPTH, &depth, sizeof(depth)) != TD_SUCCESS) {
            return MEDIA_ERR_DEVICE;
        }
    }
    return MEDIA_OK;
}

static hi_mapi_payload_type media_video_payload(media_video_output output)
{
    return output == MEDIA_VIDEO_OUTPUT_H265 ?
        HI_MAPI_PAYLOAD_TYPE_H265 : HI_MAPI_PAYLOAD_TYPE_H264;
}

static media_status media_start_encoder(media_video_stream_state *stream)
{
    hi_mapi_venc_base_attr base = {
        .type = media_video_payload(stream->config.output),
        .width = stream->config.width,
        .height = stream->config.height,
        .src_frame_rate = stream->config.frame_rate,
        .dst_frame_rate = stream->config.frame_rate,
        .bit_rate = stream->config.bit_rate_kbps,
    };
    hi_mapi_venc_attr attributes = {0};
    if (hi_mapi_venc_get_default_attr(&base, &attributes) != TD_SUCCESS) {
        return MEDIA_ERR_DEVICE;
    }
    attributes.rc_attr.cbr_attr.gop = stream->config.gop;
    if (hi_mapi_venc_init(stream->venc_channel, &attributes) != TD_SUCCESS) {
        return MEDIA_ERR_DEVICE;
    }
    stream->venc_initialized = true;
    stream->callback.proc_data_cb = media_encoded_callback;
    stream->callback.private_data = stream;
    if (hi_mapi_venc_register_callback(stream->venc_channel, &stream->callback) != TD_SUCCESS) {
        return MEDIA_ERR_DEVICE;
    }
    stream->callback_registered = true;
    if (hi_mapi_venc_start(stream->venc_channel,
        HI_MAPI_VENC_LIMITLESS_FRAME_COUNT) != TD_SUCCESS) {
        return MEDIA_ERR_DEVICE;
    }
    stream->venc_started = true;
    atomic_store_explicit(&stream->worker_running, true, memory_order_release);
    if (pthread_create(&stream->worker, NULL, media_encoder_worker, stream) != 0) {
        atomic_store_explicit(&stream->worker_running, false, memory_order_release);
        return MEDIA_ERR_DEVICE;
    }
    stream->worker_started = true;
    return MEDIA_OK;
}

media_status media_video_start(media_context *context)
{
    if (!context->config.video.enabled) {
        return MEDIA_OK;
    }
    media_video_state *video = &context->video;
    video->initialized = true;
    if (pthread_mutex_init(&video->jpeg_lock, NULL) != 0) {
        video->initialized = false;
        return MEDIA_ERR_DEVICE;
    }
    video->jpeg_lock_initialized = true;
    video->camera_count = context->config.video.camera_count;
    video->stream_count = context->config.video.stream_count;
    size_t channels[MEDIA_VIDEO_MAX_CAMERAS] = {0};
    for (size_t i = 0; i < video->stream_count; i++) {
        media_video_stream_state *stream = &video->streams[i];
        stream->config = context->config.video.streams[i];
        stream->id = (uint32_t)i;
        stream->vpss_group = (td_handle)stream->config.camera_id;
        stream->vpss_channel = (td_handle)channels[stream->config.camera_id]++;
        stream->venc_channel = (td_handle)i;
        stream->context = context;
        media_status status = media_stream_sync_init(stream);
        if (status != MEDIA_OK) {
            media_video_stop(context);
            return status;
        }
    }
    media_status status = media_start_vcap(context);
    if (status == MEDIA_OK) {
        status = media_start_vpss(context);
    }
    for (size_t i = 0; status == MEDIA_OK && i < video->stream_count; i++) {
        media_video_stream_state *stream = &video->streams[i];
        if (stream->config.output != MEDIA_VIDEO_OUTPUT_FRAME) {
            status = media_start_encoder(stream);
        }
    }
    if (status != MEDIA_OK) {
        media_video_stop(context);
    }
    return status;
}

void media_video_stop(media_context *context)
{
    media_video_state *video = &context->video;
    if (!video->initialized) {
        return;
    }
    for (size_t i = 0; i < video->stream_count; i++) {
        media_video_stream_state *stream = &video->streams[i];
        if (!stream->sync_initialized) {
            continue;
        }
        atomic_store_explicit(&stream->worker_running, false, memory_order_release);
    }
    for (size_t i = 0; i < video->stream_count; i++) {
        media_video_stream_state *stream = &video->streams[i];
        if (stream->worker_started) {
            (void)pthread_join(stream->worker, NULL);
            stream->worker_started = false;
        }
        /* Regions are detached before the channel they hang off goes away. */
        media_label_destroy_regions(stream);
        if (stream->venc_started) {
            (void)hi_mapi_venc_stop(stream->venc_channel);
            stream->venc_started = false;
        }
        if (stream->callback_registered) {
            (void)hi_mapi_venc_un_register_callback(stream->venc_channel, &stream->callback);
            stream->callback_registered = false;
        }
        if (stream->venc_initialized) {
            (void)hi_mapi_venc_deinit(stream->venc_channel);
            stream->venc_initialized = false;
        }
        if (stream->sync_initialized) {
            (void)pthread_mutex_lock(&stream->packet_queue.lock);
            stream->packet_queue.stopped = true;
            (void)pthread_cond_broadcast(&stream->packet_queue.ready);
            (void)pthread_mutex_unlock(&stream->packet_queue.lock);
        }
    }
    for (size_t i = video->stream_count; i > 0; i--) {
        media_video_stream_state *stream = &video->streams[i - 1];
        if (stream->vpss_started) {
            (void)hi_mapi_vpss_stop_chn(stream->vpss_group, stream->vpss_channel);
            stream->vpss_started = false;
        }
    }
    for (size_t i = video->camera_count; i > 0; i--) {
        media_video_camera_state *state = &video->cameras[i - 1];
        if (state->bound) {
            media_unbind_vcap(state->pipe, state->vpss_group);
            state->bound = false;
        }
        if (state->vpss_initialized) {
            (void)hi_mapi_vpss_deinit(state->vpss_group);
            state->vpss_initialized = false;
        }
    }
    for (size_t i = video->camera_count; i > 0; i--) {
        media_video_camera_state *state = &video->cameras[i - 1];
        if (state->isp_started) {
            (void)hi_mapi_vcap_stop_isp(state->pipe);
            state->isp_started = false;
        }
        if (state->pipe_started) {
            (void)hi_mapi_vcap_stop_pipe(state->pipe);
            state->pipe_started = false;
        }
        if (state->isp_initialized) {
            (void)hi_mapi_vcap_deinit_isp(state->pipe);
            state->isp_initialized = false;
        }
        if (state->channel_started) {
            (void)hi_mapi_vcap_stop_chn(state->pipe, MEDIA_VCAP_CHANNEL);
            state->channel_started = false;
        }
        if (state->device_started) {
            (void)hi_mapi_vcap_stop_dev(state->device);
            state->device_started = false;
        }
        if (state->sensor_initialized) {
            (void)hi_mapi_vcap_deinit_sensor(state->device);
            state->sensor_initialized = false;
        }
    }
    for (size_t i = 0; i < video->stream_count; i++) {
        media_stream_sync_destroy(&video->streams[i]);
    }
    if (video->jpeg_lock_initialized) {
        (void)pthread_mutex_destroy(&video->jpeg_lock);
        video->jpeg_lock_initialized = false;
    }
    video->initialized = false;
}

static media_video_stream_state *media_get_stream(media_context *context, uint32_t stream_id)
{
    if (context == NULL || !context->video.initialized || stream_id >= context->video.stream_count) {
        return NULL;
    }
    return &context->video.streams[stream_id];
}

media_status media_video_acquire_frame(media_context *context, uint32_t stream_id,
    int timeout_ms, media_video_frame **output)
{
    if (output == NULL || timeout_ms < 0) {
        return MEDIA_ERR_ARGUMENT;
    }
    *output = NULL;
    media_video_stream_state *stream = media_get_stream(context, stream_id);
    if (stream == NULL || stream->config.output != MEDIA_VIDEO_OUTPUT_FRAME) {
        return MEDIA_ERR_STATE;
    }
    (void)pthread_mutex_lock(&stream->frame_lock);
    if (stream->frame_outstanding) {
        (void)pthread_mutex_unlock(&stream->frame_lock);
        return MEDIA_ERR_BUSY;
    }
    media_video_frame *frame = calloc(1, sizeof(*frame));
    if (frame == NULL) {
        (void)pthread_mutex_unlock(&stream->frame_lock);
        return MEDIA_ERR_MEMORY;
    }
    if (mapi_vpss_get_chn_frame(stream->vpss_group, stream->vpss_channel,
        &frame->data, timeout_ms) != TD_SUCCESS) {
        free(frame);
        (void)pthread_mutex_unlock(&stream->frame_lock);
        return MEDIA_ERR_TIMEOUT;
    }
    frame->context = context;
    frame->stream_id = stream_id;
    stream->frame_outstanding = true;
    *output = frame;
    (void)pthread_mutex_unlock(&stream->frame_lock);
    return MEDIA_OK;
}

void media_video_release_frame(media_context *context, media_video_frame *frame)
{
    if (context == NULL || frame == NULL || frame->context != context) {
        return;
    }
    media_video_stream_state *stream = media_get_stream(context, frame->stream_id);
    if (stream == NULL) {
        return;
    }
    (void)hi_mapi_vpss_release_chn_frame(stream->vpss_group, stream->vpss_channel, &frame->data);
    (void)pthread_mutex_lock(&stream->frame_lock);
    stream->frame_outstanding = false;
    (void)pthread_mutex_unlock(&stream->frame_lock);
    free(frame);
}

media_status media_video_frame_get_info(const media_video_frame *frame, media_video_frame_info *info)
{
    if (frame == NULL || info == NULL || frame->data.pixel_format != HI_MAPI_PIXEL_FORMAT_YUV_SEMIPLANAR_420) {
        return MEDIA_ERR_ARGUMENT;
    }
    *info = (media_video_frame_info) {
        .format = MEDIA_PIXEL_FORMAT_NV12,
        .width = frame->data.width,
        .height = frame->data.height,
        .stride = {
            frame->data.stride[0],
            frame->data.stride[1],
        },
        .planes = {
            (const uint8_t *)(uintptr_t)frame->data.vir_addr[0],
            (const uint8_t *)(uintptr_t)frame->data.vir_addr[1],
        },
        .pts = frame->data.pts,
    };
    return MEDIA_OK;
}

size_t media_video_frame_packed_size(const media_video_frame *frame)
{
    if (frame == NULL || frame->data.width == 0 || frame->data.height == 0 ||
        frame->data.width > SIZE_MAX / frame->data.height) {
        return 0;
    }
    size_t luma = (size_t)frame->data.width * frame->data.height;
    return luma <= SIZE_MAX - luma / 2 ? luma + luma / 2 : 0;
}

media_status media_video_frame_copy(const media_video_frame *frame, void *buffer, size_t buffer_size)
{
    media_video_frame_info info;
    if (buffer == NULL || media_video_frame_get_info(frame, &info) != MEDIA_OK) {
        return MEDIA_ERR_ARGUMENT;
    }
    size_t required = media_video_frame_packed_size(frame);
    if (required == 0 || buffer_size < required || info.planes[0] == NULL || info.planes[1] == NULL) {
        return MEDIA_ERR_ARGUMENT;
    }
    uint8_t *destination = buffer;
    for (uint32_t row = 0; row < info.height; row++) {
        if (memcpy_s(destination + (size_t)row * info.width,
            buffer_size - (size_t)row * info.width,
            info.planes[0] + (size_t)row * info.stride[0], info.width) != EOK) {
            return MEDIA_ERR_MEMORY;
        }
    }
    size_t luma_size = (size_t)info.width * info.height;
    for (uint32_t row = 0; row < info.height / 2; row++) {
        if (memcpy_s(destination + luma_size + (size_t)row * info.width,
            buffer_size - luma_size - (size_t)row * info.width,
            info.planes[1] + (size_t)row * info.stride[1], info.width) != EOK) {
            return MEDIA_ERR_MEMORY;
        }
    }
    return MEDIA_OK;
}

media_status media_video_set_ae_compensation(media_context *context, uint32_t camera_id,
    uint8_t compensation)
{
    ot_isp_exposure_attr exposure = {0};

    if (context == NULL || !context->video.initialized || camera_id >= context->video.camera_count ||
        !context->video.cameras[camera_id].isp_started) {
        return MEDIA_ERR_ARGUMENT;
    }
    ot_vi_pipe pipe = (ot_vi_pipe)context->video.cameras[camera_id].pipe;
    if (ss_mpi_isp_get_exposure_attr(pipe, &exposure) != TD_SUCCESS) {
        return MEDIA_ERR_DEVICE;
    }
    exposure.bypass = TD_FALSE;
    exposure.op_type = OT_OP_MODE_AUTO;
    exposure.auto_attr.compensation = compensation;
    exposure.auto_attr.ae_strategy_mode = OT_ISP_AE_EXP_HIGHLIGHT_PRIOR;
    if (ss_mpi_isp_set_exposure_attr(pipe, &exposure) != TD_SUCCESS) {
        return MEDIA_ERR_DEVICE;
    }
    return MEDIA_OK;
}

media_status media_video_get_exposure_info(media_context *context, uint32_t camera_id,
    media_video_exposure_info *info)
{
    ot_isp_exp_info exposure = {0};

    if (context == NULL || info == NULL || !context->video.initialized ||
        camera_id >= context->video.camera_count || !context->video.cameras[camera_id].isp_started) {
        return MEDIA_ERR_ARGUMENT;
    }
    ot_vi_pipe pipe = (ot_vi_pipe)context->video.cameras[camera_id].pipe;
    if (ss_mpi_isp_query_exposure_info(pipe, &exposure) != TD_SUCCESS) {
        return MEDIA_ERR_DEVICE;
    }
    *info = (media_video_exposure_info) {
        .exposure_time_us = exposure.exp_time,
        .analog_gain = exposure.a_gain,
        .digital_gain = exposure.d_gain,
        .isp_digital_gain = exposure.isp_d_gain,
        .iso = exposure.iso,
        .average_luminance = exposure.ave_lum,
        .exposure_is_max = exposure.exposure_is_max == TD_TRUE,
        .histogram_error = exposure.hist_error,
        .first_stable_frame = exposure.first_stable_time,
    };
    return MEDIA_OK;
}

media_status media_video_read_packet(media_context *context, uint32_t stream_id,
    int timeout_ms, media_video_packet *packet)
{
    if (packet == NULL || timeout_ms < 0) {
        return MEDIA_ERR_ARGUMENT;
    }
    *packet = (media_video_packet) {0};
    media_video_stream_state *stream = media_get_stream(context, stream_id);
    if (stream == NULL || stream->config.output == MEDIA_VIDEO_OUTPUT_FRAME) {
        return MEDIA_ERR_STATE;
    }
    struct timespec deadline;
    if (!media_make_deadline(timeout_ms, &deadline)) {
        return MEDIA_ERR_ARGUMENT;
    }
    media_packet_queue *queue = &stream->packet_queue;
    (void)pthread_mutex_lock(&queue->lock);
    while (queue->count == 0 && !queue->stopped) {
        int result = pthread_cond_timedwait(&queue->ready, &queue->lock, &deadline);
        if (result == ETIMEDOUT) {
            (void)pthread_mutex_unlock(&queue->lock);
            return MEDIA_ERR_TIMEOUT;
        }
        if (result != 0) {
            (void)pthread_mutex_unlock(&queue->lock);
            return MEDIA_ERR_DEVICE;
        }
    }
    if (queue->stopped) {
        (void)pthread_mutex_unlock(&queue->lock);
        return MEDIA_ERR_STATE;
    }
    *packet = queue->packets[queue->read_index];
    queue->packets[queue->read_index] = (media_video_packet) {0};
    queue->read_index = (queue->read_index + 1) % MEDIA_PACKET_QUEUE_DEPTH;
    queue->count--;
    (void)pthread_mutex_unlock(&queue->lock);
    return MEDIA_OK;
}

void media_video_packet_release(media_video_packet *packet)
{
    if (packet == NULL) {
        return;
    }
    free(packet->data);
    *packet = (media_video_packet) {0};
}

media_status media_video_request_key_frame(media_context *context, uint32_t stream_id)
{
    media_video_stream_state *stream = media_get_stream(context, stream_id);
    if (stream == NULL || !stream->venc_started) {
        return MEDIA_ERR_STATE;
    }
    return hi_mapi_venc_request_idr(stream->venc_channel) == TD_SUCCESS ? MEDIA_OK : MEDIA_ERR_DEVICE;
}

media_status media_video_set_boxes(media_context *context, uint32_t stream_id,
    const media_video_box *boxes, size_t box_count)
{
    media_video_stream_state *stream = media_get_stream(context, stream_id);
    if (stream == NULL || box_count > MEDIA_VIDEO_MAX_BOXES ||
        (box_count > 0 && boxes == NULL)) {
        return MEDIA_ERR_ARGUMENT;
    }
    for (size_t i = 0; i < box_count; i++) {
        if (!isfinite(boxes[i].x1) || !isfinite(boxes[i].y1) ||
            !isfinite(boxes[i].x2) || !isfinite(boxes[i].y2) ||
            boxes[i].x1 < 0.0F || boxes[i].y1 < 0.0F || boxes[i].x2 > 1.0F ||
            boxes[i].y2 > 1.0F || boxes[i].x1 >= boxes[i].x2 || boxes[i].y1 >= boxes[i].y2) {
            return MEDIA_ERR_ARGUMENT;
        }
    }
    (void)pthread_mutex_lock(&stream->overlay_lock);
    if (box_count > 0 && memcpy_s(stream->boxes, sizeof(stream->boxes), boxes,
        box_count * sizeof(*boxes)) != EOK) {
        (void)pthread_mutex_unlock(&stream->overlay_lock);
        return MEDIA_ERR_MEMORY;
    }
    stream->box_count = box_count;
    (void)pthread_mutex_unlock(&stream->overlay_lock);
    return MEDIA_OK;
}

media_status media_video_set_lines(media_context *context, uint32_t stream_id,
    const media_video_line *lines, size_t line_count)
{
    media_video_stream_state *stream = media_get_stream(context, stream_id);
    if (stream == NULL || line_count > MEDIA_VIDEO_MAX_LINES ||
        (line_count > 0 && lines == NULL)) {
        return MEDIA_ERR_ARGUMENT;
    }
    for (size_t i = 0; i < line_count; i++) {
        if (!isfinite(lines[i].x1) || !isfinite(lines[i].y1) ||
            !isfinite(lines[i].x2) || !isfinite(lines[i].y2) ||
            lines[i].x1 < 0.0F || lines[i].y1 < 0.0F ||
            lines[i].x2 < 0.0F || lines[i].y2 < 0.0F ||
            lines[i].x1 > 1.0F || lines[i].y1 > 1.0F ||
            lines[i].x2 > 1.0F || lines[i].y2 > 1.0F) {
            return MEDIA_ERR_ARGUMENT;
        }
    }
    (void)pthread_mutex_lock(&stream->overlay_lock);
    if (line_count > 0 && memcpy_s(stream->lines, sizeof(stream->lines), lines,
        line_count * sizeof(*lines)) != EOK) {
        (void)pthread_mutex_unlock(&stream->overlay_lock);
        return MEDIA_ERR_MEMORY;
    }
    stream->line_count = line_count;
    (void)pthread_mutex_unlock(&stream->overlay_lock);
    return MEDIA_OK;
}

/*
 * Region handles are a global resource shared by everything using RGN, so the
 * captions of a stream take a contiguous block keyed off its encoder channel.
 */
static ot_rgn_handle label_handle(const media_video_stream_state *stream,
    size_t index)
{
    return (ot_rgn_handle)(MEDIA_LABEL_HANDLE_BASE +
        (td_u32)stream->venc_channel * MEDIA_VIDEO_MAX_LABELS + index);
}

static void label_channel(const media_video_stream_state *stream,
    ot_mpp_chn *chn)
{
    chn->mod_id = OT_ID_VENC;
    chn->dev_id = 0;
    chn->chn_id = (td_s32)stream->venc_channel;
}

/* Creates the regions once, sized for the longest caption the API accepts. */
static media_status label_create_regions(media_video_stream_state *stream)
{
    if (stream->labels_created) return MEDIA_OK;
    ot_rgn_attr attr;
    (void)memset_s(&attr, sizeof(attr), 0, sizeof(attr));
    attr.type = OT_RGN_OVERLAY;
    attr.attr.overlay.pixel_format = OT_PIXEL_FORMAT_ARGB_CLUT2;
    attr.attr.overlay.bg_color = 0;
    attr.attr.overlay.size.width = (td_u32)media_label_canvas_width();
    attr.attr.overlay.size.height = (td_u32)media_label_canvas_height();
    attr.attr.overlay.canvas_num = MEDIA_LABEL_CANVAS_NUM;
    /* Index 0 stays fully transparent; index 1 is recoloured per caption. */
    attr.attr.overlay.clut[0] = 0x00000000U;
    attr.attr.overlay.clut[1] = 0xFFFFFFFFU;
    for (size_t index = 0; index < MEDIA_VIDEO_MAX_LABELS; ++index) {
        if (ss_mpi_rgn_create(label_handle(stream, index), &attr) !=
            TD_SUCCESS) {
            for (size_t undo = 0; undo < index; ++undo) {
                (void)ss_mpi_rgn_destroy(label_handle(stream, undo));
            }
            return MEDIA_ERR_DEVICE;
        }
    }
    stream->labels_created = true;
    return MEDIA_OK;
}

void media_label_destroy_regions(media_video_stream_state *stream)
{
    if (!stream->labels_created) return;
    ot_mpp_chn chn;
    label_channel(stream, &chn);
    for (size_t index = 0; index < MEDIA_VIDEO_MAX_LABELS; ++index) {
        if (stream->label_used[index]) {
            (void)ss_mpi_rgn_detach_from_chn(label_handle(stream, index),
                &chn);
            stream->label_used[index] = false;
        }
        (void)ss_mpi_rgn_destroy(label_handle(stream, index));
    }
    stream->labels_created = false;
    stream->label_count = 0;
}

static media_status label_upload(media_video_stream_state *stream,
    size_t index, const media_video_label *label)
{
    const ot_rgn_handle handle = label_handle(stream, index);
    /* Recolouring is a palette edit, so the pixels stay as they are. */
    ot_rgn_attr attr;
    if (ss_mpi_rgn_get_attr(handle, &attr) != TD_SUCCESS) {
        return MEDIA_ERR_DEVICE;
    }
    attr.attr.overlay.clut[1] = 0xFF000000U |
        ((td_u32)label->red << 16) | ((td_u32)label->green << 8) |
        (td_u32)label->blue;
    if (ss_mpi_rgn_set_attr(handle, &attr) != TD_SUCCESS) {
        return MEDIA_ERR_DEVICE;
    }

    uint8_t *canvas = malloc(media_label_canvas_bytes());
    if (canvas == NULL) return MEDIA_ERR_MEMORY;
    uint32_t used_width = 0;
    uint32_t used_height = 0;
    media_status status = media_label_render(label, canvas,
        media_label_canvas_width(), media_label_canvas_height(),
        &used_width, &used_height);
    if (status == MEDIA_OK) {
        ot_bmp bmp = {
            .pixel_format = OT_PIXEL_FORMAT_ARGB_CLUT2,
            .width = (td_u32)media_label_canvas_width(),
            .height = (td_u32)media_label_canvas_height(),
            .data = canvas,
        };
        if (ss_mpi_rgn_set_bmp(handle, &bmp) != TD_SUCCESS) {
            status = MEDIA_ERR_DEVICE;
        }
    }
    free(canvas);
    return status;
}

static media_status label_place(media_video_stream_state *stream, size_t index,
    const media_video_label *label)
{
    ot_mpp_chn chn;
    label_channel(stream, &chn);
    ot_rgn_chn_attr chn_attr;
    (void)memset_s(&chn_attr, sizeof(chn_attr), 0, sizeof(chn_attr));
    chn_attr.is_show = TD_TRUE;
    chn_attr.type = OT_RGN_OVERLAY;
    /* Region placement must be even in both axes. */
    chn_attr.attr.overlay_chn.point.x =
        (td_s32)((td_u32)(label->x * (float)stream->config.width) & ~1U);
    chn_attr.attr.overlay_chn.point.y =
        (td_s32)((td_u32)(label->y * (float)stream->config.height) & ~1U);
    chn_attr.attr.overlay_chn.fg_alpha = MEDIA_LABEL_FG_ALPHA;
    chn_attr.attr.overlay_chn.bg_alpha = 0;
    chn_attr.attr.overlay_chn.layer = (td_u32)index;
    const ot_rgn_handle handle = label_handle(stream, index);
    if (stream->label_used[index]) {
        return ss_mpi_rgn_set_chn_display_attr(handle, &chn, &chn_attr) ==
            TD_SUCCESS ? MEDIA_OK : MEDIA_ERR_DEVICE;
    }
    if (ss_mpi_rgn_attach_to_chn(handle, &chn, &chn_attr) != TD_SUCCESS) {
        return MEDIA_ERR_DEVICE;
    }
    stream->label_used[index] = true;
    return MEDIA_OK;
}

media_status media_label_commit(media_video_stream_state *stream,
    const media_video_label *labels, size_t label_count)
{
    media_status status = label_create_regions(stream);
    if (status != MEDIA_OK) return status;

    ot_mpp_chn chn;
    label_channel(stream, &chn);
    for (size_t index = 0; index < MEDIA_VIDEO_MAX_LABELS; ++index) {
        if (index >= label_count) {
            /* Surplus regions are hidden rather than torn down and rebuilt. */
            if (stream->label_used[index]) {
                (void)ss_mpi_rgn_detach_from_chn(label_handle(stream, index),
                    &chn);
                stream->label_used[index] = false;
            }
            continue;
        }
        const media_video_label *label = &labels[index];
        const bool fresh = !stream->label_used[index] ||
            media_label_changed(&stream->labels[index], label);
        if (fresh) {
            status = label_upload(stream, index, label);
            if (status != MEDIA_OK) return status;
        }
        status = label_place(stream, index, label);
        if (status != MEDIA_OK) return status;
        stream->labels[index] = *label;
    }
    stream->label_count = label_count;
    return MEDIA_OK;
}

media_status media_video_set_labels(media_context *context, uint32_t stream_id,
    const media_video_label *labels, size_t label_count)
{
    media_video_stream_state *stream = media_get_stream(context, stream_id);
    if (stream == NULL || (label_count > 0 && labels == NULL)) {
        return MEDIA_ERR_ARGUMENT;
    }
    (void)pthread_mutex_lock(&stream->overlay_lock);
    const media_status status = media_label_apply(stream, labels, label_count);
    (void)pthread_mutex_unlock(&stream->overlay_lock);
    return status;
}

static td_s32 media_jpeg_callback(td_handle channel, hi_mapi_venc_data_attr *encoded,
    td_void *private_data)
{
    (void)channel;
    media_jpeg_waiter *waiter = private_data;
    if (waiter == NULL || encoded == NULL) {
        return TD_FAILURE;
    }
    size_t total = 0;
    for (td_u32 i = 0; i < encoded->pack_count; i++) {
        size_t pack_size = media_encoded_pack_size(&encoded->data_pack[i]);
        if (pack_size == 0 || pack_size > SIZE_MAX - total) {
            return TD_FAILURE;
        }
        total += pack_size;
    }
    uint8_t *data = malloc(total);
    if (data == NULL) {
        return TD_FAILURE;
    }
    size_t written = 0;
    for (td_u32 i = 0; i < encoded->pack_count; i++) {
        const hi_mapi_venc_data_pack_attr *pack = &encoded->data_pack[i];
        for (td_u32 j = 0; j < HI_MAPI_VENC_PACK_NUM; j++) {
            td_u32 offset = j == 0 ? pack->offset : 0;
            if (pack->vir_addr[j] == NULL || offset >= pack->len[j]) {
                continue;
            }
            size_t part = (size_t)pack->len[j] - offset;
            if (memcpy_s(data + written, total - written,
                pack->vir_addr[j] + offset, part) != EOK) {
                free(data);
                return TD_FAILURE;
            }
            written += part;
        }
    }
    (void)pthread_mutex_lock(&waiter->lock);
    if (waiter->waiting) {
        waiter->data = data;
        waiter->size = written;
        waiter->status = MEDIA_OK;
        waiter->waiting = false;
        data = NULL;
        (void)pthread_cond_signal(&waiter->ready);
    }
    (void)pthread_mutex_unlock(&waiter->lock);
    free(data);
    return TD_SUCCESS;
}

media_status media_video_save_jpeg(media_context *context, uint32_t stream_id,
    const char *path, int timeout_ms)
{
    media_video_stream_state *stream = media_get_stream(context, stream_id);
    if (stream == NULL || path == NULL || path[0] == '\0' || timeout_ms < 0) {
        return MEDIA_ERR_ARGUMENT;
    }
    (void)pthread_mutex_lock(&context->video.jpeg_lock);
    hi_mapi_venc_base_attr base = {
        .type = HI_MAPI_PAYLOAD_TYPE_JPEG,
        .width = stream->config.width,
        .height = stream->config.height,
        .src_frame_rate = stream->config.frame_rate,
        .dst_frame_rate = stream->config.frame_rate,
        .qfactor = MEDIA_JPEG_QUALITY,
    };
    hi_mapi_venc_attr attributes = {0};
    media_jpeg_waiter waiter = {
        .waiting = true,
        .status = MEDIA_ERR_DEVICE,
    };
    hi_mapi_venc_call_back callback = {
        .proc_data_cb = media_jpeg_callback,
        .private_data = &waiter,
    };
    if (pthread_mutex_init(&waiter.lock, NULL) != 0) {
        (void)pthread_mutex_unlock(&context->video.jpeg_lock);
        return MEDIA_ERR_DEVICE;
    }
    if (pthread_cond_init(&waiter.ready, NULL) != 0) {
        (void)pthread_mutex_destroy(&waiter.lock);
        (void)pthread_mutex_unlock(&context->video.jpeg_lock);
        return MEDIA_ERR_DEVICE;
    }
    bool initialized = false;
    bool registered = false;
    bool started = false;
    hi_mapi_frame_data frame = {0};
    bool frame_acquired = false;
    media_status status = MEDIA_ERR_DEVICE;
    /*
     * Pause the encoder worker before taking the stream. It relocks
     * frame_lock and the sdk_server client lock back to back on every frame,
     * and the unfair mutexes could otherwise starve this thread for seconds.
     */
    atomic_store_explicit(&stream->worker_paused, true, memory_order_release);
    (void)pthread_mutex_lock(&stream->frame_lock);
    if (hi_mapi_venc_get_default_attr(&base, &attributes) != TD_SUCCESS ||
        hi_mapi_venc_init(MEDIA_JPEG_ENCODER, &attributes) != TD_SUCCESS) {
        goto finished;
    }
    initialized = true;
    if (hi_mapi_venc_register_callback(MEDIA_JPEG_ENCODER, &callback) != TD_SUCCESS) {
        goto finished;
    }
    registered = true;
    if (hi_mapi_venc_start(MEDIA_JPEG_ENCODER, 1) != TD_SUCCESS) {
        goto finished;
    }
    started = true;
    if (stream->frame_outstanding) {
        status = MEDIA_ERR_BUSY;
        goto finished;
    }
    if (mapi_vpss_get_chn_frame(stream->vpss_group, stream->vpss_channel,
        &frame, timeout_ms) != TD_SUCCESS) {
        goto finished;
    }
    frame_acquired = true;
    media_draw_boxes(stream, &frame);
    if (hi_mapi_venc_send_frame(MEDIA_JPEG_ENCODER, &frame, timeout_ms) != TD_SUCCESS) {
        goto finished;
    }
    struct timespec deadline;
    if (!media_make_deadline(timeout_ms, &deadline)) {
        status = MEDIA_ERR_ARGUMENT;
        goto finished;
    }
    (void)pthread_mutex_lock(&waiter.lock);
    while (waiter.waiting) {
        int result = pthread_cond_timedwait(&waiter.ready, &waiter.lock, &deadline);
        if (result == ETIMEDOUT) {
            waiter.waiting = false;
            waiter.status = MEDIA_ERR_TIMEOUT;
            break;
        }
        if (result != 0) {
            waiter.waiting = false;
            waiter.status = MEDIA_ERR_DEVICE;
            break;
        }
    }
    status = waiter.status;
    (void)pthread_mutex_unlock(&waiter.lock);
    if (status == MEDIA_OK) {
        FILE *file = fopen(path, "wb");
        if (file == NULL) {
            status = MEDIA_ERR_IO;
        } else {
            bool write_failed = fwrite(waiter.data, 1, waiter.size, file) != waiter.size;
            bool close_failed = fclose(file) != 0;
            if (write_failed || close_failed) {
                status = MEDIA_ERR_IO;
            }
        }
    }

finished:
    if (frame_acquired) {
        (void)hi_mapi_vpss_release_chn_frame(stream->vpss_group, stream->vpss_channel, &frame);
    }
    if (started) {
        (void)hi_mapi_venc_stop(MEDIA_JPEG_ENCODER);
    }
    if (registered) {
        (void)hi_mapi_venc_un_register_callback(MEDIA_JPEG_ENCODER, &callback);
    }
    if (initialized) {
        (void)hi_mapi_venc_deinit(MEDIA_JPEG_ENCODER);
    }
    (void)pthread_mutex_unlock(&stream->frame_lock);
    atomic_store_explicit(&stream->worker_paused, false, memory_order_release);
    free(waiter.data);
    (void)pthread_cond_destroy(&waiter.ready);
    (void)pthread_mutex_destroy(&waiter.lock);
    (void)pthread_mutex_unlock(&context->video.jpeg_lock);
    return status;
}
