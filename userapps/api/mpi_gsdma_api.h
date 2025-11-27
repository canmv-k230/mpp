/**
 * @file mpi_gsdma_api.h
 * @author
 * @brief Defines APIs related to GDMA (Graphic DMA) device
 * @version 1.0
 * @date 2025-01-12
 *
 * @copyright
 * Copyright (c), Canaan Bright Sight Co., Ltd
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef __MPI_GSDMA_API_H__
#define __MPI_GSDMA_API_H__

#include "k_type.h"
#include "k_gsdma_comm.h"
#include "k_video_comm.h"

#ifdef __cplusplus
extern "C" {
#endif /* End of #ifdef __cplusplus */

/** \addtogroup     GSDMA */
/** @{ */ /** <!-- [GSDMA] */

/**
 * @brief Initialize the GSDMA module
 * @return k_s32
 * @retval 0 success
 * @retval "not 0" see err code
 * @see K_ERR_CODE_E
 */
k_s32 kd_mpi_gsdma_init(void);

/**
 * @brief Deinitialize the GSDMA module
 * @return k_s32
 * @retval 0 success
 * @retval "not 0" see err code
 * @see K_ERR_CODE_E
 */
k_s32 kd_mpi_gsdma_deinit(void);

/**
 * @brief Set the device attributes of the GSDMA
 * @param [in] attr device attributes
 * @return k_s32
 * @retval 0 success
 * @retval "not 0" see err code
 * @see K_ERR_CODE_E
 */
k_s32 kd_mpi_gsdma_set_dev_attr(k_gsdma_dev_attr_t* attr);

/**
 * @brief Get the device attributes of the GSDMA
 * @param [out] attr device attributes
 * @return k_s32
 * @retval 0 success
 * @retval "not 0" see err code
 * @see K_ERR_CODE_E
 */
k_s32 kd_mpi_gsdma_get_dev_attr(k_gsdma_dev_attr_t* attr);

/**
 * @brief Perform a memory transfer via SDMA (1D/2D)
 * @param [in] cfg Transfer configuration (source, destination, size, mode)
 * @return k_s32
 * @retval 0 success
 * @retval "not 0" see err code
 * @note This wraps an SDMA transfer ioctl on /dev/gsdma_device
 */
k_s32 kd_mpi_gsdma_mem_transfer(k_sdma_transfer_cfg_t* cfg);

k_s32 kd_mpi_gsdma_sdma_memcpy(k_sdma_memcpy_t *cfg);

k_s32 kd_mpi_gsdma_sdma_memset(k_sdma_memset_t *cfg);

/**
 * @brief Send a frame for GSDMA processing (rotation, mirror)
 * @param [in] cfg Channel configuration (rotation, mirror parameters)
 * @param [in] src_frame Source frame information
 * @param [in] dst_frame Destination frame information
 * @param [in] blk_handle Destination frame blk buffer
 * @param [in] timeout_ms Timeout in milliseconds, -1 for infinite wait
 * @return k_s32
 * @retval 0 success
 * @retval "not 0" see err code
 * @see K_ERR_CODE_E
 * @note This function queues the transfer and may wait based on timeout_ms
 */
k_s32 kd_mpi_gsdma_send_frame(k_gdma_chn_cfg_t* cfg, k_video_frame_info* src_frame, k_video_frame_info* dst_frame,
                              k_vb_blk_handle blk_handle, k_s32 timeout_ms);

k_s32 kd_mpi_gsdma_send_buffer(k_gdma_chn_cfg_t* cfg, k_video_frame_info* src_frame, k_u64 buffer_phys_addr, k_u64 buffer_size,
                               k_s32 timeout_ms);

/** @} */ /** <!-- ==== GDMA End ==== */

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif
