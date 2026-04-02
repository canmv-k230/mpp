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
#ifndef __K_VO_COMM_H__
#define __K_VO_COMM_H__

#include "k_errno.h"
#include "k_gsdma_comm.h"
#include "k_module.h"
#include "k_type.h"
#include "k_video_comm.h"
#ifdef __cplusplus
extern "C" {
#endif /* End of #ifdef __cplusplus */

/* vo part */

#define K_VO_MAX_DEV_NUMS   (1)
#define K_VO_MAX_CHN_NUMS   (8)
#define K_VO_DISPLAY_DEV_ID (0)

#define K_VO_DEFAULT_MIX_ORDER (0x0000000076543210)

#define K_ERR_VO_INVALID_DEVID K_DEF_ERR(K_ID_VO, K_ERR_LEVEL_ERROR, K_ERR_INVALID_DEVID)
#define K_ERR_VO_INVALID_CHNID K_DEF_ERR(K_ID_VO, K_ERR_LEVEL_ERROR, K_ERR_INVALID_CHNID)
#define K_ERR_VO_ILLEGAL_PARAM K_DEF_ERR(K_ID_VO, K_ERR_LEVEL_ERROR, K_ERR_ILLEGAL_PARAM)
#define K_ERR_VO_EXIST         K_DEF_ERR(K_ID_VO, K_ERR_LEVEL_ERROR, K_ERR_EXIST)
#define K_ERR_VO_UNEXIST       K_DEF_ERR(K_ID_VO, K_ERR_LEVEL_ERROR, K_ERR_UNEXIST)
#define K_ERR_VO_NULL_PTR      K_DEF_ERR(K_ID_VO, K_ERR_LEVEL_ERROR, K_ERR_NULL_PTR)
#define K_ERR_VO_NOT_CONFIG    K_DEF_ERR(K_ID_VO, K_ERR_LEVEL_ERROR, K_ERR_NOT_CONFIG)
#define K_ERR_VO_NOT_SUPPORT   K_DEF_ERR(K_ID_VO, K_ERR_LEVEL_ERROR, K_ERR_NOT_SUPPORT)
#define K_ERR_VO_NOT_PERM      K_DEF_ERR(K_ID_VO, K_ERR_LEVEL_ERROR, K_ERR_NOT_PERM)
#define K_ERR_VO_NOMEM         K_DEF_ERR(K_ID_VO, K_ERR_LEVEL_ERROR, K_ERR_NOMEM)
#define K_ERR_VO_NOBUF         K_DEF_ERR(K_ID_VO, K_ERR_LEVEL_ERROR, K_ERR_NOBUF)
#define K_ERR_VO_BUF_EMPTY     K_DEF_ERR(K_ID_VO, K_ERR_LEVEL_ERROR, K_ERR_BUF_EMPTY)
#define K_ERR_VO_BUF_FULL      K_DEF_ERR(K_ID_VO, K_ERR_LEVEL_ERROR, K_ERR_BUF_FULL)
#define K_ERR_VO_NOTREADY      K_DEF_ERR(K_ID_VO, K_ERR_LEVEL_ERROR, K_ERR_NOTREADY)
#define K_ERR_VO_BADADDR       K_DEF_ERR(K_ID_VO, K_ERR_LEVEL_ERROR, K_ERR_BADADDR)
#define K_ERR_VO_BUSY          K_DEF_ERR(K_ID_VO, K_ERR_LEVEL_ERROR, K_ERR_BUSY)

typedef struct {
    k_gdma_rotation_e dev_rot_flg;
} k_vo_dev_attr;

typedef enum {
    K_VO_LAYER_VIDEO0 = 0, // current can not use this layer.
    K_VO_LAYER_VIDEO1 = 1,
    K_VO_LAYER_VIDEO2 = 2,
    K_VO_LAYER_VIDEO3 = 3,
    K_VO_LAYER_OSD0   = 4,
    K_VO_LAYER_OSD1   = 5,
    K_VO_LAYER_OSD2   = 6,
    K_VO_LAYER_OSD3   = 7,
    K_MAX_VO_LAYER_NR,
} k_vo_layer_id;

typedef struct {
    k_u32 x;
    k_u32 y;
} k_vo_position;

typedef struct {
    k_u32 width;
    k_u32 height;
} k_vo_size;

typedef struct {
    k_vo_layer_id     layer_id; // Unique identifier for the hardware video layer
    k_vo_position     position; // Display coordinates (x, y) on the screen
    k_vo_size         img_size; // Dimensions (width, height) of the input image
    k_pixel_format    pixel_format; // Input format; note: for video layer only PIXEL_FORMAT_YUV_SEMIPLANAR_420 is supported
    k_gdma_rotation_e func; // Rotation angle or mirroring mode (handled by GDMA)
    k_u8              global_alpha; // Transparency level (0: fully transparent, 255: fully opaque)
    k_u8              rot_buf_nr; // Number of rotation buffers; 2 is recommended for performance/stability
    k_u8              rot_buf_bpp; // Bits per pixel for rot-buffer; 0 to auto-calculate, 4 for dynamic format switching
} k_vo_layer_attr;

struct vo_disp_layer_mix_priority_t {
    union {
        struct {
            k_u64 layer0_sel : 4;
            k_u64 layer1_sel : 4;
            k_u64 layer2_sel : 4;
            k_u64 layer3_sel : 4;
            k_u64 layer4_sel : 4;
            k_u64 layer5_sel : 4;
            k_u64 layer6_sel : 4;
            k_u64 layer7_sel : 4;

            // not used.
            k_u64 layer8_sel : 4;
            k_u64 layer9_sel : 4;
            k_u64 layer10_sel : 4;
            k_u64 layer11_sel : 4;

            k_u64 recv : 16;
        } bits;
        k_u64 reg;
    };
};

typedef struct {
    k_u32 pclk_khz; // Pixel clock in kHz
    k_u32 hactive; // Horizontal active width (visible pixels per line)
    k_u32 hsync_len; // Horizontal sync length
    k_u32 hback_porch; // Horizontal back porch
    k_u32 hfront_porch; // Horizontal front porch
    k_u32 vactive; // Vertical active height (visible lines per frame)
    k_u32 vsync_len; // Vertical sync length
    k_u32 vback_porch; // Vertical back porch
    k_u32 vfront_porch; // Vertical front porch
} k_vo_timing;

/* dsi part */
typedef enum {
    K_DSI_1LANE = 1,
    K_DSI_2LANE = 2,
    K_DSI_4LANE = 4,
} k_vo_dsi_lane_num;

typedef enum {
    K_DSI_CMD_LP_MODE = 0, // Low-power command mode (used by all current panels)
    K_DSI_CMD_HS_MODE, // High-speed command mode (driver-supported, currently unused by panels)
} k_vo_dsi_cmd_mode;

typedef enum {
    K_DSI_VIDEO_BURST_MODE                     = 0, // Burst mode (default for all panels)
    K_DSI_VIDEO_NON_BURST_MODE_WITH_SYNC_EVENT = 1, // Driver-supported, currently unused by panels
    K_DSI_VIDEO_NON_BURST_MODE_WITH_PULSES     = 2, // Driver-supported, currently unused by panels
} k_vo_dsi_video_mode;

typedef struct {
    struct {
        k_u32 datarate;
        k_u32 m, n, voc, hs_freq;
    } phy;

    const k_vo_timing* timing;

    k_vo_dsi_lane_num   lanes; // Number of data lanes (1/2/4)
    k_vo_dsi_cmd_mode   cmd_mode; // Command mode (LP_MODE/HS_MODE)
    k_vo_dsi_video_mode video_mode; // Video mode (BURST/NON_BURST_SYNC_PULSES/NON_BURST_SYNC_EVENTS)
    k_u8                vc_id; // Virtual channel ID (0-3, typically 0 for single display)
    k_u8                lp_speed_mhz; // Low-power mode speed in MHz (used for cmd_mode = LP_MODE)
} k_vo_dsi_config;

/* old vo part */
typedef struct {
    k_u32 draw_en;
    k_u32 line_x_start;
    k_u32 line_y_start;
    k_u32 line_x_end;
    k_u32 line_y_end;
    k_u32 frame_num;
} k_vo_draw_frame;

typedef enum {
    K_VO_LAYER0 = 0,
    K_VO_LAYER1 = 1,
    K_VO_LAYER2 = 2,
    K_MAX_VO_LAYER_NUM,
} k_vo_layer __attribute__((deprecated("should use k_vo_layer_id")));

typedef enum {
    K_VO_OSD0 = 0,
    K_VO_OSD1 = 1,
    K_VO_OSD2 = 2,
    K_VO_OSD3 = 3,
    K_MAX_VO_OSD_NUM,
} k_vo_osd __attribute__((deprecated("should use k_vo_layer_id")));

/* for wbc */
typedef struct {
    k_u32 blk_cnt; // Number of VB blocks to allocate in the pool
} k_vo_wbc_attr;

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
