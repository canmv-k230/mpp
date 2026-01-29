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
#include "k_module.h"
#include "k_video_comm.h"
#include "k_gsdma_comm.h"
#include "k_type.h"
#ifdef __cplusplus
extern "C" {
#endif /* End of #ifdef __cplusplus */

/* vo part */

#define K_VO_MAX_DEV_NUMS                               (1)
#define K_VO_MAX_CHN_NUMS                               (8)
#define K_VO_DISPLAY_DEV_ID                             (0)

typedef struct {
    k_gdma_rotation_e dev_rot_flg;
} k_vo_dev_attr;

typedef enum
{
    K_VO_LAYER_VIDEO0 = 0, // current can not use this layer.
    K_VO_LAYER_VIDEO1 = 1,
    K_VO_LAYER_VIDEO2 = 2,
    K_VO_LAYER_VIDEO3 = 3,
    K_VO_LAYER_OSD0 = 4,
    K_VO_LAYER_OSD1 = 5,
    K_VO_LAYER_OSD2 = 6,
    K_VO_LAYER_OSD3 = 7,
    K_MAX_VO_LAYER_NR,
} k_vo_layer_id;

typedef struct
{
    k_u32 x;
    k_u32 y;
} k_vo_position;

typedef struct
{
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

#define K_VO_DEFAULT_MIX_ORDER (0x0000000076543210)

#define K_ERR_VO_INVALID_DEVID                       K_DEF_ERR(K_ID_VO, K_ERR_LEVEL_ERROR, K_ERR_INVALID_DEVID)
#define K_ERR_VO_INVALID_CHNID                       K_DEF_ERR(K_ID_VO, K_ERR_LEVEL_ERROR, K_ERR_INVALID_CHNID)
#define K_ERR_VO_ILLEGAL_PARAM                       K_DEF_ERR(K_ID_VO, K_ERR_LEVEL_ERROR, K_ERR_ILLEGAL_PARAM)
#define K_ERR_VO_EXIST                               K_DEF_ERR(K_ID_VO, K_ERR_LEVEL_ERROR, K_ERR_EXIST)
#define K_ERR_VO_UNEXIST                             K_DEF_ERR(K_ID_VO, K_ERR_LEVEL_ERROR, K_ERR_UNEXIST)
#define K_ERR_VO_NULL_PTR                            K_DEF_ERR(K_ID_VO, K_ERR_LEVEL_ERROR, K_ERR_NULL_PTR)
#define K_ERR_VO_NOT_CONFIG                          K_DEF_ERR(K_ID_VO, K_ERR_LEVEL_ERROR, K_ERR_NOT_CONFIG)
#define K_ERR_VO_NOT_SUPPORT                         K_DEF_ERR(K_ID_VO, K_ERR_LEVEL_ERROR, K_ERR_NOT_SUPPORT)
#define K_ERR_VO_NOT_PERM                            K_DEF_ERR(K_ID_VO, K_ERR_LEVEL_ERROR, K_ERR_NOT_PERM)
#define K_ERR_VO_NOMEM                               K_DEF_ERR(K_ID_VO, K_ERR_LEVEL_ERROR, K_ERR_NOMEM)
#define K_ERR_VO_NOBUF                               K_DEF_ERR(K_ID_VO, K_ERR_LEVEL_ERROR, K_ERR_NOBUF)
#define K_ERR_VO_BUF_EMPTY                           K_DEF_ERR(K_ID_VO, K_ERR_LEVEL_ERROR, K_ERR_BUF_EMPTY)
#define K_ERR_VO_BUF_FULL                            K_DEF_ERR(K_ID_VO, K_ERR_LEVEL_ERROR, K_ERR_BUF_FULL)
#define K_ERR_VO_NOTREADY                            K_DEF_ERR(K_ID_VO, K_ERR_LEVEL_ERROR, K_ERR_NOTREADY)
#define K_ERR_VO_BADADDR                             K_DEF_ERR(K_ID_VO, K_ERR_LEVEL_ERROR, K_ERR_BADADDR)
#define K_ERR_VO_BUSY                                K_DEF_ERR(K_ID_VO, K_ERR_LEVEL_ERROR, K_ERR_BUSY)

/* dsi part */
typedef enum
{
    K_VO_LP_MODE,
    K_VO_HS_MODE,
} k_vo_dsi_cmd_mode;

typedef enum
{
    K_DSI_1LAN = 0,
    K_DSI_2LAN = 1,
    K_DSI_4LAN = 3,
} k_dsi_lan_num;

typedef enum
{
    K_BURST_MODE = 0,
    K_NON_BURST_MODE_WITH_SYNC_EVENT = 1,
    K_NON_BURST_MODE_WITH_PULSES = 2,
} k_dsi_work_mode;

typedef struct
{
    k_u32 pclk;
    k_u32 phyclk;
    k_u32 htotal;
    k_u32 hdisplay;
    k_u32 hsync_len;
    k_u32 hback_porch;
    k_u32 hfront_porch;
    k_u32 vtotal;
    k_u32 vdisplay;
    k_u32 vsync_len;
    k_u32 vback_porch;
    k_u32 vfront_porch;
} k_vo_display_resolution;

typedef struct
{
    k_u32 n;
    k_u32 m;
    k_u32 voc;
    k_u32 phy_lan_num;
    k_u32 hs_freq;
} k_vo_mipi_phy_attr;

typedef struct
{
    k_vo_display_resolution resolution;
    k_dsi_lan_num lan_num;
    k_vo_dsi_cmd_mode cmd_mode;
    k_dsi_work_mode work_mode;
    k_u32 lp_div;
} k_vo_dsi_attr;

typedef enum
{
    K_VO_INTF_MIPI = 0,
} k_vo_intf_type;

typedef enum
{
    K_VO_OUT_1080P30,
    K_VO_OUT_1080P60,
} k_vo_intf_sync;

typedef struct
{

    k_u32  bg_color;        //yuv
    k_vo_intf_type intf_type;
    k_vo_intf_sync intf_sync;
    k_vo_display_resolution *sync_info;          //

} k_vo_pub_attr;

typedef struct
{
    k_u32 hsync_start;
    k_u32 hsync_stop;
    k_u32 hsync1_start;
    k_u32 hsync1_stop;
    k_u32 hsync2_start;
    k_u32 hsync2_stop;
    k_u32 vsync1_start;
    k_u32 vsync1_stop;
    k_u32 vsync2_start;
    k_u32 vsync2_stop;
} k_vo_sync_attr;

/* old vo part */
typedef struct
{
    k_u32 draw_en;
    k_u32 line_x_start;
    k_u32 line_y_start;
    k_u32 line_x_end;
    k_u32 line_y_end;
    k_u32 frame_num;
} k_vo_draw_frame;

typedef enum
{
    K_VO_LAYER0 = 0,
    K_VO_LAYER1 = 1,
    K_VO_LAYER2 = 2,
    K_MAX_VO_LAYER_NUM,
} k_vo_layer __attribute__((deprecated("should use k_vo_layer_id")));

typedef enum
{
    K_VO_OSD0 = 0,
    K_VO_OSD1 = 1,
    K_VO_OSD2 = 2,
    K_VO_OSD3 = 3,
    K_MAX_VO_OSD_NUM,
} k_vo_osd __attribute__((deprecated("should use k_vo_layer_id")));

/* for wbc */
typedef struct {
    k_u32 blk_cnt;     // Number of VB blocks to allocate in the pool
} k_vo_wbc_attr;

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
