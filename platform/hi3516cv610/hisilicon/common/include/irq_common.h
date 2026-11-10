/*
 * Copyright (c) 2026 HiSilicon (Shanghai) Technologies Co., Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef __IRQ_COMMON_H__
#define __IRQ_COMMON_H__
#include "ot_defines.h"

/* For IRQ */
#define MAX_STATE_NUM               4
#define VIPROC_MAX_STATE_NUM        4
#define SVP_STREAM_NUM        4
#define VEDU_MAX_STATE_NUM          2

typedef enum {
    OT_INT_NORMAL = 0,
    OT_INT_VICAP  = 1,
    OT_INT_SCD    = 2,
    OT_INT_VDH    = 3,
    OT_INT_NNIE   = 4,
    OT_INT_AIO    = 5,
    OT_INT_VIPROC = 6,
    OT_INT_SVP    = 7,
	OT_INT_VEDU   = 8,
    OT_INT_BUTT,
} td_int_type;

typedef struct {
    volatile unsigned int state_num;                    /* Number of interrupt states */
    volatile unsigned int raw_state[MAX_STATE_NUM];     /* raw interrupt states */
    volatile unsigned int mask_state[MAX_STATE_NUM];    /* interrupt states after mask */
} td_normal_int_state;

typedef struct {
    volatile unsigned int state_num;
    volatile unsigned int int_state[VIPROC_MAX_STATE_NUM];
    volatile unsigned int raw_state[VIPROC_MAX_STATE_NUM];
    volatile unsigned int mask_state[VIPROC_MAX_STATE_NUM];
} td_viproc_int_state;

typedef struct {
    volatile unsigned int state_num;
    volatile unsigned int mask_state[VEDU_MAX_STATE_NUM];
    volatile unsigned int raw_state[VEDU_MAX_STATE_NUM];
    volatile unsigned int frame_cnt[VEDU_MAX_STATE_NUM];
} td_vedu_int_state;

typedef struct {
    volatile unsigned int cap_state;
    volatile unsigned int src_state;
    volatile unsigned int pt_state[OT_VI_MAX_DEV_NUM];
    volatile unsigned int ch_state[OT_VI_MAX_PHYS_PIPE_NUM];
    volatile unsigned int isp_fe_state[OT_VI_MAX_PHYS_PIPE_NUM];
} td_vicap_int_state;

typedef struct {
    unsigned int aio_int_status_state;
    unsigned int aio_int_raw_state;
    unsigned int tx_raw_state[OT_AO_DEV_MAX_NUM];
    unsigned int tx_stat_state[OT_AO_DEV_MAX_NUM];
    unsigned int rx_raw_state[OT_AI_DEV_MAX_NUM];
    unsigned int rx_stat_state[OT_AI_DEV_MAX_NUM];
} td_aio_int_state;

typedef struct {
    volatile unsigned int state_num;
    volatile unsigned int raw_state[MAX_STATE_NUM];
    volatile unsigned int mask_state[MAX_STATE_NUM];
    volatile unsigned int stream_sta[MAX_STATE_NUM];
    volatile unsigned int stream_clr[SVP_STREAM_NUM];
    volatile unsigned int aicpu_start_status[MAX_STATE_NUM];
    volatile unsigned int aicpu_clr[MAX_STATE_NUM];
    volatile unsigned int aic_int0_sta[MAX_STATE_NUM];
    volatile unsigned int aic_int0_clr[MAX_STATE_NUM];
    volatile unsigned int max_node_num;
} td_svp_int_state;

typedef struct {
    td_int_type type;
    union {
        td_normal_int_state normal;         /* AUTO:td_int_type:OT_INT_NORMAL; */
        td_vicap_int_state  vicap;          /* AUTO:td_int_type:OT_INT_VICAP; */
        td_aio_int_state    aio;            /* AUTO:td_int_type:OT_INT_AIO; */
        td_viproc_int_state viproc;         /* AUTO:td_int_type:OT_INT_VIPROC; */
        td_svp_int_state    svp;            /* AUTO:td_int_type:OT_INT_SVP; */
        td_vedu_int_state   vedu;           /* AUTO:td_int_type:OT_INT_VEDU; */
    };
} td_int_state_info;

