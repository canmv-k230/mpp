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

#include <rtthread.h>

#include "connector_panel.h"
#include "k_autoconf_comm.h"

static inline int i8080_cmd(const struct panel_desc* desc, k_u8 cmd, const k_u8* data, k_u32 len)
{
    if (desc->bus_ops && desc->bus_ops->send_cmd)
        return desc->bus_ops->send_cmd(desc, cmd, data, len);
    return -1;
}

static int panel_send_init_sequence(const struct panel_desc* desc, const k_u8* data, size_t size)
{
    const k_u8* p_data   = data;
    const k_u8* data_end = data + size;

    while (p_data < data_end) {
        if ((size_t)(data_end - p_data) < 2) {
            rt_kprintf("command sequence format error @ %d(0x%02x).\n", (int)(p_data - data), p_data[0]);
            return -1;
        }

        if (p_data[0] == 0x00) {
            rt_thread_mdelay(p_data[1]);
            p_data += 2;
        } else {
            if ((size_t)(data_end - p_data - 2) < p_data[1]) {
                rt_kprintf("command sequence format error @ %d(0x%02x).\n", (int)(p_data - data), p_data[0]);
                return -1;
            }

            if (i8080_cmd(desc, p_data[0], &p_data[2], p_data[1]) != 0) {
                rt_kprintf("st7789_i8080: init cmd 0x%02x failed\n", p_data[0]);
                return -1;
            }
            p_data += p_data[1] + 2;
        }
    }

    return 0;
}

static int st7789_i8080_init(const struct panel_desc* desc)
{
    /* clang-format off */
    const k_u8 lcd_init_sequence[] = {
        0x11, 0,
        0x00, 120,
        0x36, 1, 0x00,
        0x3A, 1, 0x06,
        0xB2, 5, 0x0B, 0x0B, 0x00, 0x33, 0x33,
        0xB7, 1, 0x35,
        0xBB, 1, 0x23,
        0xC0, 1, 0x2C,
        0xC2, 1, 0x01,
        0xC3, 1, 0x0D,
        0xC4, 1, 0x20,
        0xC6, 1, 0x13,
        0xD0, 2, 0xA4, 0xA1,
        0xD6, 1, 0xA1,
        0xE0, 14, 0xF0, 0x02, 0x05, 0x07, 0x06, 0x03, 0x26,
                  0x33, 0x3D, 0x36, 0x12, 0x12, 0x24, 0x2B,
        0xE1, 14, 0xF0, 0x04, 0x07, 0x09, 0x08, 0x25, 0x25,
                  0x33, 0x3D, 0x37, 0x14, 0x14, 0x26, 0x2C,
        0xE4, 3, 0x25, 0x00, 0x00,
        0x21, 0,
        0x29, 0,
    };
    /* clang-format on */

    return panel_send_init_sequence(desc, lcd_init_sequence, sizeof(lcd_init_sequence));
}

static int st7789_i8080_power_off(const struct panel_desc* desc)
{
    i8080_cmd(desc, 0x28, NULL, 0);
    rt_thread_mdelay(20);

    i8080_cmd(desc, 0x10, NULL, 0);
    rt_thread_mdelay(120);

    return 0;
}

static const struct panel_ops st7789_i8080_ops = {
    .reset     = panel_generic_reset,
    .init      = st7789_i8080_init,
    .power_off = st7789_i8080_power_off,
};

static const struct panel_desc st7789_i8080_240x280_desc = {
    .name           = "st7789_i8080_240x280",
    .connector_type = ST7789_240_280_I8080_V1,
    .bus_type       = PANEL_BUS_I8080_SPI,

    .timing = {
        .pclk_khz = (240 + 20 + 40 + 80) * (280 + 20 + 40 + 60) * 30 / 1000,
        .hactive = 240,
        .hsync_len = 20,
        .hback_porch = 40,
        .hfront_porch = 80,
        .vactive = 280,
        .vsync_len = 20,
        .vback_porch = 40,
        .vfront_porch = 60,
    },

    .bg_color = PANEL_BG_COLOR_BLACK,

    .gpio = {
        .reset_pin            = CONFIG_MPP_OSPI_LCD_RESET_PIN,
        .backlight_pin        = CONFIG_MPP_OSPI_LCD_BACKLIGHT_PIN,
        .reset_delay_ms       = 120,
        .backlight_delay_ms   = 0,
        .reset_active_low     = K_TRUE,
        .backlight_active_low = K_FALSE,
    },

    .bus.i8080_spi = {
        .base = {
            .pixel_format = PIXEL_FORMAT_RGB_888,
            .flag         = 0,
            .fps          = 30,
            .x_offset     = 0,
            .y_offset     = 20,
        },
        .spi_dev_name = "lcd_st7789_i8080",
        .spi_mode     = 0,
        .spi_speed_hz = 5 * 1000 * 1000,
        .bus_width    = 8,
    },

    .bus_ops = &i8080_spi_bus_ops,
    .ops     = &st7789_i8080_ops,
};

static const struct panel_desc* st7789_i8080_panel_variants[] = {
    &st7789_i8080_240x280_desc,
    NULL,
};

struct panel_drv i8080_st7789_drv = {
    .connector_name = "st7789_i8080",
    .panel_variants = st7789_i8080_panel_variants,
    .active_panel   = &st7789_i8080_240x280_desc,
};
