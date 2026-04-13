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

static int jd9852_init(const struct panel_desc* desc)
{
    /* clang-format off */
    const k_u8 init_cmds[] = {
        0x05,100,1,0x01, // SW RESET
        0x39,0,4,0xDF,0x98,0x51,0xE9,
        0x15,0,2,0xDE,0x00,
        0x39,0,5,0xB7,0x16,0x7D,0x16,0x3B,
        0x39,0,33,0xC8,0x3F,0x2B,0x22,0x21,0x23,0x26,0x21,0x22,0x22,0x22,0x1E,0x15,0x11,0x0A,0x09,0x0E,0x3F,0x2B,0x22,0x21,0x24,0x27,0x22,0x22,0x22,0x21,0x1E,0x14,0x11,0x0A,0x09,0x0E,
        0x39,0,4,0xB9,0x33,0x08,0xCC,
        0x39,0,9,0xBB,0x47,0x7A,0x30,0x40,0x7C,0x60,0x70,0x70,
        0x39,0,3,0xBC,0x38,0x3C,
        0x15,0,2,0xC0,0x31,
        0x15,0,2,0x20,0x00,
        0x15,0,2,0xC1,0x12,
        0x39,0,10,0xC3,0x08,0x00,0x0A,0x10,0x08,0x54,0x45,0x71,0x2C,
        0x39,0,18,0xC4,0x00,0xA0,0x79,0x0E,0x0A,0x16,0x79,0x0E,0x0A,0x16,0x79,0x0E,0x0A,0x16,0x82,0x00,0x03,
        0x39,0,7,0xD0,0x04,0x0C,0x6A,0x0F,0x00,0x03,
        0x39,0,3,0xD7,0x13,0x00,
        0x15,1,2,0xDE,0x02,
        0x39,0,6,0xB8,0x1D,0xA0,0x2F,0x2C,0x2B,
        0x39,0,5,0xC1,0x10,0x66,0x66,0x01,
        0x39,0,2,0xDE,0x00,
        0x15,0,2,0x3A,0x55,
        0x05,100,1,0x11,
        0x05,0,1,0x29
    };
    /* clang-format on */

    return dsi_send_cmd_sequence(desc, init_cmds, sizeof(init_cmds), K_FALSE);
}

static const struct panel_ops jd9852_ops = {
    .reset        = panel_generic_reset,
    .init         = jd9852_init,
    .power_off    = NULL,
    .read_chip_id = NULL, /* JD9852 does not support DCS reads */
};

static const struct panel_desc jd9852_panel_desc = {
    .name = "jd9852_240x320",
    .connector_type = JD9852_240_320_DSI_V1,
    .bus_type = PANEL_BUS_DSI,

    .timing = {
        .pclk_khz = 5500,
        .hactive = 240,
        .hsync_len = 4,
        .hback_porch = 20,
        .hfront_porch = 40,
        .vactive = 320,
        .vsync_len = 8,
        .vback_porch = 24,
        .vfront_porch = 32,
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
        .lanes = K_DSI_1LANE,
        .cmd_mode = K_DSI_CMD_LP_MODE,
        .video_mode = K_DSI_VIDEO_BURST_MODE,
        .vc_id = 0,
        .lp_cmd_speed_mhz = 10,
    },

    .bus_ops = &dsi_bus_ops,
    .ops = &jd9852_ops,
};

static const struct panel_desc* jd9852_panel_variants[] = {
    &jd9852_panel_desc,
    NULL,
};

struct panel_drv mipi_jd9852_drv = {
    .connector_name = "jd9852",
    .panel_variants = jd9852_panel_variants,
    .active_panel   = &jd9852_panel_desc,
};
