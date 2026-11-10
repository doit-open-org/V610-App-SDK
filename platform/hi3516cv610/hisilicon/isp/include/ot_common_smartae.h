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

#ifndef OT_COMMON_SMARTAE_H
#define OT_COMMON_SMARTAE_H

#include "ot_common_video.h"

#define OT_SMARTAE_MAX_RECT_NUM 5
#define OT_SMARTAE_MAX_OBJ_CLASS 2

typedef struct {
    ot_rect rect;
    td_float score;
} ot_smartae_obj;

typedef enum {
    OT_SMARTAE_OBJ_FACE = 0,
    OT_SMARTAE_OBJ_PEOPLE,
    OT_SMARTAE_OBJ_BUTT
} ot_smartae_obj_type;

typedef struct {
    ot_smartae_obj_type type;
    td_u32 rect_num;
    ot_smartae_obj objs[OT_SMARTAE_MAX_RECT_NUM];
} ot_smartae_obj_class;

typedef struct {
    td_u32 class_num;
    ot_smartae_obj_class obj_class[OT_SMARTAE_MAX_OBJ_CLASS];
} ot_smartae_roi_info;

#endif /* OT_COMMON_SMARTAE_H */
