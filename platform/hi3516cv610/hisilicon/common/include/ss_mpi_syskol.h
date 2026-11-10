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

#ifndef SS_MPI_SYSKOL_H
#define SS_MPI_SYSKOL_H

#include "ot_common.h"
#include "ot_common_syskol.h"

#ifdef __cplusplus
extern "C" {
#endif

td_s32 ss_mpi_syskol_set_param(const ot_syskol_param *param);
td_s32 ss_mpi_syskol_get_param(ot_syskol_param *param);

td_s32 ss_mpi_syskol_sleep(td_s32 milli_sec);
td_s32 ss_mpi_syskol_wakeup(td_void);

td_s32 ss_mpi_syskol_suspend(td_void);

#ifdef __cplusplus
}
#endif
#endif /* SS_MPI_SYSKOL_H */
