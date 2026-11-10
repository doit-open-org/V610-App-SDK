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

#ifndef OT_COMMON_FR_H
#define OT_COMMON_FR_H

#include "ot_type.h"
#include "ot_common_aidetect.h"

#define OT_FR_MAX_OUTPUT_NUM 10     /* RW; max output fr result object num */
#define OT_FR_FACE_FEATURE_LEN 512

typedef struct {
    float feature[OT_FR_FACE_FEATURE_LEN];
} ot_fr_face_feature;

typedef struct {
    ot_aidetect_object detect_object;
    ot_fr_face_feature face_feature;
} ot_fr_result;

typedef struct {
    td_u32 object_num;
    td_u32 object_capacity; /* RW; fr result capacity,memory must be setted by user */
    ot_fr_result* fr_info_result; /* RW; fr result addr,memory must be setted by user */
} ot_fr_result_array;

#endif
