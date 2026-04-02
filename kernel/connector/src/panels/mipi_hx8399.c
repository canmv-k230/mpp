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

static int hx8399_init(const struct panel_desc* desc)
{
    /* clang-format off */
    const k_u8 init_sequence[] = {
        // cmd type, delay, data length, data0 ... dataN
        0x39, 0, 4,  0xB9, 0xFF, 0x83, 0x99,                         // CMD_SETEXTC
        0x15, 0, 2,  0xD2, 0x77,                                     // CMD_SETOFFSET

        0x39, 0, 16, 0xB1, 0x02,0x04,0x74,0x94,0x01,0x32,0x33,0x11,
                        0x11,0xAB,0x4D,0x56,0x73,0x02,0x02,           // CMD_SETPOWER

        0x15, 0, 3,  0xBA, 0x63, 0x03,                               // CMD_SETMIPI

        0x39, 0, 16, 0xB2, 0x00,0x80,0x80,0xAE,0x05,0x07,0x5A,0x11,
                        0x00,0x00,0x10,0x1E,0x70,0x03,0xD4,           // CMD_SETDISP

        0x15, 0, 2,  0x36, 0x02,                                     // CMD_SETMADCTL

        0x39, 0, 45, 0xB4,
            0x00,0xFF,0x02,0xC0,0x02,0xC0,0x00,0x00,
            0x08,0x00,0x04,0x06,0x00,0x32,0x04,0x0A,
            0x08,0x21,0x03,0x01,0x00,0x0F,0xB8,0x8B,
            0x02,0xC0,0x02,0xC0,0x00,0x00,0x08,0x00,
            0x04,0x06,0x00,0x32,0x04,0x0A,0x08,0x01,
            0x00,0x0F,0xB8,0x01,                                    // CMD_SETCYC

        0x39, 0, 34, 0xD3,
            0x00,0x00,0x00,0x00,0x00,0x00,0x06,0x00,
            0x00,0x10,0x04,0x00,0x04,0x00,0x00,0x00,
            0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,
            0x00,0x05,0x05,0x07,0x00,0x00,0x00,0x05,
            0x40,                                                 // CMD_SETGIP_0

        0x39, 5, 33, 0xD5,
            0x18,0x18,0x19,0x19,0x18,0x18,0x21,0x20,
            0x01,0x00,0x07,0x06,0x05,0x04,0x03,0x02,
            0x18,0x18,0x18,0x18,0x18,0x18,0x2F,0x2F,
            0x30,0x30,0x31,0x31,0x18,0x18,0x18,0x18,   // CMD_SETGIP_1

        0x39, 5, 33, 0xD6,
            0x18,0x18,0x19,0x19,0x40,0x40,0x20,0x21,
            0x06,0x07,0x00,0x01,0x02,0x03,0x04,0x05,
            0x40,0x40,0x40,0x40,0x40,0x40,0x2F,0x2F,
            0x30,0x30,0x31,0x31,0x40,0x40,0x40,0x40,   // CMD_SETGIP_2

        0x39, 5, 17, 0xD8,
            0xA2,0xAA,0x02,0xA0,0xA2,0xA8,0x02,0xA0,
            0xB0,0x00,0x00,0x00,0xB0,0x00,0x00,0x00,   // CMD_SETGIP_3

        0x15, 0, 2,  0xBD, 0x01,
        0x39, 0, 17, 0xD8,
            0xB0,0x00,0x00,0x00,0xB0,0x00,0x00,0x00,
            0xE2,0xAA,0x03,0xF0,0xE2,0xAA,0x03,0xF0,

        0x15, 0, 2,  0xBD, 0x02,
        0x39, 0, 9,  0xD8,
            0xE2,0xAA,0x03,0xF0,0xE2,0xAA,0x03,0xF0,

        0x15, 0, 2,  0xBD, 0x00,

        0x39, 0, 3,  0xB6, 0x8D, 0x8D,                       // CMD_SETVCOM

        0x39, 0, 55, 0xE0,
            0x00,0x0E,0x19,0x13,0x2E,0x39,0x48,0x44,
            0x4D,0x57,0x5F,0x66,0x6C,0x76,0x7F,0x85,
            0x8A,0x95,0x9A,0xA4,0x9B,0xAB,0xB0,0x5C,
            0x58,0x64,0x77,
            0x00,0x0E,0x19,0x13,0x2E,0x39,0x48,0x44,
            0x4D,0x57,0x5F,0x66,0x6C,0x76,0x7F,0x85,
            0x8A,0x95,0x9A,0xA4,0x9B,0xAB,0xB0,0x5C,
            0x58,0x64,0x77,                                 // CMD_SETGAMMA

        0x05, 100, 1, 0x11,                                 // Sleep Out
        0x05, 20,  1, 0x29,                                 // Display ON
    };
    /* clang-format on */

    return dsi_send_cmd_sequence(desc, init_sequence, sizeof(init_sequence), K_FALSE);
}

static int hx8399_power_off(const struct panel_desc* desc)
{
    (void)desc;

    // // Display Off
    // uint8_t display_off[] = { 0x28 };
    // dwc_dsi_dcs_write(display_off, 1);
    // rt_thread_mdelay(10); // Wait for the command to take effect

    // // Sleep In
    // uint8_t sleep_in[] = { 0x10 };
    // dwc_dsi_dcs_write(sleep_in, 1);

    return 0;
}

static const struct panel_ops hx8399_ops = {
    .reset        = panel_generic_reset,
    .init         = hx8399_init,
    .power_off    = hx8399_power_off,
    .read_chip_id = dsi_read_chip_id,
};

static const struct panel_desc hx8399_panel_desc = {
    .name = "hx8399_1080x1920",
    .connector_type = HX8399_1080_1920_DSI_V1,
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
    .ops = &hx8399_ops,
};

static const struct panel_desc* hx8399_panel_variants[] = {
    &hx8399_panel_desc,
    NULL,
};

struct panel_drv mipi_hx8399_drv = {
    .connector_name = "hx8399",
    .panel_variants = hx8399_panel_variants,
    .active_panel   = &hx8399_panel_desc,
};
