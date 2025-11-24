/* Copyright (c) 2023, Canaan Bright Sight Co., Ltd
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

#include "connector_dev.h"

#include "drv_gpio.h"

#include "k_autoconf_comm.h"

static void lcd_init(void)
{
    /* clang-format off */
    const k_u8 init_sequence[] = {
        // cmd type, delay, data length, data0 ... dataN
    0x05,0,1,0x10,
    0x05,0,1,0x28,
    0x39,0,4,0x99,0x71,0x02,0xA2,
    0x39,0,4,0x99,0x71,0x02,0xA3,
    0x39,0,4,0x99,0x71,0x02,0xA4,
    0x39,0,8,0xB0,0x22,0x43,0x1E,0x43,0x2F,0x57,0x57,
    0x39,0,3,0xB7,0x7D,0x7D,
    0x39,0,3,0xBF,0x7A,0x7A,
    0x39,0,38,0xC8,0x00,0x00,0x13,0x23,0x3E,0x00,0x6A,0x03,0xB0,0x06,0x11,0x0F,0x07,0x85,0x03,0x21,0xD5,0x01,0x18,0x00,0x22,0x56,0x0F,0x98,0x0A,0x32,0xF8,0x0D,0x48,0x0F,0xF3,0x80,0x0F,0xAC,0xC1,0x03,0xC4,
    0x39,0,38,0xC9,0x00,0x00,0x13,0x23,0x3E,0x00,0x6A,0x03,0xB0,0x06,0x11,0x0F,0x07,0x85,0x03,0x21,0xD5,0x01,0x18,0x00,0x22,0x56,0x0F,0x98,0x0A,0x32,0xF8,0x0D,0x48,0x0F,0xF3,0x80,0x0F,0xAC,0xC1,0x03,0xC4,
    0x39,0,7,0xD7,0x10,0x0C,0x02,0x19,0x40,0x40,
    0x39,0,33,0xA3,0x40,0x03,0x80,0xCF,0x44,0x00,0x00,0x00,0x02,0x05,0x6F,0x6F,0x00,0x1A,0x00,0x45,0x05,0x00,0x00,0x00,0x00,0x46,0x00,0x00,0x02,0x20,0x52,0x00,0x05,0x00,0x00,0xFF,
    0x39,0,45,0xA6,0x02,0x00,0x24,0x55,0x35,0x00,0x38,0x00,0x97,0x97,0x00,0x24,0x55,0x36,0x00,0x37,0x00,0x97,0x97,0x02,0xAC,0x51,0x3A,0x00,0x00,0x00,0x97,0x97,0x00,0xAC,0x21,0x00,0x0B,0x00,0x00,0x97,0x97,0x00,0x00,0x06,0x00,0x00,0x00,0x00,
    0x39,0,49,0xA7,0x19,0x19,0x00,0x64,0x40,0x07,0x16,0x40,0x00,0x04,0x03,0x97,0x97,0x00,0x64,0x40,0x25,0x34,0x00,0x00,0x02,0x01,0x97,0x97,0x00,0x64,0x40,0x4B,0x5A,0x00,0x00,0x02,0x01,0x97,0x97,0x00,0x24,0x40,0x69,0x78,0x00,0x00,0x00,0x00,0x97,0x97,0x00,0x44,
    0x39,0,38,0xAC,0x11,0x08,0x13,0x0A,0x18,0x1A,0x1B,0x00,0x06,0x03,0x19,0x1B,0x1B,0x1B,0x18,0x1B,0x10,0x09,0x12,0x0B,0x18,0x1A,0x1B,0x02,0x06,0x01,0x19,0x1B,0x1B,0x1B,0x18,0x1B,0xFF,0x67,0xFF,0x67,0x00,
    0x39,0,8,0xAD,0xCC,0x40,0x46,0x11,0x04,0x6F,0x6F,
    0x39,0,15,0xE8,0x30,0x07,0x00,0xB3,0xB3,0x9C,0x00,0xE2,0x04,0x00,0x00,0x00,0x00,0xEF,
    0x39,0,3,0x75,0x03,0x04,
    0x39,0,34,0xE7,0x8B,0x3C,0x00,0x0C,0xF0,0x5D,0x00,0x5D,0x00,0x5D,0x00,0x5D,0x00,0xFF,0x00,0x08,0x7B,0x00,0x00,0xC8,0x6A,0x5A,0x08,0x1A,0x3C,0x00,0x71,0x01,0x8C,0x01,0x7F,0xF0,0x22,
    0x39,0,10,0xE9,0x3C,0x7F,0x08,0x07,0x1A,0x7A,0x22,0x1A,0x33,
    0x15,0,2,0x35,0x00,
    0x05,120,1,0x11,
    0x05,0,1,0x29,
    };
    /* clang-format on */

    connector_send_cmd(init_sequence, sizeof(init_sequence), K_FALSE);
}

static void st7102_power_reset(k_s32 on)
{
    k_u8 rst_gpio;

    rst_gpio = CONFIG_MPP_DSI_LCD_RESET_PIN;

    kd_pin_mode(rst_gpio, GPIO_DM_OUTPUT);

    if (on)
        kd_pin_write(rst_gpio, GPIO_PV_HIGH);
    else
        kd_pin_write(rst_gpio, GPIO_PV_LOW);
}

static void st7102_set_backlight(k_s32 on)
{
    k_u8 backlight_gpio;

    if (CONFIG_MPP_DSI_LCD_BACKLIGHT_PIN == 255) // unused
        return;

    backlight_gpio = CONFIG_MPP_DSI_LCD_BACKLIGHT_PIN;

    kd_pin_mode(backlight_gpio, GPIO_DM_OUTPUT);
    if (on)
        kd_pin_write(backlight_gpio, GPIO_PV_HIGH);
    else
        kd_pin_write(backlight_gpio, GPIO_PV_LOW);
}

static k_s32 st7102_power_on(void* ctx, k_s32 on)
{
    k_s32                        ret = 0;
    struct connector_driver_dev* dev = ctx;

    // rst vo;
    k230_display_rst();

    if (on) {
        // rst st7102
        st7102_power_reset(1);
        rt_thread_mdelay(50);
        st7102_power_reset(0);
        rt_thread_mdelay(50);
        st7102_power_reset(1);

        rt_thread_mdelay(120);
        // enable backlight
        st7102_set_backlight(1);
    } else {
        st7102_set_backlight(0);
    }

    return ret;
}

static k_s32 st7102_set_phy_freq(k_connectori_phy_attr* phy_attr)
{
    k_vo_mipi_phy_attr mipi_phy_attr;

    memset(&mipi_phy_attr, 0, sizeof(k_vo_mipi_phy_attr));

    mipi_phy_attr.m           = phy_attr->m;
    mipi_phy_attr.n           = phy_attr->n;
    mipi_phy_attr.hs_freq     = phy_attr->hs_freq;
    mipi_phy_attr.voc         = phy_attr->voc;
    mipi_phy_attr.phy_lan_num = K_DSI_2LAN;

    connector_set_phy_freq(&mipi_phy_attr);

    return 0;
}

static k_s32 st7102_dsi_resolution_init(k_connector_info* info)
{
    k_vo_dsi_attr           attr;
    k_vo_display_resolution resolution;

    memset(&attr, 0, sizeof(k_vo_dsi_attr));
    attr.lan_num   = info->lan_num;
    attr.cmd_mode  = info->cmd_mode;
    attr.lp_div    = 8;
    attr.work_mode = info->work_mode;
    memcpy(&resolution, &info->resolution, sizeof(k_vo_display_resolution));
    memcpy(&attr.resolution, &resolution, sizeof(k_vo_display_resolution));
    connector_set_dsi_attr(&attr);

    lcd_init();

    connector_set_dsi_enable(1);

    if (info->dsi_test_mode == 1)
        connector_set_dsi_test_mode();

    return 0;
}

static k_s32 st7102_vo_resolution_init(k_vo_display_resolution* resolution, k_u32 bg_color, k_u32 intr_line)
{
    k_vo_display_resolution vo_resolution;
    k_vo_pub_attr           attr;

    memset(&attr, 0, sizeof(k_vo_pub_attr));
    attr.bg_color  = bg_color;
    attr.intf_sync = K_VO_OUT_1080P30;
    attr.intf_type = K_VO_INTF_MIPI;
    attr.sync_info = resolution;

    connector_set_vo_init();
    connector_set_vtth_intr(1, intr_line);
    connector_set_vo_param(&attr);
    connector_set_vo_enable();

    return 0;
}

static k_s32 st7102_init(void* ctx, k_connector_info* info)
{
    k_s32                        ret = 0;
    struct connector_driver_dev* dev = ctx;

    if (info->pixclk_div != 0)
        connector_set_pixclk(info->pixclk_div);

    ret |= st7102_set_phy_freq(&info->phy_attr);
    ret |= st7102_dsi_resolution_init(info);
    ret |= st7102_vo_resolution_init(&info->resolution, info->bg_color, info->intr_line);

    return ret;
}

static k_s32 st7102_get_chip_id(void* ctx, k_u32* chip_id)
{
    k_s32 ret = 0;

    return ret;
}

static k_s32 st7102_conn_check(void* ctx, k_s32* conn)
{
    k_s32 ret = 0;

    *conn = 1;

    return ret;
}

struct connector_driver_dev st7102_connector_drv = {
    .connector_name = "st7102",
    .connector_func = {
        .connector_power = st7102_power_on,
        .connector_init = st7102_init,
        .connector_get_chip_id = st7102_get_chip_id,
        .connector_conn_check = st7102_conn_check,
    },
};
