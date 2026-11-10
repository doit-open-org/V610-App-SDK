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

#ifndef OT_COMMON_SYSKOL_H
#define OT_COMMON_SYSKOL_H

#include "ot_type.h"

#ifdef __cplusplus
extern "C" {
#endif /* End of #ifdef __cplusplus */

typedef enum {
    OT_SYSKOL_CAPTURE_MODE_NORMAL = 0, /* capture frame continuously */
    OT_SYSKOL_CAPTURE_MODE_SINGLE,     /* capture one frame after syskol_suspend or syskol_sleep done */
    OT_SYSKOL_CAPTURE_MODE_BUTT
} ot_syskol_capture_mode;

typedef enum {
    OT_SYSKOL_SLEEP_MODE_NONE,    /* next sleep with nothing */
    OT_SYSKOL_SLEEP_MODE_LIGHT,   /* next sleep light, only cpu run with slow freq */
    OT_SYSKOL_SLEEP_MODE_DEEP,    /* next sleep deep, all ip need stop working, sensor run with standby */
    OT_SYSKOL_SLEEP_MODE_USER,    /* next sleep with user config, some ip could work commpared with deep mode */
    OT_SYSKOL_SLEEP_MODE_BUTT
} ot_syskol_sleep_mode;

typedef union {
    struct {
        td_u32 audio_cken : 1;
    } bits;
    td_u32 cken_mask;
} ot_syskol_sleep_user_param;

typedef struct {
    ot_syskol_capture_mode capture_mode; /* kol capture frame mode */
    ot_syskol_sleep_mode   sleep_mode;   /* kol sleep mode */
    ot_syskol_sleep_user_param user_param; /* kol user param */
} ot_syskol_param;

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* OT_COMMON_SYSKOL_H */
