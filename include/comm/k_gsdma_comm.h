/**
 * @copyright
 * Copyright (c) 2025, Canaan Bright Sight Co., Ltd
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
#ifndef __K_GSDMA_COMM_H__
#define __K_GSDMA_COMM_H__

#include "k_errno.h"
#include "k_module.h"

#ifdef __cplusplus
extern "C" {
#endif /* End of #ifdef __cplusplus */

/* GSDMA device attributes (shared for GDMA/SDMA) */
typedef struct {
    k_u8  burst_len; /**< burst长度 */
    k_u8  outstanding; /**< outstanding数量 */
    k_u8  ckg_bypass; /**< 时钟门控bypass */
    k_u32 arbitration_weight; /**< 仲裁权重 */
} k_gsdma_dev_attr_t;

/**
 * @brief Defines the rotation mode of the gdma
 *
 */
typedef enum {
    GDMA_ROTATE_NONE       = 0,
    GDMA_ROTATE_DEGREE_0   = (1 << 0), /**< Rotate 0 degrees */
    GDMA_ROTATE_DEGREE_90  = (1 << 1), /**< Rotate 90 degrees */
    GDMA_ROTATE_DEGREE_180 = (1 << 2), /**< Rotate 180 degrees */
    GDMA_ROTATE_DEGREE_270 = (1 << 3), /**< Rotate 270 degrees */

    GDMA_ROTATE_MIRROR_HOR = (1 << 4), /**< Rotate Mirror Horizontal */
    GDMA_ROTATE_MIRROR_VER = (1 << 5), /**< Rotate Mirror Vertical */
} k_gdma_rotation_e;

typedef enum {
    GDMA_DST_FRAME  = 1, /**< Rotate to VideoFrameInfo */
    GDMA_DST_BUFFER = 2, /**< Rotate to PhysAddr Buffer */
} k_gdma_dst_type_e;

/* GDMA channel configuration */
typedef struct {
    k_gdma_rotation_e rotation; /**< 旋转角度 */
    k_gdma_dst_type_e dst_type; /**< 目标输出类型, 用户不需要关心 */
} k_gdma_chn_cfg_t;

/**
 * @brief Defines the 1d or 2d mode for sdma
 *
 */
typedef enum {
    /**< One dimensional mode (Line mode),
        In line mode, the total length of the current data is line_size, line_space and line_num do not need to be configured */
    DIMENSION1,

    /**< Two dimensional mode，
    2D mode can move 2D format data(such as image)to the target memory for continuous storage. */
    DIMENSION2,
} k_sdma_mode_e;

typedef enum {
    SDMA_DATA_MODE_NORMAL_MODE, /**< normal mode, read data from the source and transfer to the destination */
    SDMA_DATA_MODE_FIXED_MODE, /**<  Fixed value mode, transfer the data in the register user_data to the destination */
} k_sdma_data_mode_e;

typedef enum {
    SDMA_DATA_SIZE_1_BYTE, /**< 1-byte, the lowest 8-bit is valid */
    SDMA_DATA_SIZE_2_BYTE, /**< 2-byte, the lowest 16-bit is valid */
    SDMA_DATA_SIZE_4_BYTE, /**< 4-byte, the entire CHx_USR_DATA is valid */
} k_sdma_data_size_e;

/**
 * @brief SDMA data endianness configuration
 *
 * This field is used to specify the byte order of the transmitted data
 */
typedef enum {
    SDMA_DATA_ENDIAN_DEFAULT = 0, /**< Default configuration, do nothing */
    SDMA_DATA_ENDIAN_2BYTE, /**< 2-byte endianness conversion: transmission length is 2 bytes */
    SDMA_DATA_ENDIAN_4BYTE, /**< 4-byte endianness conversion: transmission length is 4 bytes */
    SDMA_DATA_ENDIAN_8BYTE /**< 8-byte endianness conversion: transmission length is 8 bytes */
} k_sdma_data_endian_e;

typedef union {
    struct {
        k_u32 dat_mode : 1; /**< @k_sdma_data_mode_e */
        k_u32 dat_size : 2; /**< @k_sdma_data_size_e */
        k_u32 reserved_0 : 1;
        k_u32 dat_endian : 2; /**< @k_sdma_data_endian_e */
        k_u32 reserved_1 : 2;
        k_u32 src_fixed : 1;
        k_u32 dst_fixed : 1;
        k_u32 dec_en : 1; /**< chn0 and chn1 can used for gzip. */
        k_u32 reserved_2 : 5;
        k_u32 wr_outstanding : 4;
        k_u32 rd_outstanding : 4;
        k_u32 reserved_3 : 8;
    };
    k_u32 value;
} k_sdma_ch_cfg_t;

