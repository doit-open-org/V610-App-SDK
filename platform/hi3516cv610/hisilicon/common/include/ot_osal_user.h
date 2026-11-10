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

#ifndef __OT_OSAL_USER_H__
#define __OT_OSAL_USER_H__

#include "autoconf.h"
#include "ot_osal.h"

#ifdef CONFIG_USER_SPACE
#include "osal_uds.h"
#endif

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

#ifdef CONFIG_USER_SPACE // user space
#define OSAL_OPEN           osal_uds_client_open
#define OSAL_READ           osal_uds_client_read
#define OSAL_WRITE          osal_uds_client_write
#define OSAL_IOCTL          osal_uds_client_ioctl
#define OSAL_IOCTL_EX       osal_uds_client_ioctl_ex
#define OSAL_IOCTL_SHM      osal_uds_client_ioctl_shm
#define OSAL_CLOSE          osal_uds_client_close
#define OSAL_MMAP           osal_mmapdev
#define OSAL_SELECT         osal_uds_client_select
#define OSAL_DEV_FD_SET     osal_dev_fd_set
#define OSAL_FD_ZERO        osal_fd_zero
#define OSAL_FD_SET         osal_fd_set
#define OSAL_FD_CLR         osal_fd_clr
#define OSAL_FD_ISSET       osal_fd_isset


#else // kernel space
#define OSAL_OPEN           open
#define OSAL_READ           read
#define OSAL_WRITE          write
#define OSAL_IOCTL          ioctl
#define OSAL_CLOSE          close
#define OSAL_MMAP           mmap
#define OSAL_SELECT         select
#define OSAL_DEV_FD_SET     fd_set
#define OSAL_FD_ZERO        FD_ZERO
#define OSAL_FD_SET         FD_SET
#define OSAL_FD_ISSET       FD_ISSET

#endif

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif // __OT_OSAL_USER_H__
