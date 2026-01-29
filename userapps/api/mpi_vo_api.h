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
#ifndef __MPI_VO_API_H__
#define __MPI_VO_API_H__

#include "k_type.h"
#include "k_vo_comm.h"

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

#ifdef __cplusplus
extern "C" {
#endif /* End of #ifdef __cplusplus */

k_s32 kd_mpi_vo_set_dev_attr(const k_vo_dev_attr *attr);
k_s32 kd_mpi_vo_get_dev_attr(k_vo_dev_attr *attr);
k_s32 kd_mpi_vo_clr_dev_attr(void);

k_s32 kd_mpi_vo_get_resolution(k_vo_size *resolution);

k_s32 kd_mpi_vo_set_layer_mix_order(k_u64 mix_prio);

k_s32 kd_mpi_vo_set_layer_attr(k_vo_layer_id layer_id, const k_vo_layer_attr* attr);
k_s32 kd_mpi_vo_get_layer_attr(k_vo_layer_id layer_id, k_vo_layer_attr* attr);

k_s32 kd_mpi_vo_update_layer_position(k_vo_layer_id layer_id, k_u32 offset_x, k_u32 offset_y);
k_s32 kd_mpi_vo_update_layer_image_size(k_vo_layer_id layer_id, k_u32 img_width, k_u32 img_height);
k_s32 kd_mpi_vo_update_layer_pixel_format(k_vo_layer_id layer_id, k_pixel_format pixel_format);
k_s32 kd_mpi_vo_update_layer_func(k_vo_layer_id layer_id, k_gdma_rotation_e func);
k_s32 kd_mpi_vo_update_layer_alpha(k_vo_layer_id layer_id, k_u8 alpha);

k_s32 kd_mpi_vo_enable_layer(k_vo_layer_id layer_id);
k_s32 kd_mpi_vo_disable_layer(k_vo_layer_id layer_id);

k_s32 kd_mpi_vo_insert_frame(k_vo_layer_id layer_id, const k_video_frame_info* vf_info);

k_s32 kd_mpi_vo_set_wbc_attr(k_vo_wbc_attr* attr);

k_s32 kd_mpi_vo_enable_wbc(void);
k_s32 kd_mpi_vo_disable_wbc(void);

k_s32 kd_mpi_vo_get_stats(k_bool *is_running);

k_s32 kd_mpi_wbc_dump_frame(k_video_frame_info* vf_info, k_u32 timeout_ms);
k_s32 kd_mpi_wbc_dump_release(const k_video_frame_info* vf_info);

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif
