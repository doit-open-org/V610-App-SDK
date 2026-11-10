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
#ifndef OT_VI_EXPORT_H
#define OT_VI_EXPORT_H

#include "ot_common.h"
#include "ot_common_vi.h"
#include "ot_type.h"


typedef struct {
    td_s32 (*vi_export_switch_gpio_high_level)(ot_vi_pipe vi_pipe);
    td_s32 (*vi_export_switch_gpio_low_level)(ot_vi_pipe vi_pipe);
} ot_vi_export_callback;

typedef td_s32 ot_vi_register_export_callback(ot_vi_pipe vi_pipe, ot_vi_export_callback *export_callback);

typedef struct {
    ot_vi_register_export_callback *register_export_callback;
} ot_vi_export_symbol;

ot_vi_export_symbol *ot_vi_get_export_symbol(td_void);

#endif /* OT_VI_EXPORT_H */
