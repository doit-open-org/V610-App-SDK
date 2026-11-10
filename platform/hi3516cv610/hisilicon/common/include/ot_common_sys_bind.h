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

#ifndef OT_COMMON_SYS_BIND_H
#define OT_COMMON_SYS_BIND_H

#include "ot_common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    td_u32 num;
    ot_mpp_chn mpp_chn[OT_MAX_BIND_DST_NUM];
} ot_mpp_bind_dst;

#ifdef __cplusplus
}
#endif
#endif /* OT_COMMON_SYS_BIND_H */
