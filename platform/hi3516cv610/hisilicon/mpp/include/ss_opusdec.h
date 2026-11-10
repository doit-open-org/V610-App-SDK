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

#ifndef SS_OPUSDEC_H
#define SS_OPUSDEC_H

#include "ot_opusdec.h"

#ifdef __cplusplus
extern "C" {
#endif /* End of #ifdef __cplusplus */

ot_opus_decoder ss_opusdec_create(const ot_opusdec_config *config);

td_void ss_opusdec_destroy(ot_opus_decoder decoder);

td_s32 ss_opusdec_process_frame(ot_opus_decoder decoder, td_u8 **in_buf, td_s32 *in_left_byte, td_s16 *out_buf,
    td_u32 *out_bytes_per_chn);

#ifdef __cplusplus
}
#endif /* End of #ifdef __cplusplus */

#endif
