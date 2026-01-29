/* Copyright (c) 2026, Canaan Bright Sight Co., Ltd
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
#pragma once

#include <stdint.h>

#include "k_type.h"
#include "mpi_connector_api.h"
#include "mpi_vo_api.h"

#include "plooc.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
Video Layer supported pixel format

    PIXEL_FORMAT_YUV_SEMIPLANAR_420

OSD Layer supported pixel format

    PIXEL_FORMAT_RGB_565
    PIXEL_FORMAT_BGR_565
    PIXEL_FORMAT_RGB_565_LE
    PIXEL_FORMAT_BGR_565_LE

    PIXEL_FORMAT_RGB_888
    PIXEL_FORMAT_BGR_888

    PIXEL_FORMAT_ARGB_8888
    PIXEL_FORMAT_ABGR_8888
    PIXEL_FORMAT_RGBA_8888
    PIXEL_FORMAT_BGRA_8888

    PIXEL_FORMAT_ABGR_4444
    PIXEL_FORMAT_ARGB_4444
    PIXEL_FORMAT_ARGB_1555
    PIXEL_FORMAT_ABGR_1555

    PIXEL_FORMAT_RGB_MONOCHROME_8BPP
*/

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// KD Display APIs ////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/* Display */
/**
 * width & height & fps only used when type is VIRTUAL_DISPLAY_DEVICE
 */
// k_s32 kd_display_init(k_connector_type type, k_u32 width = 0, k_u32 height = 0, k_gdma_rotation_e rotate = GDMA_ROTATE_NONE,
// k_u8 fps = 100);
#define kd_display_init(...) __PLOOC_EVAL(KD_DISPLAY_INIT_NARGS_, __VA_ARGS__)(__VA_ARGS__)

k_s32 kd_display_init_ex(k_connector_type type, k_u32 width, k_u32 height, k_gdma_rotation_e rotate, k_u8 fps);

k_s32 kd_display_deinit(void);

// below api must call after kd_display_init
k_s32 kd_display_get_resolution(k_u32* width, k_u32* height);

k_s32 kd_display_get_connector_info(k_connector_info* info);

k_s32 kd_display_control_backlight(int mode, int value);

/* Layer Mix */
static inline k_s32 kd_display_set_layer_mix_order(k_u64 mix_prio) { return kd_mpi_vo_set_layer_mix_order(mix_prio); }

/* Layer */

static inline k_s32 kd_display_layer_update_position(k_vo_layer_id layer_id, k_u32 offset_x, k_u32 offset_y)
{
    return kd_mpi_vo_update_layer_position(layer_id, offset_x, offset_y);
}

static inline k_s32 kd_display_layer_update_layer_image_size(k_vo_layer_id layer_id, k_u32 img_width, k_u32 img_height)
{
    return kd_mpi_vo_update_layer_image_size(layer_id, img_width, img_height);
}

static inline k_s32 kd_display_layer_update_pixel_format(k_vo_layer_id layer_id, k_pixel_format pixel_format)
{
    return kd_mpi_vo_update_layer_pixel_format(layer_id, pixel_format);
}

static inline k_s32 kd_display_layer_update_func(k_vo_layer_id layer_id, k_gdma_rotation_e func)
{
    return kd_mpi_vo_update_layer_func(layer_id, func);
}

static inline k_s32 kd_display_layer_update_alpha(k_vo_layer_id layer_id, k_u8 alpha)
{
    return kd_mpi_vo_update_layer_alpha(layer_id, alpha);
}

// k_s32 kd_display_layer_configure(k_vo_layer_id layer, k_pixel_format pixel_format, k_u32 width = 0, k_u32 height = 0,
//                                  k_u32 offset_x = 0, k_u32 offset_y = 0, k_u8 alpha = 255, k_gdma_rotation_e rotate =
//                                  GDMA_ROTATE_NONE, k_u8 rot_buf_nr = 2, k_u8 rot_buf_bpp = 0);

#define kd_display_layer_configure(...) __PLOOC_EVAL(KD_DISPLAY_CONFIG_LAYR_NARGS_, __VA_ARGS__)(__VA_ARGS__)

k_s32 kd_display_layer_configure_ex(k_vo_layer_id layer, k_pixel_format pixel_format, k_u32 width, k_u32 height, k_u32 offset_x,
                                    k_u32 offset_y, k_u8 alpha, k_gdma_rotation_e rotate, k_u8 rot_buf_nr, k_u8 rot_buf_bpp);

static inline k_s32 kd_display_layer_set_attr(k_vo_layer_id layer_id, k_vo_layer_attr* attr)
{
    return kd_mpi_vo_set_layer_attr(layer_id, attr);
}

static inline k_s32 kd_display_layer_get_attr(k_vo_layer_id layer_id, k_vo_layer_attr* attr)
{
    return kd_mpi_vo_get_layer_attr(layer_id, attr);
}

static inline k_s32 kd_display_layer_enable(k_vo_layer_id layer_id) { return kd_mpi_vo_enable_layer(layer_id); }

static inline k_s32 kd_display_layer_disable(k_vo_layer_id layer_id) { return kd_mpi_vo_disable_layer(layer_id); }

static inline k_s32 kd_display_layer_push_frame(k_vo_layer_id layer_id, const k_video_frame_info* vf_info)
{
    return kd_mpi_vo_insert_frame(layer_id, vf_info);
}

/* Write-back (WBC) */

// k_s32 kd_display_write_back_config(k_u32 blk_cnt = 3);
#define kd_display_wbc_configure(...) __PLOOC_EVAL(KD_DISPLAY_CONFIG_WBC_NARGS_, __VA_ARGS__)(__VA_ARGS__)

static inline k_s32 kd_display_wbc_set_attr(k_vo_wbc_attr* attr) { return kd_mpi_vo_set_wbc_attr(attr); }

static inline k_s32 kd_display_wbc_enable() { return kd_mpi_vo_enable_wbc(); }

static inline k_s32 kd_display_wbc_disable() { return kd_mpi_vo_disable_wbc(); }

static inline k_s32 kd_display_wbc_stats(k_bool* is_running) { return kd_mpi_vo_get_stats(is_running); }

// k_s32 kd_display_wbc_dump_frame(k_video_frame_info* vf_info, k_u32 timeout_ms = 1000);
#define kd_display_wbc_dump_frame(...) __PLOOC_EVAL(KD_DISPLAY_WBC_DUMP_FRAME_, __VA_ARGS__)(__VA_ARGS__)

static inline k_s32 kd_display_wbc_release_frame(k_video_frame_info* vf_info) { return kd_mpi_wbc_dump_release(vf_info); }

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Marco Helpers //////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define KD_DISPLAY_INIT_NARGS_1(type) (__extension__({ kd_display_init_ex((type), 0, 0, GDMA_ROTATE_NONE, 60); }))

#define KD_DISPLAY_INIT_NARGS_3(type, width, height)                                                                           \
    (__extension__({ kd_display_init_ex((type), (width), (height), GDMA_ROTATE_NONE, 60); }))

#define KD_DISPLAY_INIT_NARGS_4(type, width, height, rotate)                                                                   \
    (__extension__({ kd_display_init_ex((type), (width), (height), (rotate), 60); }))

#define KD_DISPLAY_INIT_NARGS_5(type, width, height, rotate, fps)                                                              \
    (__extension__({ kd_display_init_ex((type), (width), (height), (rotate), (fps)); }))

#define KD_DISPLAY_CONFIG_LAYR_NARGS_4(layer, pixel_format, width, height)                                                     \
    (__extension__(                                                                                                            \
        { kd_display_layer_configure_ex((layer), (pixel_format), (width), (height), 0, 0, 255, GDMA_ROTATE_NONE, 2, 0); }))

#define KD_DISPLAY_CONFIG_LAYR_NARGS_5(layer, pixel_format, width, height, offset_x)                                           \
    (__extension__({                                                                                                           \
        kd_display_layer_configure_ex((layer), (pixel_format), (width), (height), (offset_x), 0, 255, GDMA_ROTATE_NONE, 2, 0); \
    }))

#define KD_DISPLAY_CONFIG_LAYR_NARGS_6(layer, pixel_format, width, height, offset_x, offset_y)                                 \
    (__extension__({                                                                                                           \
        kd_display_layer_configure_ex((layer), (pixel_format), (width), (height), (offset_x), (offset_y), 255,                 \
                                      GDMA_ROTATE_NONE, 2, 0);                                                                 \
    }))

#define KD_DISPLAY_CONFIG_LAYR_NARGS_7(layer, pixel_format, width, height, offset_x, offset_y, alpha)                          \
    (__extension__({                                                                                                           \
        kd_display_layer_configure_ex((layer), (pixel_format), (width), (height), (offset_x), (offset_y), (alpha), 0, 2, 0);   \
    }))

#define KD_DISPLAY_CONFIG_LAYR_NARGS_8(layer, pixel_format, width, height, offset_x, offset_y, alpha, rotate)                  \
    (__extension__({                                                                                                           \
        kd_display_layer_configure_ex((layer), (pixel_format), (width), (height), (offset_x), (offset_y), (alpha), (rotate),   \
                                      2, 0);                                                                                   \
    }))

#define KD_DISPLAY_CONFIG_LAYR_NARGS_9(layer, pixel_format, width, height, offset_x, offset_y, alpha, rotate, rot_buf_nr)      \
    (__extension__({                                                                                                           \
        kd_display_layer_configure_ex((layer), (pixel_format), (width), (height), (offset_x), (offset_y), (alpha), (rotate),   \
                                      (rot_buf_nr), 0);                                                                        \
    }))

#define KD_DISPLAY_CONFIG_LAYR_NARGS_10(layer, pixel_format, width, height, offset_x, offset_y, alpha, rotate, rot_buf_nr,     \
                                        rot_buf_bpp)                                                                           \
    (__extension__({                                                                                                           \
        kd_display_layer_configure_ex((layer), (pixel_format), (width), (height), (offset_x), (offset_y), (alpha), (rotate),   \
                                      (rot_buf_nr), (rot_buf_bpp));                                                            \
    }))

#define KD_DISPLAY_CONFIG_WBC_NARGS_0()                                                                                        \
    (__extension__({                                                                                                           \
        k_vo_wbc_attr attr = {                                                                                                 \
            .blk_cnt = 3,                                                                                                      \
        };                                                                                                                     \
        kd_mpi_vo_set_wbc_attr(&attr);                                                                                         \
    }))

#define KD_DISPLAY_CONFIG_WBC_NARGS_1(_blk_cnt)                                                                                \
    (__extension__({                                                                                                           \
        k_vo_wbc_attr attr = {                                                                                                 \
            .blk_cnt = (_blk_cnt),                                                                                             \
        };                                                                                                                     \
        kd_mpi_vo_set_wbc_attr(&attr);                                                                                         \
    }))

#define KD_DISPLAY_WBC_DUMP_FRAME_1(vf_info) (__extension__({ kd_mpi_wbc_dump_frame((vf_info), 1000); }))

#define KD_DISPLAY_WBC_DUMP_FRAME_2(vf_info, timeout_ms) (__extension__({ kd_mpi_wbc_dump_frame((vf_info), (timeout_ms)); }))

#ifdef __cplusplus
}
#endif
