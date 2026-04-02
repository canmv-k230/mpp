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

/*
 * NT35516 requires a custom init callback because it writes black pixel data
 * to the last 4 columns of the display programmatically.
 */

static int nt35516_init(const struct panel_desc* desc)
{
    /* clang-format off */
    /* Sequence A: set GRAM window to last 4 columns (536–539) */
    const k_u8 set_last4_cols[] = {
        0x05, 10, 1, 0x11,
        /* Set column (x = 536 .. 539) */
        0x39, 0, 5, 0x2A, 0x02, 0x18, 0x02, 0x1B,
        /* Set page (y = 0 .. 959) */
        0x39, 0, 5, 0x2B, 0x00, 0x00, 0x03, 0xBF,
        /* Memory write */
        0x39, 0, 1, 0x2C,
    };

    /* Sequence B: normal init, but active area = 0–535 */
    const k_u8 lcd_init_seq[] = {
        0x05, 10, 1, 0x11,
        /* Set column (x = 0 .. 535) */
        0x39, 0, 5, 0x2A, 0x00, 0x00, 0x02, 0x17,
        /* Set page (y = 0 .. 959) */
        0x39, 0, 5, 0x2B, 0x00, 0x00, 0x03, 0xBF,
        /* Memory write */
        0x05, 10, 1, 0x29,
    };
    /* clang-format on */

    k_u8 black_data[4] = { 0x3C, 0x00, 0x00, 0x00 };
    int  i;

    /* Step 1: prepare last 4 columns */
    dsi_send_cmd_sequence(desc, set_last4_cols, sizeof(set_last4_cols), K_FALSE);

    /* Step 2: fill last 4 columns with black pixels (4 columns * 960 rows) */
    for (i = 0; i < (4 * 960); i++) {
        dwc_dsi_dcs_write(black_data, 4, desc->bus.dsi.vc_id);
    }

    /* Step 3: run normal init with restricted active area */
    dsi_send_cmd_sequence(desc, lcd_init_seq, sizeof(lcd_init_seq), K_FALSE);

    return 0;
}

static const struct panel_ops nt35516_ops = {
    .reset        = panel_generic_reset,
    .init         = nt35516_init,
    .power_off    = NULL,
    .read_chip_id = dsi_read_chip_id,
};

static const struct panel_desc nt35516_panel_desc = {
    .name = "nt35516_536x960",
    .connector_type = NT35516_536_960_DSI_V1,
    .bus_type = PANEL_BUS_DSI,

    .timing = {
         .pclk_khz = 33000,
         .hactive = 536,
         .hsync_len = 20,
         .hback_porch = 20,
         .hfront_porch = 40,
         .vactive = 960,
         .vsync_len = 10,
         .vback_porch = 20,
         .vfront_porch = 110,
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
    .ops = &nt35516_ops,
};

static const struct panel_desc* nt35516_panel_variants[] = {
    &nt35516_panel_desc,
    NULL,
};

struct panel_drv mipi_nt35516_drv = {
    .connector_name = "nt35516",
    .panel_variants = nt35516_panel_variants,
    .active_panel   = &nt35516_panel_desc,
};
