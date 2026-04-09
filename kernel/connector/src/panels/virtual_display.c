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

#include <stdbool.h>
#include <string.h>

#include "k_connector_comm.h"

#include "rtthread.h"
#include "rvv_ops.h"

#include "connector_panel.h"

static const struct panel_desc virtdev_panel_desc;
struct panel_desc              virtdev_runtime_desc;

k_s32 virtdev_calculate_timings(k_u32 hdisplay, k_u32 vdisplay, k_u32 fps, struct panel_desc* runtime_desc)
{
    k_u32 htotal, vtotal;
    k_u32 pclk_hz, pclk_khz;

    // 1. Basic Validation
    if (hdisplay < 64 || hdisplay > 4096 || vdisplay < 64 || vdisplay > 4096 || fps == 0) {
        return -1;
    }

    if (fps > 100) {
        fps = 100;
    }

    // 2. Minimum blanking: htotal >= hactive+100, vtotal >= vactive+50
    k_u32 htotal_min = hdisplay + 100;
    k_u32 vtotal_min = vdisplay + 50;

    // 3. Find pclk = 594MHz / div that best matches the target fps.
    //    pclk = htotal * vtotal * fps, so we need:
    //    div = 594MHz / (htotal * vtotal * fps)
    //
    //    Strategy: compute ideal pclk, find nearest 594MHz/div,
    //    then adjust vtotal so that pclk / (htotal * vtotal) is close to fps.
    k_u64 ideal_pclk = (k_u64)htotal_min * vtotal_min * fps;
    k_u32 div;

    if (ideal_pclk == 0) {
        return -1;
    }

    // Find the VO divider closest to ideal
    div = (k_u32)((VO_PIXEL_CLOCK_HZ + ideal_pclk / 2) / ideal_pclk);
    if (div == 0) {
        div = 1;
    }

    pclk_hz  = VO_PIXEL_CLOCK_HZ / div;
    pclk_khz = pclk_hz / 1000;

    if (pclk_khz == 0) {
        return -1;
    }

    // 4. With this pclk, derive vtotal for exact frame rate match:
    //    vtotal = pclk / (htotal_min * fps)
    //    If not exact, pick the floor and accept slightly higher fps.
    htotal = htotal_min;
    vtotal = pclk_hz / ((k_u64)htotal * fps);

    if (vtotal < vtotal_min) {
        // vtotal too small — increase htotal to compensate
        // vtotal_min * htotal * fps <= pclk => htotal <= pclk / (vtotal_min * fps)
        htotal = pclk_hz / ((k_u64)vtotal_min * fps);
        if (htotal < htotal_min) {
            // pclk too low for this resolution+fps, try a higher pclk (lower div)
            if (div > 1) {
                div--;
                pclk_hz  = VO_PIXEL_CLOCK_HZ / div;
                pclk_khz = pclk_hz / 1000;
                htotal   = htotal_min;
                vtotal   = pclk_hz / ((k_u64)htotal * fps);
                if (vtotal < vtotal_min) {
                    htotal = pclk_hz / ((k_u64)vtotal_min * fps);
                    vtotal = vtotal_min;
                }
            }
            if (htotal < htotal_min) {
                htotal = htotal_min;
                vtotal = vtotal_min;
            }
        } else {
            vtotal = vtotal_min;
        }
    }

    // 5. Safety: ensure minimums
    if (htotal < htotal_min) {
        htotal = htotal_min;
    }
    if (vtotal < vtotal_min) {
        vtotal = vtotal_min;
    }

    // 6. Fill the runtime descriptor
    rvv_memcpy(runtime_desc, &virtdev_panel_desc, sizeof(struct panel_desc));

    runtime_desc->timing.pclk_khz = pclk_khz;
    runtime_desc->timing.hactive  = hdisplay;
    runtime_desc->timing.vactive  = vdisplay;

    // 7. Distribute blanking
    k_u32 hblank = htotal - hdisplay;
    k_u32 vblank = vtotal - vdisplay;

    // Horizontal distribution
    runtime_desc->timing.hsync_len = (hblank * 2) / 8;
    if (runtime_desc->timing.hsync_len < 4) {
        runtime_desc->timing.hsync_len = 4;
    }
    runtime_desc->timing.hback_porch  = (hblank * 4) / 8;
    runtime_desc->timing.hfront_porch = hblank - (runtime_desc->timing.hsync_len + runtime_desc->timing.hback_porch);

    // Vertical distribution
    runtime_desc->timing.vsync_len = (vblank * 2) / 8;
    if (runtime_desc->timing.vsync_len < 4) {
        runtime_desc->timing.vsync_len = 4;
    }
    runtime_desc->timing.vback_porch  = (vblank * 3) / 8;
    runtime_desc->timing.vfront_porch = vblank - (runtime_desc->timing.vsync_len + runtime_desc->timing.vback_porch);

#if 0
    {
        k_vo_timing *timing = &runtime_desc->timing;

        rt_kprintf("[VIRT Timing Config]: %d fps\n", fps);
        rt_kprintf("  Pixel Clk  : %u kHz\n", timing->pclk_khz);
        rt_kprintf("  H-Active   : %u\n", timing->hactive);
        rt_kprintf("  H-Sync Len : %u\n", timing->hsync_len);
        rt_kprintf("  H-Back P.  : %u\n", timing->hback_porch);
        rt_kprintf("  H-Front P. : %u\n", timing->hfront_porch);
        rt_kprintf("  V-Active   : %u\n", timing->vactive);
        rt_kprintf("  V-Sync Len : %u\n", timing->vsync_len);
        rt_kprintf("  V-Back P.  : %u\n", timing->vback_porch);
        rt_kprintf("  V-Front P. : %u\n", timing->vfront_porch);
    }
#endif

    return 0;
}

static int virtdev_init(const struct panel_desc* desc)
{
    (void)desc;

    return 0;
}

static const struct panel_bus_ops virtdev_bus_ops = {
    .init    = NULL,
    .enable  = NULL,
    .disable = NULL,
};

static const struct panel_ops virtdev_ops = {
    .init      = virtdev_init,
    .power_off = NULL,
};

static const struct panel_desc virtdev_panel_desc = {
    .name = "virtdev",
    .connector_type = VIRTUAL_DISPLAY_DEVICE,
    .bus_type = PANEL_BUS_NONE,

    .timing = {
         .pclk_khz = 0,
         .hactive = 0,
         .hsync_len = 0,
         .hback_porch = 0,
         .hfront_porch = 0,
         .vactive = 0,
         .vsync_len = 0,
         .vback_porch = 0,
         .vfront_porch = 0,
      },

    .bg_color = PANEL_BG_COLOR_WHITE,

    .bus.dsi = {
        // nothing meaningful since bus_type is NONE, but set defaults anyway
        .lanes = K_DSI_4LANE,
        .cmd_mode = K_DSI_CMD_LP_MODE,
        .video_mode = K_DSI_VIDEO_BURST_MODE,
        .vc_id = 0,
    },

    .gpio = {
        .reset_pin = -1,
        .backlight_pin = -1,
        .reset_delay_ms = 0,
        .backlight_delay_ms = 0,
        .reset_active_low = K_FALSE,
        .backlight_active_low = K_FALSE,
    },

    .bus_ops = &virtdev_bus_ops,
    .ops = &virtdev_ops,
};

static const struct panel_desc* virtdev_panel_variants[] = {
    &virtdev_panel_desc,
    NULL,
};

struct panel_drv virtual_display_drv = {
    .connector_name = "virtdev",
    .panel_variants = virtdev_panel_variants,
    .active_panel   = &virtdev_panel_desc,
};
