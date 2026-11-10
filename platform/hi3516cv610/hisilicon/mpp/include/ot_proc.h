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

#ifndef OT_PROC_H
#define OT_PROC_H

#include "ot_type.h"
#include "ot_osal.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define MAX_PROC_CMD_LEN 50
#define MAX_PROC_NAME_LEN 50
#define OT_PROC_BUF_SIZE 65536
#define OT_PROC_WRITE_CMD_SIZE 32

#define OT_PROC_DEVICE_NAME "ot_proc"
#define OT_PROC_PFX "ot_proc: "

#define OT_USER_PROC_READ_CMD "__read"
#define OT_USER_PROC_WRITE_CMD "__write"

typedef struct {
    td_char *buf;
    td_u32 size;
    td_u32 offset;
} td_proc_show_buf;

typedef struct {
    td_char buf[OT_PROC_WRITE_CMD_SIZE];
    td_u32 count;
    td_s64 ppos;
} td_proc_write_buf;

typedef struct {
    td_char cmd[MAX_PROC_CMD_LEN];
    osal_proc_entry entry;
    td_proc_write_buf write_buf;
} td_proc_para;

typedef struct {
    char name[MAX_PROC_NAME_LEN];
} td_proc_name;

#define TD_ID_PROC 'Y'

#define USER_CREATE_PROC_ENTRY           _IOW(TD_ID_PROC, 1, td_proc_name)
#define USER_REMOVE_PROC_ENTRY           _IOW(TD_ID_PROC, 2, td_proc_name)
#define USER_PROC_GET_CMD                _IOWR(TD_ID_PROC, 5, td_proc_para)
#define USER_PROC_WAKE_GET_CMD           _IOW(TD_ID_PROC, 4, td_proc_para)
#define USER_PROC_WAKE_READ_TASK         _IOW(TD_ID_PROC, 6, td_proc_show_buf)
#define USER_PROC_WAKE_WRITE_TASK        _IOW(TD_ID_PROC, 7, td_proc_show_buf)

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
