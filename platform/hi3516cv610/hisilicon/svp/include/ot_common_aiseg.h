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

#ifndef OT_COMMON_AISEG_H
#define OT_COMMON_AISEG_H

#include "ot_type.h"
#include "ot_common.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define OT_AISEG_MAX_CLASS_NUM              255

typedef struct {
    td_s16  delta_x;
    td_s16  delta_y;
} ot_aiseg_point_delta;

typedef struct {
    td_u8   prob;
    td_u8   point_num;
    td_u16  base_point_x;
    td_u16  base_point_y;
} ot_aiseg_point;

typedef struct {
    td_u8  category;
    td_u8  aiseg_num;
} ot_aiseg_point_category;

typedef struct {
    td_u32   size;
    td_bool  prob_enable;
    td_u8    category_num;
} ot_aiseg_point_info;

typedef struct {
    td_u8   prob;
    td_u16  left;
    td_u16  top;
    td_u16  width;
    td_u16  height;
} ot_aiseg_rect;

typedef struct {
    td_u8  category;
    td_u8  aiseg_num;
} ot_aiseg_rect_category;

typedef struct {
    td_u32   size;
    td_bool  prob_enable;
    td_u8    category_num;
} ot_aiseg_rect_info;

typedef struct {
    td_u16  map_width;
    td_u16  map_height;
    td_u8   map_class_num;
    td_u8   map_label[OT_AISEG_MAX_CLASS_NUM];
    td_u8   map_value[OT_AISEG_MAX_CLASS_NUM];
} ot_aiseg_map_info;

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* OT_COMMON_VIDEO_H */
