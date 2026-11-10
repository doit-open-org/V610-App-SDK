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

#ifndef SS_MPI_AIVSR_H
#define SS_MPI_AIVSR_H

#include "ot_common_aivsr.h"
#include "ot_common_aidetect.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Prototype    : ss_mpi_aivsr_create_chn
 * Description  : Create aivsr channel and set channel attr.
 * Parameters   : ot_aivsr_chn           chn            Input channel,set by user. Range
 *                ot_aivsr_input_model   *input_model   Input model info.
 *                ot_aivsr_chn_attr      *chn_attr      Input channel attr.
 * Return Value : TD_SUCCESS: Success;Error codes: Failure.
 * Date         : 2024-08-29
 *
 */
td_s32 ss_mpi_aivsr_create_chn(ot_aivsr_chn chn, const ot_aivsr_input_model *input_model,
    const ot_aivsr_chn_attr *chn_attr);

/*
 * Prototype    : ss_mpi_aivsr_destroy_chn
 * Description  : Destroy  channel.
 * Parameters   : ot_aivsr_chn chn  Input channel, must exist.
 * Return Value : TD_SUCCESS: Success;Error codes: Failure.
 * Date         : 2024-08-29
 *
 */
td_s32 ss_mpi_aivsr_destroy_chn(ot_aivsr_chn chn);

/*
 * Prototype    : ss_mpi_aidetect_set_chn_attr
 * Description  : Set channel attr.
 * Parameters   : ot_aivsr_chn       chn          Input channel, must exist.
 *                ot_aivsr_chn_attr  *chn_attr    Chn attr.
 * Return Value : TD_SUCCESS: Success;Error codes: Failure.
 * Date         : 2024-08-29
 *
 */
td_s32 ss_mpi_aivsr_set_chn_attr(ot_aivsr_chn chn, const ot_aivsr_chn_attr *chn_attr);

/*
 * Prototype    : ss_mpi_aidetect_get_chn_attr
 * Description  : Get chanel attr.
 * Parameters   : ot_aivsr_chn        chn        Input channel, must exist.
 *                ot_aivsr_chn_attr   *chn_attr  Chn attr.
 * Return Value : TD_SUCCESS: Success;Error codes: Failure.
 * Date         : 2024-08-29
 *
 */
td_s32 ss_mpi_aivsr_get_chn_attr(ot_aivsr_chn chn, ot_aivsr_chn_attr *chn_attr);

/*
 * Prototype    : ss_mpi_aivsr_set_chn_param
 * Description  : Set channel param.
 * Parameters   : ot_aivsr_chn         chn          Input channel, must exist.
 *                ot_aivsr_chn_param   *chn_param    Chn param.
 * Return Value : TD_SUCCESS: Success;Error codes: Failure.
 * Date         : 2024-08-29
 *
 */
td_s32 ss_mpi_aivsr_set_chn_param(ot_aivsr_chn chn, const ot_aivsr_chn_param *chn_param);

/*
 * Prototype    : ss_mpi_aivsr_get_chn_param
 * Description  : Get chanel attr.
 * Parameters   : ot_aivsr_chn        chn        Input channel, must exist.
 *                ot_aivsr_chn_param  *chn_attr  Chn param.
 * Return Value : TD_SUCCESS: Success;Error codes: Failure.
 * Date         : 2024-08-29
 *
 */
td_s32 ss_mpi_aivsr_get_chn_param(ot_aivsr_chn chn, ot_aivsr_chn_param *chn_param);

/*
 * Prototype    : ss_mpi_aivsr_process
 * Description  : aivsr processing, sync interface, input frame and chn, then get the plate result.
 * Parameters   : ot_aivsr_chn     chn        Input channel, must exist.
 *                ot_video_frame   *frame     Input source frame data, default nv21.The input data must not be null.
 *                ot_aidetect_object_of_one_class *object  Input vehicle result from aidetect
 *                ot_aivsr_result_array           *result    Output plate result.
 * Return Value : TD_SUCCESS: Success; Error codes: Failure.
 * Date         : 2024-08-29
 *
 */
td_s32 ss_mpi_aivsr_process(ot_aivsr_chn chn, const ot_video_frame *frame,
    const ot_aidetect_object_of_one_class *object, ot_aivsr_result_array *result);

/*
 * Prototype    : ss_mpi_aivsr_register_result_callback
 * Description  : register result callback.
 * Parameters   : ot_aivsr_chn                   chn        Input channel must exist.
 *                ss_mpi_aivsr_result_callback   callback   Input result callback
 * Return Value: TD_SUCCESS: Success; Error codes: Failure.
 * Date         : 2024-08-29
 *
 */
td_s32 ss_mpi_aivsr_register_result_callback(ot_aivsr_chn chn, const ot_aivsr_result_callback callback);

/*
 * Prototype    : ss_mpi_aivsr_unregister_result_callback
 * Description  : unregister result callback.
 * Parameters   : ot_aivsr_chn     chn        Input channel must exist.
 * Return Value: TD_SUCCESS: Success; Error codes: Failure.
 * Date         : 2024-08-29
 *
 */
td_s32 ss_mpi_aivsr_unregister_result_callback(ot_aivsr_chn chn);

/*
 * Prototype    : ss_mpi_aivsr_async_process
 * Description  : aivsr processing, async interface, input frame and chn ,then get the plate result.
 * Parameters   : ot_aivsr_chn      chn         Input channel, must exist.
 *                ot_video_frame    *frame      Input source frame data, default nv21.The input data must not be null.
 *                ot_aidetect_object_of_one_class  *object   Input vehicle result from aidetect.
                  td_s32             milli_sec   timeout
 * Return Value : TD_SUCCESS: Success; Error codes: Failure.
 * Date         : 2024-08-29
 *
 */
td_s32 ss_mpi_aivsr_async_process(ot_aivsr_chn chn, const ot_video_frame *frame,
    const ot_aidetect_object_of_one_class *object, td_s32 milli_sec);

#ifdef __cplusplus
}
#endif

#endif
