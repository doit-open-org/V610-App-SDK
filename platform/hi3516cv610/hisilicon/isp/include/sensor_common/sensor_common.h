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

#ifndef SENSOR_COMMON_H
#define SENSOR_COMMON_H

#include <pthread.h>
#include "ot_math.h"
#include "ot_common_isp.h"
#include "ot_sns_ctrl.h"
#include "ot_mpi_isp.h"
#include "ot_mpi_ae.h"
#include "ot_mpi_awb.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

#define sns_check_pipe_return(pipe)                                \
    do {                                                           \
        if (((pipe) < 0) || ((pipe) >= OT_ISP_MAX_PIPE_NUM)) {     \
            isp_err_trace("Err isp pipe %d!\n", (pipe));           \
            return OT_ERR_ISP_ILLEGAL_PARAM;                       \
        }                                                          \
    } while (0)

#define sns_check_pipe_void_return(pipe)                           \
    do {                                                           \
        if (((pipe) < 0) || ((pipe) >= OT_ISP_MAX_PIPE_NUM)) {     \
            isp_err_trace("Err isp pipe %d!\n", (pipe));           \
            return;                                                \
        }                                                          \
    } while (0)

#define sns_check_return(ret) \
    do { \
        td_s32 _ret = (ret); \
        if ((_ret) != TD_SUCCESS) { \
            isp_err_trace("failed! ret:0x%x\n", _ret); \
            return _ret; \
        } \
    } while (0)

#define sns_check_pipe_sns_num_return(num)                                \
    do {                                                           \
        if (((num) <= 0) || ((num) > OT_ISP_PIPE_SENSOR_MAX_NUM)) {     \
            isp_err_trace("Err isp pipe sensor num %d!\n", (num));           \
            return OT_ERR_ISP_ILLEGAL_PARAM;                       \
        }                                                          \
    } while (0)

/* common I2C bus config */
#define I2C_DEV_FILE_NUM     16
#define I2C_BUF_NUM          8

typedef struct {
    td_u16 addr;
    td_u16 data;
} cis_reg_cfg;

typedef struct {
    td_u32 addr;
    td_u32 data;
} cis_reg_cfg_with_32bits;

typedef struct {
    ot_isp_3a_alg_lib *ae_lib;
    ot_isp_3a_alg_lib *awb_lib;
    ot_isp_sns_register isp_register;
    ot_isp_ae_sensor_register ae_register;
    ot_isp_awb_sensor_register awb_register;
} cis_register;

typedef struct {
    td_s32 fd;
    td_u32 addr;
    td_u8 addr_byte_num;
    td_u8 data_byte_num;
} cis_i2c;

typedef struct {
    td_s32 pipe;
    pthread_mutex_t mutex;
    ot_sensor_id sns_id;
    ot_isp_fswdr_mode fswdr_mode;
    td_u8 ae_stat_pos;
    td_u32 max_time_get_cnt;
    td_u32 init_exposure;
    td_u32 init_int_time;
    td_u32 init_again;
    td_u32 init_dgain;
    td_u32 init_isp_dgain;
    td_u32 lines_per500ms;

    td_u16 init_wb_r_gain;
    td_u16 init_wb_g_gain;
    td_u16 init_wb_b_gain;

    td_u16 sample_r_gain;
    td_u16 sample_b_gain;
    td_bool quick_start_en;
    td_bool fast_ae_en;

    td_bool ae_route_ex_valid;
    ot_isp_ae_route init_ae_route;
    ot_isp_ae_route_ex init_ae_route_ex;
    ot_isp_ae_route init_ae_route_sf;
    ot_isp_ae_route_ex init_ae_route_sf_ex;

    td_u32 max_short_exp;

    ot_isp_sns_state *sns_state;
    td_bool blc_clamp_info;
    cis_video_mode_tbl mode_tbl[SENSOR_MODE_MAX];
    td_u32 pre_ratio;

    ot_isp_sns_commbus bus_info;
    cis_i2c i2c;

    ot_isp_sns_commbus bus_info_multi[OT_ISP_PIPE_SENSOR_MAX_NUM];
    cis_i2c i2c_multi[OT_ISP_PIPE_SENSOR_MAX_NUM];
    td_u32  sns_multi_num;
} cis_info;

td_void cis_delay_ms(td_u32 ms);
td_s32  cis_register_callback(cis_info *cis, cis_register *reg);
td_s32  cis_unregister_callback(cis_info *cis, cis_register *reg);
td_s32  cis_i2c_init(cis_info *cis);
td_s32  cis_i2c_exit(cis_info *cis);
td_s32  cis_read_reg(cis_i2c *i2c, td_u32 addr);
td_s32  cis_write_reg(cis_i2c *i2c, td_u32 addr, td_u32 data);
td_s32  cis_wdr_range_check(td_u32 *ratio);
td_void cis_init_attr(cis_info *cis, ot_isp_init_attr *init_attr);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif
#endif
