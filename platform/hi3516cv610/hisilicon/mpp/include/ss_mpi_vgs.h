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

#ifndef SS_MPI_VGS_H
#define SS_MPI_VGS_H

#include "ot_common.h"
#include "ot_common_video.h"
#include "ot_common_vgs.h"

#ifdef __cplusplus
extern "C" {
#endif

td_s32 ss_mpi_vgs_begin_job(ot_vgs_handle *handle);

td_s32 ss_mpi_vgs_end_job(ot_vgs_handle handle);

td_s32 ss_mpi_vgs_cancel_job(ot_vgs_handle handle);

td_s32 ss_mpi_vgs_add_scale_task(ot_vgs_handle handle, const ot_vgs_task_attr *task, ot_vgs_scale_coef_mode mode);

td_s32 ss_mpi_vgs_add_draw_line_task(ot_vgs_handle handle, const ot_vgs_task_attr *task, const ot_vgs_line line[],
    td_u32 array_size);

td_s32 ss_mpi_vgs_add_cover_task(ot_vgs_handle handle, const ot_vgs_task_attr *task, const ot_cover cover[],
    td_u32 array_size);

td_s32 ss_mpi_vgs_add_mosaic_task(ot_vgs_handle handle, const ot_vgs_task_attr *task, const ot_mosaic mosaic[],
    td_u32 array_size);

td_s32 ss_mpi_vgs_add_osd_task(ot_vgs_handle handle, const ot_vgs_task_attr *task, const ot_vgs_osd osd[],
    td_u32 array_size);

td_s32 ss_mpi_vgs_add_rotation_task(ot_vgs_handle handle, const ot_vgs_task_attr *task, ot_rotation angle);

td_s32 ss_mpi_vgs_add_luma_task(ot_vgs_handle handle, const ot_vgs_task_attr *task, const ot_rect luma_rect[],
    td_u32 array_size, td_u64 luma_data[]);

td_s32 ss_mpi_vgs_add_corner_rect_task(ot_vgs_handle handle, const ot_vgs_task_attr *task,
    const ot_corner_rect corner_rect[], const ot_corner_rect_attr *corner_rect_attr, td_u32 array_size);

td_s32 ss_mpi_vgs_add_stitch_task(ot_vgs_handle handle, const ot_vgs_stitch_task_attr *task, td_u32 pic_num);

td_s32 ss_mpi_vgs_add_online_task(ot_vgs_handle handle, const ot_vgs_task_attr *task, const ot_vgs_online *online);

td_s32 ss_mpi_vgs_get_ver_split_job(const ot_vgs_ver_split_attr *ver_split_attr,
    ot_vgs_handle *handle, td_u32 *node_num);
 
td_s32 ss_mpi_vgs_proc_ver_split(ot_vgs_handle handle, td_u32 node_id, ot_video_frame_info *in_frame,
    ot_video_frame_info *out_frame);
 
td_s32 ss_mpi_vgs_release_ver_split_job(ot_vgs_handle handle);

#ifdef __cplusplus
}
#endif

#endif /* end of SS_MPI_VGS_H */
