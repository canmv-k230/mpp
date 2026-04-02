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
#include "rtthread.h"

#include "connector_panel.h"

#include "k_autoconf_comm.h"

static int nt35532_init(const struct panel_desc* desc)
{
    /* clang-format off */
    const k_u8 init_cmds[] = {
        CONNECTOR_CMD_SEQUENCE(0x05, 50, 0x01),
        CONNECTOR_CMD_SEQUENCE(0x15, 0, 0xFF, 0x00),
        CONNECTOR_CMD_SEQUENCE(0x15, 0, 0xD3, 52),
        CONNECTOR_CMD_SEQUENCE(0x15, 0, 0xD4, 124),
        CONNECTOR_CMD_SEQUENCE(0x15, 0, 0xD5, 20),
        CONNECTOR_CMD_SEQUENCE(0x15, 0, 0xD6, 120),
        CONNECTOR_CMD_SEQUENCE(0x15, 0, 0xD7, 0),
        CONNECTOR_CMD_SEQUENCE(0x05, 10, 0x11),
        CONNECTOR_CMD_SEQUENCE(0x05, 0, 0x29),
    };
    /* clang-format on */

    return dsi_send_cmd_sequence(desc, init_cmds, sizeof(init_cmds), K_FALSE);
}

static const struct panel_ops nt35532_ops = {
    .reset        = panel_generic_reset,
    .init         = nt35532_init,
    .power_off    = NULL,
    .read_chip_id = dsi_read_chip_id,
};

static const struct panel_desc nt35532_panel_desc = {
    .name = "nt35532_1080x1920",
    .connector_type = NT35532_1080_1920_DSI_V1,
    .bus_type = PANEL_BUS_DSI,

    .timing = {
        .pclk_khz = 148500,
        .hactive = 1080,
        .hsync_len = 20,
        .hback_porch = 20,
        .hfront_porch = 120,
        .vactive = 1920,
        .vsync_len = 50,
        .vback_porch = 52,
        .vfront_porch = 124,
    },

    .bg_color = PANEL_BG_COLOR_BLACK,

    .gpio = {
        .reset_pin = CONFIG_MPP_DSI_LCD_RESET_PIN,
        .backlight_pin = CONFIG_MPP_DSI_LCD_BACKLIGHT_PIN,
        .reset_delay_ms = 10,
        .backlight_delay_ms = 0,
        .reset_active_low = K_TRUE,
        .backlight_active_low = K_FALSE,
    },

    .bus.dsi = {
        .lanes = K_DSI_4LANE,
        .cmd_mode = K_DSI_CMD_LP_MODE,
        .video_mode = K_DSI_VIDEO_BURST_MODE,
        .vc_id = 0,
    },

    .bus_ops = &dsi_bus_ops,
    .ops = &nt35532_ops,
};

static const struct panel_desc* nt35532_panel_variants[] = {
    &nt35532_panel_desc,
    NULL,
};

struct panel_drv mipi_nt35532_drv = {
    .connector_name = "nt35532",
    .panel_variants = nt35532_panel_variants,
    .active_panel   = &nt35532_panel_desc,
};
