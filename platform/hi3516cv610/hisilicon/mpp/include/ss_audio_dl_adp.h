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

#ifndef SS_AUDIO_DL_ADP_H
#define SS_AUDIO_DL_ADP_H

#include "ot_audio_dl_adp.h"

#ifdef __cplusplus
extern "C" {
#endif /* End of #ifdef __cplusplus */

td_s32 ss_audio_dlpath(td_char *lib_path);

td_s32 ss_audio_dlopen(td_void **lib_handle, td_char *lib_name);

td_s32 ss_audio_dlsym(td_void **func_handle, td_void *lib_handle, td_char *func_name);

td_s32 ss_audio_dlclose(td_void *lib_handle);

#ifdef __cplusplus
}
#endif /* End of #ifdef __cplusplus */

#endif
