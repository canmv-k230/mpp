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
#include "kd_display.h"

#include <stdio.h>
#include <string.h>

static int              info_valid          = 0;
static k_connector_info curr_connector_info = {};

k_s32 kd_display_init_ex(k_connector_type type, k_u32 width, k_u32 height, k_gdma_rotation_e rotate, k_u8 fps)
{
    k_s32             ret;
    k_s32             connector_fd;
    k_connector_info  connector_info;
    k_gdma_rotation_e _rotate = rotate;

    memset(&connector_info, 0, sizeof(connector_info));
    ret = kd_mpi_get_connector_info(type, &connector_info);
    if (K_SUCCESS != ret) {
        printf("[fw_display]: failed to get connector info type=%d, ret=%d\n", type, ret);
        return ret;
    }

    if (type == VIRTUAL_DISPLAY_DEVICE) {
        _rotate = 0; // Force no rotate when virtual

        connector_info.resolution.hdisplay = width;
        connector_info.resolution.vdisplay = height;
        connector_info.resolution.pclk     = fps;
    } else {
        /* Auto Rotation Logic for Physical Devices */
        if (width != 0 && height != 0 && _rotate == 0) {
            k_u32 phys_w, phys_h;
            // Get physical panel resolution from the DB/driver
            phys_w = connector_info.resolution.hdisplay;
            phys_h = connector_info.resolution.vdisplay;

            // If requested resolution is the transpose of physical resolution, auto-rotate 90
            if (width == phys_h && height == phys_w) {
                _rotate = GDMA_ROTATE_DEGREE_90;
                printf("[fw_display]: Auto-rotating to 90 deg (Req: %dx%d, Phys: %dx%d)\n", width, height, phys_w, phys_h);
            }
        }
    }

    connector_fd = kd_mpi_connector_open(connector_info.connector_name);
    if (connector_fd < 0) {
        printf("[fw_display]:: connector open %s failed\n", connector_info.connector_name);
        return K_ERR_VO_NOTREADY;
    }

    ret = kd_mpi_connector_power_set(connector_fd, 1);
    if (K_SUCCESS != ret) {
        printf("[fw_display]:: kd_mpi_connector_power_set failed, ret=%d\n", ret);
        kd_mpi_connector_close(connector_fd);
        return ret;
    }

    ret = kd_mpi_connector_init(connector_fd, connector_info);
    if (K_SUCCESS != ret) {
        printf("[fw_display]:: kd_mpi_connector_init failed, ret=%d\n", ret);
        kd_mpi_connector_close(connector_fd);
        return ret;
    }

    kd_mpi_connector_close(connector_fd);

    // Configure VO hardware device attributes (Rotation)
    k_vo_dev_attr dev_attr;
    memset(&dev_attr, 0, sizeof(dev_attr));
    dev_attr.dev_rot_flg = _rotate;

    if (0x00 != (ret = kd_mpi_vo_set_dev_attr(&dev_attr))) {
        printf("[fw_display]:: kd_mpi_vo_set_dev_attr failed, ret=%d\n", ret);
        return ret;
    }

    info_valid = 1;
    memcpy(&curr_connector_info, &connector_info, sizeof(curr_connector_info));

    return K_SUCCESS;
}

k_s32 kd_display_deinit(void)
{
    k_s32 ret = K_SUCCESS;

    k_s32 connector_fd;

    for (k_vo_layer_id layer = K_VO_LAYER_VIDEO0; layer < K_MAX_VO_LAYER_NR; layer++) {
        kd_mpi_vo_disable_layer(layer);
    }

    kd_mpi_vo_disable_wbc();

    kd_mpi_vo_clr_dev_attr();

    connector_fd = kd_mpi_connector_open(curr_connector_info.connector_name);
    if (0 <= connector_fd) {
        ret = kd_mpi_connector_power_set(connector_fd, 0);
        if (K_SUCCESS != ret) {
            printf("[fw_display]:: kd_mpi_connector_power_set failed, ret=%d\n", ret);
        }
        kd_mpi_connector_close(connector_fd);
    } else {
        ret = K_FAILED;
    }

    info_valid = 0;
    memset(&curr_connector_info, 0, sizeof(curr_connector_info));

    return ret;
}

k_s32 kd_display_get_connector_info(k_connector_info* info)
{
    if (0x00 == info_valid) {
        return -1;
    }

    memcpy(info, &curr_connector_info, sizeof(k_connector_info));

    return 0;
}

k_s32 kd_display_get_resolution(k_u32* width, k_u32* height)
{
    k_vo_size size;

    k_s32 ret = K_FAILED;

    ret = kd_mpi_vo_get_resolution(&size);

    if (width) {
        *width = size.width;
    }

    if (height) {
        *height = size.height;
    }

    return ret;
}

k_s32 kd_display_layer_configure_ex(k_vo_layer_id layer, k_pixel_format pixel_format, k_u32 width, k_u32 height, k_u32 offset_x,
                                    k_u32 offset_y, k_u8 alpha, k_gdma_rotation_e rotate, k_u8 rot_buf_nr, k_u8 rot_buf_bpp)
{
    k_u32 layer_width  = width;
    k_u32 layer_height = height;

    if ((0x00 == layer_width) || (0x00 == layer_height)) {
        if (0x00 != kd_display_get_resolution(&layer_width, &layer_height)) {
            return -1;
        }
    }

    const k_vo_layer_attr attr = {
        .layer_id     = layer,
        .position     = { offset_x, offset_y },
        .img_size     = { layer_width, layer_height },
        .pixel_format = pixel_format,
        .func         = rotate,
        .global_alpha = alpha,
        .rot_buf_nr   = rot_buf_nr,
        .rot_buf_bpp  = rot_buf_bpp,
    };

    return kd_mpi_vo_set_layer_attr((layer), &attr);
}

k_s32 kd_display_control_backlight(int mode, int value)
{
    printf("TODO: %s\n", __func__);
    return K_FAILED;
}
