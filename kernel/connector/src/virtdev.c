/* Copyright (c) 2023, Canaan Bright Sight Co., Ltd
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

#include "connector_dev.h"
#include "drv_gpio.h"
#include "io.h"
#include "k_connector_comm.h"
#include "k_vo_comm.h"

static k_s32 virtdev_power_on(void* ctx, k_s32 on) { return 0; }

static k_s32 virtdev_vo_resolution_init(k_vo_display_resolution* resolution, k_u32 bg_color, k_u32 intr_line)
{
    k_vo_pub_attr attr;

    memset(&attr, 0, sizeof(k_vo_pub_attr));
    attr.bg_color  = bg_color;
    attr.sync_info = resolution;

    connector_set_vo_init();
    connector_set_vtth_intr(1, intr_line);
    connector_set_vo_param(&attr);
    connector_set_vo_enable();

    return 0;
}

static k_u32 virtdev_correct_pclk(k_u32 pclk)
{
    const k_u32 DIVISOR = 594000000; // 594 million
    k_u32       ratio;
    k_u32       corrected_value;

    if (pclk <= 0) {
        return 0;
    }

    if (pclk % DIVISOR == 0) {
        return pclk;
    }

    // Calculate the initial ratio
    ratio = DIVISOR / pclk;

    // Adjust the ratio until DIVISOR / ratio is divisible by 1000
    while (1) {
        if (ratio == 0)
            return 0; // Guard against division by zero
        corrected_value = DIVISOR / ratio;

        if (corrected_value % 1000 == 0) {
            return corrected_value;
        }

        ratio++;
        if (ratio > 1000)
            return pclk; // Safety break
    }
}

static k_s32 virtdev_init(void* ctx, k_connector_info* info)
{
    k_vo_display_resolution* resolution = &info->resolution;
    // 使用pclk字段表示帧率，如果设置为非0则自动计算分频和行场信息，否则使用结构体中的配置
    if (resolution->pclk != 0) {
        uint32_t hact, htotal, htotal_min, htotal_max = 4096;
        uint32_t vact, vtotal, vtotal_min, vtotal_max = 4096;
        uint32_t fps, pixtotal_in, corrected_pixtotal, pixclk_div, intr_line;
        uint32_t corrected_pclk;

        hact = resolution->hdisplay;
        vact = resolution->vdisplay;
        fps  = resolution->pclk;
        if (hact < 64 || hact > 4096 || vact < 64 || vact > 4096 || fps > 200) {
            return -1;
        }

        // Use the original min calculations for the *input* PCLK estimation
        intr_line  = 32 - __builtin_clz(vact);
        vtotal_min = (1UL << intr_line) + 15;
        if (vtotal_min < (vact + 96)) {
            vtotal_min = vact + 96;
        }
        htotal_min = hact + 96;

        // Calculate the required PCLK_in using the estimated min totals
        pixtotal_in      = vtotal_min * htotal_min;
        uint32_t pclk_in = pixtotal_in * fps;

        // 1. Correct PCLK (Debugger's logic)
        corrected_pclk = virtdev_correct_pclk(pclk_in);
        if (corrected_pclk == 0) {
            return -1;
        }

        // 2. Calculate pixclk_div (Debugger's logic)
        pixclk_div = (594000000 / corrected_pclk) - 1;
        if (pixclk_div == 0) // Should not happen if corrected_pclk is correct
            return -1;
        if (pixclk_div > 128)
            pixclk_div = 128; // Clamp as in original virtdev.c

        // 3. Recalculate pixel total based on the corrected PCLK
        corrected_pixtotal = corrected_pclk / fps;

        // 4. Find htotal/vtotal pair (Original virtdev.c loop logic)
        for (vtotal = vtotal_min; vtotal < htotal_max; vtotal++) {
            htotal = corrected_pixtotal / vtotal;
            if (htotal <= htotal_max)
                break;
            else if (htotal < htotal_min)
                return -1;
        }

        // 5. Update info structure
        info->pixclk_div         = pixclk_div;
        info->intr_line          = intr_line;
        resolution->htotal       = htotal;
        resolution->hsync_len    = 32;
        resolution->hback_porch  = 32;
        resolution->hfront_porch = htotal - hact - 64;
        resolution->vtotal       = vtotal;
        resolution->vsync_len    = 32;
        resolution->vback_porch  = 32;
        resolution->vfront_porch = vtotal - vact - 64;
        resolution->pclk         = corrected_pclk / 1000; // Store corrected pclk/1000
    }
    connector_set_pixclk(info->pixclk_div);
    connector_set_cmd_buff_num(info->buff_num);
    virtdev_vo_resolution_init(resolution, info->bg_color, info->intr_line);

    return 0;
}

static k_s32 virtdev_get_chip_id(void* ctx, k_u32* chip_id)
{
    *chip_id = 0xFFFFFFFF;
    return 0;
}

static k_s32 virtdev_conn_check(void* ctx, k_s32* conn)
{
    *conn = 1;
    return 0;
}

struct connector_driver_dev virtdev_connector_drv = {
    .connector_name = "virtdev",
    .connector_func = {
        .connector_power = virtdev_power_on,
        .connector_init = virtdev_init,
        .connector_get_chip_id = virtdev_get_chip_id,
        .connector_conn_check = virtdev_conn_check,
    },
};