typedef struct {
    void *dev;
    td_int_state_info int_info;
} td_irq_arg;


typedef struct {
    unsigned int cap_reg;
    unsigned int src_reg;
    unsigned int pt_reg[OT_VI_MAX_DEV_NUM];
    unsigned int ch_reg[OT_VI_MAX_PHYS_PIPE_NUM];
    unsigned int isp_fe_reg[OT_VI_MAX_PHYS_PIPE_NUM];
} td_vicap_int_reg;

typedef struct {
    unsigned int reg_num;
    unsigned int raw_int_reg[VEDU_MAX_STATE_NUM];   /* raw int state */
    unsigned int int_reg[VEDU_MAX_STATE_NUM];       /* int state after mask */
    unsigned int frame_cnt_reg[VEDU_MAX_STATE_NUM];
    unsigned int clr_int_reg[VEDU_MAX_STATE_NUM];
} td_vedu_int_reg;

typedef struct {
    unsigned int aio_int_status_reg;
    unsigned int aio_int_raw_reg;
    unsigned int tx_clr_reg[OT_AO_DEV_MAX_NUM];
    unsigned int tx_raw_reg[OT_AO_DEV_MAX_NUM];
    unsigned int tx_stat_reg[OT_AO_DEV_MAX_NUM];
    unsigned int rx_clr_reg[OT_AI_DEV_MAX_NUM];
    unsigned int rx_raw_reg[OT_AI_DEV_MAX_NUM];
    unsigned int rx_stat_reg[OT_AI_DEV_MAX_NUM];
} td_aio_int_reg;

typedef struct {
    unsigned int reg_num;
    unsigned int raw_int_reg[MAX_STATE_NUM];   /* raw int state */
    unsigned int mask_int_reg[MAX_STATE_NUM];  /* int state after mask */
    unsigned int clr_int_reg[MAX_STATE_NUM];
} td_normal_int_reg;

typedef struct {
    unsigned int reg_num;
    unsigned int int_reg[VIPROC_MAX_STATE_NUM];
    unsigned int raw_int_reg[VIPROC_MAX_STATE_NUM];
    unsigned int mask_int_reg[VIPROC_MAX_STATE_NUM];
    unsigned int clr_int_reg[VIPROC_MAX_STATE_NUM];
} td_viproc_int_reg;

typedef struct {
    unsigned int reg_num;
    unsigned int raw_int_reg[MAX_STATE_NUM];
    unsigned int mask_int_reg[MAX_STATE_NUM];
    unsigned int clr_int_reg[MAX_STATE_NUM];
    unsigned int stream_sta[MAX_STATE_NUM];
    unsigned int stream_clr[SVP_STREAM_NUM];
    unsigned int aicpu_start_status[MAX_STATE_NUM];
    unsigned int aicpu_clr[MAX_STATE_NUM];
    unsigned int aic_int0_sta[MAX_STATE_NUM];
    unsigned int aic_int0_clr[MAX_STATE_NUM];
    unsigned int max_node_num;
} td_svp_int_reg;

typedef struct {
    td_int_type type;
    union {
        td_normal_int_reg   normal;         /* AUTO:td_int_type:OT_INT_NORMAL; */
        td_vicap_int_reg    vicap;          /* AUTO:td_int_type:OT_INT_VICAP; */
        td_aio_int_reg      aio;            /* AUTO:td_int_type:OT_INT_AIO; */
        td_viproc_int_reg   viproc;         /* AUTO:td_int_type:OT_INT_VIPROC; */
        td_svp_int_reg      svp;            /* AUTO:td_int_type:OT_INT_SVP; */
        td_vedu_int_reg     vedu;           /* AUTO:td_int_type:OT_INT_VEDU; */
    };
} td_int_reg_info;

#endif /* end of #ifndef __IRQ_COMMON_H__ */

