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

#ifndef SS_MPI_OTP_H
#define SS_MPI_OTP_H

#include "ot_type.h"

#ifdef __cplusplus
#if __cplusplus
{
#endif /* __cplusplus */
#endif /* __cplusplus */

td_s32 ss_mpi_otp_init(td_void);

td_s32 ss_mpi_otp_deinit(td_void);

td_s32 ss_mpi_otp_read_word(td_u32 offset, td_u32 *data);

td_s32 ss_mpi_otp_read_byte(td_u32 offset, td_u8 *data);

td_s32 ss_mpi_otp_write_byte(td_u32 offset, td_u8 data);

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif
