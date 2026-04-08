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

static inline int qspi_cmd(const struct panel_desc* desc, k_u8 cmd, const k_u8* data, k_u32 len)
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

            qspi_cmd(desc, p_data[0], &p_data[2], p_data[1]);
            p_data += p_data[1] + 2;
        }
    }
}

/* https://item.taobao.com/item.htm?id=1001917382287&spm=a21m98.27004841 */
static int nv3030b_xr0183716b2_init(const struct panel_desc* desc)
{
    /* clang-format off */
    const uint8_t lcd_init_sequence[] = {
        /* Format: 
           - 0x00 + delay_ms: delay command
           - command byte + data length byte + data bytes
        */

        0x11, 0x00,                   /* WriteComm(0x11) with no data */
        0x00, 10,                     /* delay 10ms */

        /* Vendor specific commands */
        0xFD, 0x02, 0x06, 0x08,       /* WriteComm(0xFD) + 2 bytes: 0x06, 0x08 */
        0x61, 0x02, 0x07, 0x04,       /* WriteComm(0x61) + 2 bytes: 0x07, 0x04 */
        0x62, 0x03, 0x00, 0x44, 0x45, /* WriteComm(0x62) + 3 bytes: 0x00, 0x44, 0x45 */
        0x63, 0x04, 0x41, 0x07, 0x12, 0x12, /* WriteComm(0x63) + 4 bytes */
        0x64, 0x01, 0x37,             /* WriteComm(0x64) + 1 byte: 0x37 */

        /* VSP */
        0x65, 0x03, 0x09, 0x10, 0x21, /* WriteComm(0x65) + 3 bytes */

        /* VSN */
        0x66, 0x03, 0x09, 0x10, 0x21, /* WriteComm(0x66) + 3 bytes */

        /* add source_neg_time */
        0x67, 0x02, 0x20, 0x40,       /* WriteComm(0x67) + 2 bytes */

        /* gamma vap/van */
        0x68, 0x04, 0x90, 0x4C, 0x7C, 0x66, /* WriteComm(0x68) + 4 bytes */

        0xb1, 0x03, 0x0F, 0x02, 0x01, /* WriteComm(0xb1) + 3 bytes */
        0xB4, 0x01, 0x01,             /* WriteComm(0xB4) + 1 byte: 0x01 */

        /* porch */
        0xB5, 0x04, 0x02, 0x02, 0x0a, 0x14, /* WriteComm(0xB5) + 4 bytes */
        0xB6, 0x05, 0x04, 0x01, 0x9f, 0x00, 0x02, /* WriteComm(0xB6) + 5 bytes */

        /* gamme sel */
        0xDF, 0x01, 0x11,             /* WriteComm(0xDF) + 1 byte: 0x11 */

        /* GAMMA section */
        0xE2, 0x06, 0x13, 0x00, 0x00, 0x30, 0x33, 0x3f, /* WriteComm(0xE2) + 6 bytes */
        0xE5, 0x06, 0x3f, 0x33, 0x30, 0x00, 0x00, 0x13, /* WriteComm(0xE5) + 6 bytes */
        0xE1, 0x02, 0x00, 0x57,       /* WriteComm(0xE1) + 2 bytes */
        0xE4, 0x02, 0x58, 0x00,       /* WriteComm(0xE4) + 2 bytes */
        0xE0, 0x07, 0x01, 0x03, 0x0e, 0x0e, 0x0c, 0x15, 0x19, /* WriteComm(0xE0) + 7 bytes */
        0xE3, 0x08, 0x1a, 0x16, 0x0C, 0x0f, 0x0e, 0x0d, 0x02, 0x01, /* WriteComm(0xE3) + 8 bytes */
        0xE6, 0x02, 0x00, 0xff,       /* WriteComm(0xE6) + 2 bytes */
        0xE7, 0x06, 0x01, 0x04, 0x03, 0x03, 0x00, 0x12, /* WriteComm(0xE7) + 6 bytes */

        /* source */
        0xE8, 0x03, 0x00, 0x70, 0x00, /* WriteComm(0xE8) + 3 bytes */

        /* gate */
        0xEC, 0x01, 0x52,             /* WriteComm(0xEC) + 1 byte: 0x52 */

        0xF1, 0x03, 0x01, 0x01, 0x02, /* WriteComm(0xF1) + 3 bytes */
        0xF6, 0x04, 0x09, 0x10, 0x00, 0x00, /* WriteComm(0xF6) + 4 bytes */
        0xfd, 0x02, 0xfa, 0xfc,       /* WriteComm(0xfd) + 2 bytes */
        0x3a, 0x01, 0x05,             /* WriteComm(0x3a) + 1 byte: 0x05 */
        0x36, 0x01, 0x08,             /* WriteComm(0x36) + 1 byte: 0x08 */
        0x35, 0x01, 0x00,             /* WriteComm(0x35) + 1 byte: 0x00 */
        0x21, 0x00,                   /* WriteComm(0x21) with no data */

        0x00, 1,                      /* delay 1ms */

        /* Color format: 55=565 (16-bit), 66=18-bit, 77=24-bit */
        0x3A, 0x01, 0x55,             /* WriteComm(0x3A) + 1 byte: 0x55 (16-bit RGB565) */

        /* Display control: D3=RGB/BGR swap, D5=scan direction, D6/D7=mirror */
        0x36, 0x01, 0x08,             /* WriteComm(0x36) + 1 byte: 0x08 */

        /* Sleep out */
        0x11, 0x00,                   /* WriteComm(0x11) with no data */
        0x00, 120,                    /* delay 100ms */

        /* Display on */
        0x29, 0x00,                   /* WriteComm(0x29) with no data */
    };
    /* clang-format on */

    panel_send_init_sequence(desc, lcd_init_sequence, sizeof(lcd_init_sequence));

    return 0;
}

static int nv3030b_power_off(const struct panel_desc* desc)
{
    qspi_cmd(desc, 0x28, NULL, 0);
    rt_thread_mdelay(20);

    qspi_cmd(desc, 0x10, NULL, 0);
    rt_thread_mdelay(120);

    return 0;
}

static const struct panel_ops nv3030b_xr0183716b2_ops = {
    .reset     = panel_generic_reset,
    .init      = nv3030b_xr0183716b2_init,
    .power_off = nv3030b_power_off,
};

static const struct panel_desc nv3030b_qspi_xr0183716b2_240x240_desc = {
    .name           = "nv3030b_qspi_xr0183716b2_240x240",
    .connector_type = NV3030B_240_240_QSPI_V1,
    .bus_type       = PANEL_BUS_QSPI,

    .timing = {
        .pclk_khz = (240 + 20 + 40 + 80) * (284 + 20 + 40 + 60) * 30 / 1000,
        .hactive = 240,
        .hsync_len = 20,
        .hback_porch = 40,
        .hfront_porch = 80,
        .vactive = 284,
        .vsync_len = 20,
        .vback_porch = 40,
        .vfront_porch = 60,
    },

    .bg_color = PANEL_BG_COLOR_BLACK,

    .gpio = {
        .reset_pin            = CONFIG_MPP_QSPI_LCD_RESET_PIN,
        .backlight_pin        = CONFIG_MPP_QSPI_LCD_BACKLIGHT_PIN,
        .reset_delay_ms       = 100,
        .backlight_delay_ms   = 0,
        .reset_active_low     = K_TRUE,
        .backlight_active_low = K_FALSE,
    },

    .bus.qspi = {
        .base = {
            .pixel_format = PIXEL_FORMAT_RGB_565,
            .flag = CONNECTOR_SW_BRIDGE_FLAG_SWAP_RGB565_BYTE_ORDER,
        },
        .qspi_dev_name = "lcd_nv3030b",
        .qspi_mode     = 0,
        .qspi_speed_hz = 15 * 1000 * 1000,
    },

    .bus_ops = &qspi_bus_ops,
    .ops     = &nv3030b_xr0183716b2_ops,
};

static const struct panel_desc* nv3030b_panel_variants[] = {
    &nv3030b_qspi_xr0183716b2_240x240_desc,
    NULL,
};

struct panel_drv qspi_nv3030b_drv = {
    .connector_name = "nv3030b_qspi",
    .panel_variants = nv3030b_panel_variants,
    .active_panel   = &nv3030b_qspi_xr0183716b2_240x240_desc,
};