typedef struct {
    void*           src_addr; /**< phys_addr */
    void*           dst_addr; /**< phys_addr */
    k_u8            dimension; /**< @k_sdma_mode_e */
    k_u16           line_num; /**< used when DIMENSION2, how many lines */
    k_u16           line_space; /**< used when DIMENSION2, line gap */
    k_u32           line_size; /**< one line data size */
    k_u32           user_data; /**< used when SDMA_DATA_MODE_FIXED_MODE */
    k_s32           timeout_ms;
    k_sdma_ch_cfg_t ch_cfg;
} k_sdma_transfer_cfg_t;

typedef struct {
    k_u64 dst_phys_addr;
    k_u64 src_phys_addr;
    k_u64 size;
    k_s32 timeout_ms;
} k_sdma_memcpy_t;

typedef struct {
    k_u64              phys_addr;
    k_u64              size;
    k_u32              data;
    k_sdma_data_size_e data_size;
    k_s32              timeout_ms;
} k_sdma_memset_t;

#define K_ERR_DMA_INVALID_DEVID K_DEF_ERR(K_ID_DMA, K_ERR_LEVEL_ERROR, K_ERR_INVALID_DEVID)
#define K_ERR_DMA_INVALID_CHNID K_DEF_ERR(K_ID_DMA, K_ERR_LEVEL_ERROR, K_ERR_INVALID_CHNID)
#define K_ERR_DMA_ILLEGAL_PARAM K_DEF_ERR(K_ID_DMA, K_ERR_LEVEL_ERROR, K_ERR_ILLEGAL_PARAM)
#define K_ERR_DMA_EXIST         K_DEF_ERR(K_ID_DMA, K_ERR_LEVEL_ERROR, K_ERR_EXIST)
#define K_ERR_DMA_UNEXIST       K_DEF_ERR(K_ID_DMA, K_ERR_LEVEL_ERROR, K_ERR_UNEXIST)
#define K_ERR_DMA_NULL_PTR      K_DEF_ERR(K_ID_DMA, K_ERR_LEVEL_ERROR, K_ERR_NULL_PTR)
#define K_ERR_DMA_NOT_CONFIG    K_DEF_ERR(K_ID_DMA, K_ERR_LEVEL_ERROR, K_ERR_NOT_CONFIG)
#define K_ERR_DMA_NOT_SUPPORT   K_DEF_ERR(K_ID_DMA, K_ERR_LEVEL_ERROR, K_ERR_NOT_SUPPORT)
#define K_ERR_DMA_NOT_PERM      K_DEF_ERR(K_ID_DMA, K_ERR_LEVEL_ERROR, K_ERR_NOT_PERM)
#define K_ERR_DMA_NOMEM         K_DEF_ERR(K_ID_DMA, K_ERR_LEVEL_ERROR, K_ERR_NOMEM)
#define K_ERR_DMA_NOBUF         K_DEF_ERR(K_ID_DMA, K_ERR_LEVEL_ERROR, K_ERR_NOBUF)
#define K_ERR_DMA_BUF_EMPTY     K_DEF_ERR(K_ID_DMA, K_ERR_LEVEL_ERROR, K_ERR_BUF_EMPTY)
#define K_ERR_DMA_BUF_FULL      K_DEF_ERR(K_ID_DMA, K_ERR_LEVEL_ERROR, K_ERR_BUF_FULL)
#define K_ERR_DMA_NOTREADY      K_DEF_ERR(K_ID_DMA, K_ERR_LEVEL_ERROR, K_ERR_NOTREADY)
#define K_ERR_DMA_BADADDR       K_DEF_ERR(K_ID_DMA, K_ERR_LEVEL_ERROR, K_ERR_BADADDR)
#define K_ERR_DMA_BUSY          K_DEF_ERR(K_ID_DMA, K_ERR_LEVEL_ERROR, K_ERR_BUSY)

/** @} */ /** <!-- ==== DMA End ==== */
#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
