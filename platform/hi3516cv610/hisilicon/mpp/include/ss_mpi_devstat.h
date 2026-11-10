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

#ifndef SS_MPI_DEVSTAT_H
#define SS_MPI_DEVSTAT_H

#include "ot_common_devstat.h"

#ifdef __cplusplus
extern "C" {
#endif

td_s32 ss_mpi_devstat_query_device_status(ot_devstat_device_status *device_status);

#ifdef __cplusplus
}
#endif

#endif /* SS_MPI_DEVSTAT_H */
