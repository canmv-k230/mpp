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

/**
 * Helper: send an SPI panel command via bus_ops->send_cmd
 */
static inline int spi_cmd(const struct panel_desc* desc, k_u8 cmd, const k_u8* data, k_u32 len)
{
    if (desc->bus_ops && desc->bus_ops->send_cmd)
        return desc->bus_ops->send_cmd(desc, cmd, data, len);
    return -1;
}

static void panel_send_init_sequence(const struct panel_desc* desc, const uint8_t* data, size_t size)
{
    const uint8_t* p_data   = data;
    const uint8_t* data_end = data + size;

    while (p_data < data_end) {
        if ((size_t)(data_end - p_data) < 2) {
            rt_kprintf("command sequence format error @ %d(0x%02x).\n", (int)(p_data - data), p_data[0]);
            break;
        }

        if (p_data[0] == 0x00) {
            rt_thread_mdelay(p_data[1]);
            p_data += 2;
        } else {
            if ((size_t)(data_end - p_data - 2) < p_data[1]) {
                rt_kprintf("command sequence format error @ %d(0x%02x).\n", (int)(p_data - data), p_data[0]);
                break;
            }

            spi_cmd(desc, p_data[0], &p_data[2], p_data[1]);
            p_data += p_data[1] + 2;
        }
    }
}

static int st7789_init(const struct panel_desc* desc)
{
    /* clang-format off */
    const uint8_t lcd_init_sequence[] = {
        // Command sequence for initializing the LCD
        0x11, 0, // Sleep Out
        0x00, 5, // Delay
        0x11, 0, // Sleep Out, send twice
        0x00, 30, // Delay
        0x36, 1,    (0 << 7) | (1 << 6) | (1 << 5), // Memory Access Control; BIT7: MY; BIT6: MX; BIT5: MV (landscape)
        0x3A, 1,    0x65, // Interface Pixel Format
        0xB2, 5,    0x0C, 0x0C, 0x00, 0x33, 0x33, // Porch Setting
        0xB7, 1,    0x75, // Gate Control
        0xBB, 1,    0x1A, // VCOM Setting
        0xC0, 1,    0x2C, // LV0 Control
        0xC2, 1,    0x01, // VGH/VGL Setting
        0xC3, 1,    0x13, // VGH Setting
        0xC4, 1,    0x20, // VGL Setting
        0xC6, 1,    0x0F, // Display Control
        0xD0, 2,    0xA4, 0xA1, // Power Control 1
        0xD6, 1,    0xA1, // Power Control 2
        0xE0, 14,   0xD0, 0x0D, 0x14, 0x0D, 0x0D, 0x09,
        0x38, 0x44, 0x4E, 0x3A, 0x17, 0x18, 0x2F, 0x30, // Positive Gamma Correction
        0xE1, 14,   0xD0, 0x09, 0x0F, 0x08, 0x07, 0x14,
        0x37, 0x44, 0x4D, 0x38, 0x15, 0x16, 0x2C, 0x2E, // Negative Gamma Correction
        0x20, 0, // Inversion On
        0x29, 0, // Display On
    };
    /* clang-format on */

    panel_send_init_sequence(desc, lcd_init_sequence, sizeof(lcd_init_sequence));

    return 0;
}

static int st7789_power_off(const struct panel_desc* desc)
{
    /* Display Off */
    spi_cmd(desc, 0x28, NULL, 0);
    rt_thread_mdelay(20);

    /* Sleep In */
    spi_cmd(desc, 0x10, NULL, 0);
    rt_thread_mdelay(120);

    return 0;
}

static const struct panel_ops st7789_ops = {
    .reset       = panel_generic_reset,
    .init        = st7789_init,
    .power_off   = st7789_power_off,
};

static const struct panel_desc st7789_spi_320x240_desc = {
    .name           = "st7789_spi_320x240",
    .connector_type = ST7789_320_240_SPI_V1,
    .bus_type       = PANEL_BUS_SPI,

    .timing = {
        .pclk_khz = (320 + 20 + 40 + 80) * (240 + 20 + 40 + 60) * 30 / 1000,
        .hactive = 320,
        .hsync_len = 20,
        .hback_porch = 40,
        .hfront_porch = 80,
        .vactive = 240,
        .vsync_len = 20,
        .vback_porch = 40,
        .vfront_porch = 60,
    },

    .bg_color = PANEL_BG_COLOR_BLACK,

    .gpio = {
        .reset_pin         = CONFIG_MPP_SPI_LCD_RESET_PIN,
        .backlight_pin     = CONFIG_MPP_SPI_LCD_BACKLIGHT_PIN,
        .reset_delay_ms    = 10,
        .backlight_delay_ms = 0,
        .reset_active_low  = K_TRUE,
        .backlight_active_low = K_FALSE,
    },

    .bus.spi = {
        .base = {
            .pixel_format = PIXEL_FORMAT_RGB_565,
            .flag = CONNECTOR_SW_BRIDGE_FLAG_SWAP_RGB565_PIXEL_ORDER,
        },
        .spi_dev_name  = "lcd_st7789",
        .spi_mode      = 3,
        .spi_speed_hz  = 50 * 1000 * 1000,
    },

    .bus_ops = &spi_bus_ops,
    .ops     = &st7789_ops,
};

static const struct panel_desc* st7789_panel_variants[] = {
    &st7789_spi_320x240_desc,
    NULL,
};

struct panel_drv spi_st7789_drv = {
    .connector_name = "st7789_spi",
    .panel_variants = st7789_panel_variants,
    .active_panel   = &st7789_spi_320x240_desc,
};
