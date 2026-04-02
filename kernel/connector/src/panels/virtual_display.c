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

#include <string.h>
#include <stdbool.h>

#include "k_connector_comm.h"
#include "rtthread.h"

#include "connector_panel.h"

static const struct panel_desc virtdev_panel_desc;
struct panel_desc virtdev_runtime_desc;

k_s32 virtdev_calculate_timings(k_u32 hdisplay, k_u32 vdisplay, k_u32 fps,
                                struct panel_desc *runtime_desc)
{
    k_u32 htotal, vtotal;
    k_u32 pixtotal, pclk_req;
    k_u32 corrected_pclk = 0;

    // 1. Basic Validation
    if (hdisplay < 64 || hdisplay > 4096 || vdisplay < 64 || vdisplay > 4096 || fps == 0) {
        return -1;
    }

    if (100 <= fps) {
        fps = 100;
    }

    // 2. Start with conservative minimum blanking
    // Standard MIPI panels usually need at least 100 horizontal and 50 vertical pixels
    k_u32 htotal_base = hdisplay + 100;
    k_u32 vtotal_base = vdisplay + 50;

    // 3. Search for a valid Hardware Clock
    // We increment vtotal to find a combination where:
    // (htotal * vtotal * fps) matches a hardware DIVISOR of 594MHz
    k_u32 vtotal_search = vtotal_base;
    bool found = false;

    for (vtotal_search = vtotal_base; vtotal_search < vtotal_base + 100; vtotal_search++) {
        pclk_req = htotal_base * vtotal_search * fps;
        corrected_pclk = panel_correct_pclk(pclk_req);

        // Check if the corrected clock can result in our exact target FPS
        // with an integer total pixel count
        if (corrected_pclk > 0 && (corrected_pclk % fps == 0)) {
            pixtotal = corrected_pclk / fps;

            // Now derive the final htotal from the valid pixtotal
            htotal = pixtotal / vtotal_search;
            vtotal = vtotal_search;

            // Final safety check: htotal must still be >= hdisplay + min_blank
            if (htotal >= hdisplay + 40) {
                found = true;
                break;
            }
        }
    }

    if (!found) {
        rt_kprintf("Failed to find valid timing for %dx%d@%d\n", hdisplay, vdisplay, fps);
        return -1;
    }

    // 4. Fill the runtime descriptor
    memcpy(runtime_desc, &virtdev_panel_desc, sizeof(struct panel_desc));

    runtime_desc->timing.pclk_khz = corrected_pclk / 1000;
    runtime_desc->timing.hactive = hdisplay;
    runtime_desc->timing.vactive = vdisplay;

    // 5. Distribute Blanking (Porches)
    // Using a 1/8th distribution for sync to ensure stability
    k_u32 hblank = htotal - hdisplay;
    k_u32 vblank = vtotal - vdisplay;

    // Horizontal distribution
    runtime_desc->timing.hsync_len = (hblank * 2) / 8;
    runtime_desc->timing.hback_porch = (hblank * 4) / 8;
    runtime_desc->timing.hfront_porch = hblank - (runtime_desc->timing.hsync_len + runtime_desc->timing.hback_porch);

    // Vertical distribution
    runtime_desc->timing.vsync_len = (vblank * 2) / 8;
    if (runtime_desc->timing.vsync_len < 2) runtime_desc->timing.vsync_len = 2; 
    runtime_desc->timing.vback_porch = (vblank * 3) / 8;
    runtime_desc->timing.vfront_porch = vblank - (runtime_desc->timing.vsync_len + runtime_desc->timing.vback_porch);

    return 0;
}

static int virtdev_init(const struct panel_desc *desc)
{
    (void)desc;

    return 0;
}

static const struct panel_bus_ops virtdev_bus_ops = {
    .init = NULL,
    .enable = NULL,
    .disable = NULL,
};

static const struct panel_ops virtdev_ops = {
    .init = virtdev_init,
    .power_off = NULL,
};

static const struct panel_desc virtdev_panel_desc = {
    .name = "virtdev",
    .connector_type = VIRTUAL_DISPLAY_DEVICE,
    .bus_type = PANEL_BUS_DSI,

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
        .lanes = K_DSI_4LANE,
        .cmd_mode = K_DSI_CMD_LP_MODE,
        
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

static const struct panel_desc *virtdev_panel_variants[] = {
    &virtdev_panel_desc,
    NULL
};

struct panel_drv virtual_display_drv = {
    .connector_name = "virtdev",
    .panel_variants = virtdev_panel_variants,
    .active_panel = &virtdev_panel_desc,
};
