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

#ifndef SS_MPI_KM_H
#define SS_MPI_KM_H

#include "ot_common_crypto.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

td_s32 ss_mpi_km_init(td_void);

td_s32 ss_mpi_km_deinit(td_void);

/* Keyslot. */
td_s32 ss_mpi_keyslot_create(crypto_handle *mpi_keyslot_handle, km_keyslot_type keyslot_type);
td_s32 ss_mpi_keyslot_destroy(crypto_handle mpi_keyslot_handle);

/* Klad. */
td_s32 ss_mpi_klad_create(crypto_handle *mpi_klad_handle);

td_s32 ss_mpi_klad_destroy(crypto_handle mpi_klad_handle);

td_s32 ss_mpi_klad_attach(crypto_handle mpi_klad_handle, km_klad_dest_type klad_type,
    crypto_handle mpi_keyslot_handle);

td_s32 ss_mpi_klad_detach(crypto_handle mpi_klad_handle, km_klad_dest_type klad_type,
    crypto_handle mpi_keyslot_handle);

td_s32 ss_mpi_klad_set_attr(crypto_handle mpi_klad_handle, const km_klad_attr *attr);

td_s32 ss_mpi_klad_get_attr(crypto_handle mpi_klad_handle, km_klad_attr *attr);

td_s32 ss_mpi_klad_set_session_key(crypto_handle mpi_klad_handle, const km_klad_session_key *key);

td_s32 ss_mpi_klad_set_content_key(crypto_handle mpi_klad_handle, const km_klad_content_key *key);

td_s32 ss_mpi_klad_set_clear_key(crypto_handle mpi_klad_handle, const km_klad_clear_key *key);

td_s32 ss_mpi_klad_set_effective_key(crypto_handle mpi_klad_handle, const km_klad_effective_key *key);

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif
