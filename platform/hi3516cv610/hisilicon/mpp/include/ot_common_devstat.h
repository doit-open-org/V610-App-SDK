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

#ifndef OT_COMMON_DEVSTAT_H
#define OT_COMMON_DEVSTAT_H

#include "ot_errno.h"
#include "ot_common.h"

#define OT_ERR_DEVSTAT_NULL_PTR         OT_DEFINE_ERR(OT_ID_DEVSTAT, OT_ERR_LEVEL_ERROR, OT_ERR_NULL_PTR)
#define OT_ERR_DEVSTAT_NOT_READY        OT_DEFINE_ERR(OT_ID_DEVSTAT, OT_ERR_LEVEL_ERROR, OT_ERR_NOT_READY)
#define OT_ERR_DEVSTAT_EXIST            OT_DEFINE_ERR(OT_ID_DEVSTAT, OT_ERR_LEVEL_ERROR, OT_ERR_EXIST)
#define OT_ERR_DEVSTAT_UNEXIST          OT_DEFINE_ERR(OT_ID_DEVSTAT, OT_ERR_LEVEL_ERROR, OT_ERR_UNEXIST)
#define OT_ERR_DEVSTAT_NOT_SUPPORT      OT_DEFINE_ERR(OT_ID_DEVSTAT, OT_ERR_LEVEL_ERROR, OT_ERR_NOT_SUPPORT)
#define OT_ERR_DEVSTAT_NOT_PERM         OT_DEFINE_ERR(OT_ID_DEVSTAT, OT_ERR_LEVEL_ERROR, OT_ERR_NOT_PERM)
#define OT_ERR_DEVSTAT_NOMEM            OT_DEFINE_ERR(OT_ID_DEVSTAT, OT_ERR_LEVEL_ERROR, OT_ERR_NO_MEM)
#define OT_ERR_DEVSTAT_NOBUF            OT_DEFINE_ERR(OT_ID_DEVSTAT, OT_ERR_LEVEL_ERROR, OT_ERR_NO_BUF)
#define OT_ERR_DEVSTAT_ILLEGAL_PARAM    OT_DEFINE_ERR(OT_ID_DEVSTAT, OT_ERR_LEVEL_ERROR, OT_ERR_ILLEGAL_PARAM)
#define OT_ERR_DEVSTAT_BUSY             OT_DEFINE_ERR(OT_ID_DEVSTAT, OT_ERR_LEVEL_ERROR, OT_ERR_BUSY)
#define OT_ERR_DEVSTAT_BUF_EMPTY        OT_DEFINE_ERR(OT_ID_DEVSTAT, OT_ERR_LEVEL_ERROR, OT_ERR_BUF_EMPTY)

typedef struct {
    td_u32 clk; /* RO; unit: MHz */
    td_u32 bit_width; /* RO; unit: bit */
} ot_devstat_ddr_status;

typedef struct {
    td_u32 clk; /* RO; unit: MHz */
} ot_devstat_fmc_status;

typedef enum {
    OT_DEVSTAT_DEVICE_TYPE_DDR,
    OT_DEVSTAT_DEVICE_TYPE_FMC,

    OT_DEVSTAT_DEVICE_TYPE_BUTT
} ot_devstat_device_type;

typedef struct {
    ot_devstat_device_type type;
    union {
        ot_devstat_ddr_status ddr; /* AUTO: ot_devstat_device_type: OT_DEVSTAT_DEVICE_TYPE_DDR */
        ot_devstat_fmc_status fmc; /* AUTO: ot_devstat_device_type: OT_DEVSTAT_DEVICE_TYPE_FMC */
    };
} ot_devstat_device_status;

#endif /* end of #ifndef OT_COMMON_DEVSTAT_H */
