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

#ifndef SS_MPI_SYS_BIND_H
#define SS_MPI_SYS_BIND_H

#include "ot_common.h"
#include "ot_common_sys_bind.h"

#ifdef __cplusplus
extern "C" {
#endif

td_s32 ss_mpi_sys_bind(const ot_mpp_chn *src_chn, const ot_mpp_chn *dst_chn);
td_s32 ss_mpi_sys_unbind(const ot_mpp_chn *src_chn, const ot_mpp_chn *dst_chn);

td_s32 ss_mpi_sys_get_bind_by_dst(const ot_mpp_chn *dst_chn, ot_mpp_chn *src_chn);
td_s32 ss_mpi_sys_get_bind_by_src(const ot_mpp_chn *src_chn, ot_mpp_bind_dst *bind_dst);

#ifdef __cplusplus
}
#endif
#endif /* SS_MPI_SYS_BIND_H */
