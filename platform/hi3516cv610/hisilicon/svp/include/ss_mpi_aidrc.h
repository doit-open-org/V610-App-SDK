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

#ifndef SS_MPI_AIDRC_H
#define SS_MPI_AIDRC_H

#include "ot_common_aiisp.h"
#include "ot_common_aidrc.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

td_s32 ss_mpi_aidrc_init(td_void);
td_s32 ss_mpi_aidrc_exit(td_void);
td_s32 ss_mpi_aidrc_set_thread_attr(const ot_aiisp_thread_attr *thread_attr);
td_s32 ss_mpi_aidrc_get_thread_attr(ot_aiisp_thread_attr *thread_attr);

td_s32 ss_mpi_aidrc_enable(td_s32 pipe);
td_s32 ss_mpi_aidrc_disable(td_s32 pipe);
td_s32 ss_mpi_aidrc_query_status(td_s32 pipe, ot_aidrc_status *status);
td_s32 ss_mpi_aidrc_set_attr(td_s32 pipe, const ot_aidrc_attr *attr);
td_s32 ss_mpi_aidrc_get_attr(td_s32 pipe, ot_aidrc_attr *attr);
td_s32 ss_mpi_aidrc_set_cfg(td_s32 pipe, const ot_aidrc_cfg *cfg);
td_s32 ss_mpi_aidrc_get_cfg(td_s32 pipe, ot_aidrc_cfg *cfg);

td_s32 ss_mpi_aidrc_load_model(const ot_aidrc_model *model, td_s32 *model_id);
td_s32 ss_mpi_aidrc_unload_model(td_s32 model_id);

td_s32 ss_mpi_aidrc_set_smooth(td_s32 pipe, const ot_aidrc_smooth *smooth);
td_s32 ss_mpi_aidrc_get_smooth(td_s32 pipe, ot_aidrc_smooth *smooth);

td_s32 ss_mpi_aidrc_get_output_frame(td_s32 pipe,          ot_video_frame_info *frame,
    ot_aiisp_mem_info *out_data[], td_u32 out_data_num, td_s32 milli_sec);
td_s32 ss_mpi_aidrc_release_output_frame(td_s32 pipe, ot_video_frame_info *frame);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* SS_MPI_AIDRC_H */
