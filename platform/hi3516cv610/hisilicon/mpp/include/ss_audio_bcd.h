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

#ifndef SS_AUDIO_BCD_H
#define SS_AUDIO_BCD_H

#include "ot_audio_bcd.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* End of #ifdef __cplusplus */

td_s32 ss_baby_crying_detection_init(ot_bcd_handle *bcd, td_s32 sample_rate, const ot_bcd_config *bcd_config);

td_s32 ss_baby_crying_detection_process(ot_bcd_handle bcd, ot_bcd_process_data *input_data,
    ot_bcd_process_data *output_data);

td_s32 ss_baby_crying_detection_deinit(ot_bcd_handle bcd);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */

#endif
