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

#ifndef SS_MP3DEC_H
#define SS_MP3DEC_H

#include "ot_mp3dec.h"

#ifdef __cplusplus
extern "C" {
#endif /* End of #ifdef __cplusplus */

ot_mp3dec_handle ss_mp3dec_init_decoder(td_void);

td_s32 ss_mp3dec_find_sync_header(ot_mp3dec_handle mp3_decoder, td_u8 **in_buff, td_s32 *bytes_left);

td_s32 ss_mp3dec_frame(ot_mp3dec_handle mp3_decoder, td_u8 **in_buff,
    td_s32 *bytes_left, td_s16 *out_pcm, td_s32 reserved_num);

td_void ss_mp3dec_free_decoder(ot_mp3dec_handle mp3_decoder);

td_s32 ss_mp3dec_get_last_frame_info(ot_mp3dec_handle mp3_decoder, ot_mp3dec_frame_info *mp3dec_frame_info);

#ifdef __cplusplus
}
#endif /* End of #ifdef __cplusplus */

#endif
