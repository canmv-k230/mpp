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

#include "connector_panel.h"

#include "k_autoconf_comm.h"

static int ili9806_init(const struct panel_desc* desc)
{
    const k_u8 init_cmds[] = {
        CONNECTOR_CMD_SEQUENCE(0x39, 0x00, 0xff, 0xff, 0x98, 0x06, 0x04, 0x01),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0x08, 0x10),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0x21, 0x01),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0x30, 0x02),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0x31, 0x01),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0x40, 0x16),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0x41, 0x33),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0x42, 0x00),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0x43, 0x85),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0x44, 0x8b),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0x45, 0x1b),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0x50, 0x78),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0x51, 0x78),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0x52, 0x00),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0x53, 0x60),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0x60, 0x07),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0x61, 0x00),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0x62, 0x07),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0x63, 0x00),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0xA0, 0x00),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0xa1, 0x0b),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0xa2, 0x12),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0xa3, 0x0c),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0xa4, 0x05),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0xa5, 0x0c),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0xa6, 0x07),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0xa7, 0x16),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0xa8, 0x06),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0xa9, 0x0a),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0xaa, 0x0f),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0xab, 0x06),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0xac, 0x0e),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0xad, 0x1a),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0xae, 0x12),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0xaf, 0x00),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0xB5, 127, 120, 20, 0),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0xc0, 0x00),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0xc1, 0x0b),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0xc2, 0x12),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0xc3, 0x0c),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0xc4, 0x05),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0xc5, 0x0c),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0xc6, 0x07),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0xc7, 0x16),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0xc8, 0x06),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0xc9, 0x0a),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0xca, 0x0f),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0xcb, 0x06),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0xcc, 0x0e),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0xcd, 0x1a),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0xce, 0x12),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0xcf, 0x00),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0xff, 0xff, 0x98, 0x06, 0x04, 0x06),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0x00, 0xa0),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0x01, 0x05),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0x02, 0x00),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0x03, 0x00),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0x04, 0x01),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0x05, 0x01),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0x06, 0x88),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0x07, 0x04),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0x08, 0x01),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0x09, 0x90),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0x0a, 0x04),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0x0b, 0x01),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0x0c, 0x01),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0x0d, 0x01),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0x0e, 0x00),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0x0f, 0x00),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0x10, 0x55),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0x11, 0x50),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0x12, 0x01),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0x13, 0x85),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0x14, 0x85),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0x15, 0xc0),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0x16, 0x0b),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0x17, 0x00),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0x18, 0x00),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0x19, 0x00),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0x1a, 0x00),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0x1b, 0x00),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0x1c, 0x00),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0x1d, 0x00),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0x20, 0x01),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0x21, 0x23),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0x22, 0x45),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0x23, 0x67),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0x24, 0x01),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0x25, 0x23),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0x26, 0x45),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0x27, 0x67),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0x30, 0x02),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0x31, 0x22),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0x32, 0x11),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0x33, 0xaa),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0x34, 0xbb),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0x35, 0x66),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0x36, 0x00),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0x37, 0x22),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0x38, 0x22),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0x39, 0x22),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0x3a, 0x22),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0x3b, 0x22),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0x3c, 0x22),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0x3d, 0x20),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0x3e, 0x22),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0x3f, 0x22),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0x40, 0x22),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0x53, 0x1a),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0xff, 0xff, 0x98, 0x06, 0x04, 0x07),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0x17, 0x12),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0x02, 0x77),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0xff, 0xff, 0x98, 0x06, 0x04, 0x00),
        CONNECTOR_CMD_SEQUENCE(0x15, 0x00, 0x35, 0x00),
        CONNECTOR_CMD_SEQUENCE(0x15, 100, 0x11, 0x00),
        CONNECTOR_CMD_SEQUENCE(0x15, 10, 0x29, 0x00),
    };
    return dsi_send_cmd_sequence(desc, init_cmds, sizeof(init_cmds), K_FALSE);
}

static const struct panel_ops ili9806_ops = {
    .reset        = panel_generic_reset,
    .init         = ili9806_init,
    .power_off    = NULL,
    .read_chip_id = dsi_read_chip_id,
};

static const struct panel_desc ili9806_panel_desc = {
    .name = "ili9806_480x800",
    .connector_type = ILI9806_480_800_DSI_V1,
    .bus_type = PANEL_BUS_DSI,

    .timing = {
        .pclk_khz = 27000,
        .hactive = 480,
        .hsync_len = 4,
        .hback_porch = 10,
        .hfront_porch = 30,
        .vactive = 800,
        .vsync_len = 8,
        .vback_porch = 20,
        .vfront_porch = 40,
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
        .lanes = K_DSI_2LANE,
        .cmd_mode = K_DSI_CMD_LP_MODE,
        .video_mode = K_DSI_VIDEO_BURST_MODE,
        .vc_id = 0,
    },

    .bus_ops = &dsi_bus_ops,
    .ops = &ili9806_ops,
};

static const struct panel_desc* ili9806_panel_variants[] = {
    &ili9806_panel_desc,
    NULL,
};

struct panel_drv mipi_ili9806_drv = {
    .connector_name = "ili9806",
    .panel_variants = ili9806_panel_variants,
    .active_panel   = &ili9806_panel_desc,
};
