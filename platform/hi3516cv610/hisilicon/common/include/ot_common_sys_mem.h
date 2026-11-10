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

#ifndef OT_COMMON_SYS_MEM_H
#define OT_COMMON_SYS_MEM_H

#include "ot_type.h"
#include "ot_errno.h"

#define OT_ERR_MEM_ILLEGAL_PARAM OT_DEFINE_ERR(OT_ID_MEM, OT_ERR_LEVEL_ERROR, OT_ERR_ILLEGAL_PARAM)
#define OT_ERR_MEM_NULL_PTR      OT_DEFINE_ERR(OT_ID_MEM, OT_ERR_LEVEL_ERROR, OT_ERR_NULL_PTR)
#define OT_ERR_MEM_NOT_PERM      OT_DEFINE_ERR(OT_ID_MEM, OT_ERR_LEVEL_ERROR, OT_ERR_NOT_PERM)
#define OT_ERR_MEM_NOT_READY     OT_DEFINE_ERR(OT_ID_MEM, OT_ERR_LEVEL_ERROR, OT_ERR_NOT_READY)
#define OT_ERR_MEM_BUSY          OT_DEFINE_ERR(OT_ID_MEM, OT_ERR_LEVEL_ERROR, OT_ERR_BUSY)

typedef struct {
    td_phys_addr_t phys_addr;
    td_bool is_cached;
} ot_sys_virt_mem_info;

typedef struct {
    td_phys_addr_t phys_addr;
    td_u64 offset;
    td_void *mem_handle;
} ot_sys_mem_info;

typedef struct {
    td_u32 mmz_total; /* RO; unit: KB */
    td_u32 mmz_free;  /* RO; unit: KB */
} ot_sys_mmz_status;

#endif /* OT_COMMON_SYS_MEM_H */
