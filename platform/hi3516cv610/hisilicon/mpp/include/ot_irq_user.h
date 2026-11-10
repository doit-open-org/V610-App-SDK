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


#ifndef __OT_IRQ_USER_H__
#define __OT_IRQ_USER_H__

#include <linux/types.h>
#include "ot_osal.h"
#include "irq_common.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define ot_irq_err_trace(fmt, ...) \
    HI_ERR_TRACE(OT_ID_IRQ, "[Func]:%s [Line]:%d [Info]:" fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__)

#define ot_irq_warn_trace(fmt, ...) \
    HI_WARN_TRACE(OT_ID_IRQ, "[Func]:%s [Line]:%d [Info]:" fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__)

#define ot_irq_debug_trace(fmt, ...) \
    HI_DEBUG_TRACE(OT_ID_IRQ, "[Func]:%s [Line]:%d [Info]:" fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__)

int ot_irq_request_irq(unsigned int irq, osal_irq_handler handler, osal_irq_handler thread_fn, const char *name,
    td_irq_arg *dev);
void ot_irq_free_irq(unsigned int irq, td_irq_arg *dev);
int ot_irq_in_interrupt(void);
int ot_irq_platform_get_irq(td_irq_arg *dev, unsigned int num);
int ot_irq_platform_get_irq_byname(td_irq_arg *dev, const char *name);
void *ot_irq_mmapdev(void *addr, unsigned long length, int prot, int flags, int fd, unsigned long offset);
int ot_irq_platform_set_irq_reg(unsigned int irq_num, td_int_reg_info *reg_info);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* End of #ifndef __OT_IRQ_USER_H__ */
