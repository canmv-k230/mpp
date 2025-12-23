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
#include "io.h"
#include "drv_gpio.h"
#include "k_vo_comm.h"
#include "k_connector_comm.h"

#define DELAY_MS_BACKLIGHT_DEFAULT     200
#define DELAY_MS_BACKLIGHT_FIRST       1

static k_s32 g_blacklight_delay_ms = DELAY_MS_BACKLIGHT_FIRST;


static void aml020t_480x360_init(k_u8 test_mode_en)
{
    const k_u8 init_sequence[] = {        
        // cmd type, delay, data length, data0 ... dataN
		0x39, 0x00, 0x06, 0xFF, 0x77, 0x01, 0x00, 0x00, 0x13,
		0x15, 0x00, 0x02, 0xEF, 0x08,
		0x39, 0x00, 0x06, 0xFF, 0x77, 0x01, 0x00, 0x00, 0x10,
		0x39, 0x00, 0x03, 0xC0, 0x2C, 0x00,
		0x39, 0x00, 0x03, 0xC1, 0x10, 0x0C,
		0x39, 0x00, 0x03, 0xC2, 0x21, 0x0A,
		0x15, 0x00, 0x02, 0xCC, 0x10,
		0x39, 0x00, 0x11, 0xB0, 0x00, 0x12, 0x9A, 0x0E, 0x12, 0x07, 0x0B, 0x08, 0x09, 0x26, 0x05, 0x51, 0x0F, 0x69, 0x30, 0x1C,
		0x39, 0x00, 0x11, 0xB1, 0x00, 0x12, 0x9B, 0x0D, 0x10, 0x06, 0x0B, 0x09, 0x08, 0x25, 0x03, 0x50, 0x0F, 0x68, 0x30, 0x1C,
		0x39, 0x00, 0x06, 0xFF, 0x77, 0x01, 0x00, 0x00, 0x11,
		0x15, 0x00, 0x02, 0xB0, 0x5D,
		0x15, 0x00, 0x02, 0xB1, 0x66,
		0x15, 0x00, 0x02, 0xB2, 0x84,
		0x15, 0x00, 0x02, 0xB3, 0x80,
		0x15, 0x00, 0x02, 0xB5, 0x4E,
		0x15, 0x00, 0x02, 0xB7, 0x85,
		0x15, 0x00, 0x02, 0xB8, 0x20,
		0x15, 0x00, 0x02, 0xC1, 0x78,
		0x15, 0x00, 0x02, 0xC2, 0x78,
		0x15, 0x00, 0x02, 0xD0, 0x88,
		0x39, 0x00, 0x04, 0xE0, 0x00, 0x00, 0x02,
		0x39, 0x00, 0x0C, 0xE1, 0x06, 0xA0, 0x08, 0xA0, 0x05, 0xA0, 0x07, 0xA0, 0x00, 0x44, 0x44,
		0x39, 0x00, 0x0D, 0xE2, 0x00, 0x00, 0x44, 0x44, 0x01, 0xA0, 0x00, 0x00, 0x01, 0xA0, 0x00, 0x00,
		0x39, 0x00, 0x05, 0xE3, 0x00, 0x00, 0x11, 0x11,
		0x39, 0x00, 0x03, 0xE4, 0x44, 0x44,
		0x39, 0x00, 0x11, 0xE5, 0x0D, 0x79, 0x0A, 0xA0, 0x0F, 0x7B, 0x0A, 0xA0, 0x09, 0x75, 0x0A, 0xA0, 0x0B, 0x77, 0x0A, 0xA0,
		0x39, 0x00, 0x05, 0xE6, 0x00, 0x00, 0x11, 0x11,
		0x39, 0x00, 0x03, 0xE7, 0x44, 0x44,
		0x39, 0x00, 0x11, 0xE8, 0x0C, 0x78, 0x0A, 0xA0, 0x0E, 0x7A, 0x0A, 0xA0, 0x08, 0x74, 0x0A, 0xA0, 0x0A, 0x76, 0x0A, 0xA0,
		0x39, 0x00, 0x08, 0xEB, 0x00, 0x01, 0xE4, 0xE4, 0x44, 0x88, 0x40,
		0x39, 0x00, 0x11, 0xED, 0xFF, 0x45, 0x67, 0xFA, 0x01, 0x2B, 0xAB, 0xFF, 0xFF, 0xBA, 0xB2, 0x10, 0xAF, 0x76, 0x54, 0xFF,
		0x39, 0x00, 0x07, 0xEF, 0x10, 0x0D, 0x04, 0x08, 0x3F, 0x1F,
		0x39, 0x00, 0x06, 0xFF, 0x77, 0x01, 0x00, 0x00, 0x00,
		0x39, 0xff, 0x01, 0x11,
		0x39, 0x00, 0x06, 0xFF, 0x77, 0x01, 0x00, 0x00, 0x13,
		0x39, 0x00, 0x03, 0xE8, 0x00, 0x0E,
		0x39, 0x00, 0x06, 0xff, 0x77, 0x01, 0x00, 0x00, 0x00,
		0x05, 0x78, 0x01, 0x11,
		0x39, 0x00, 0x06, 0xff, 0x77, 0x01, 0x00, 0x00, 0x13,
		0x39, 0x0A, 0x03, 0xE8, 0x00, 0x0C,
		0x39, 0x00, 0x03, 0xE8, 0x00, 0x00,
		0x39, 0x00, 0x06, 0xFF, 0x77, 0x01, 0x00, 0x00, 0x00,
		0x39, 0x00, 0x02, 0x3a, 0x50,
		0x05, 0x14, 0x01, 0x29,
    };

    connector_send_cmd(init_sequence, sizeof(init_sequence), K_FALSE);
}


static void aml020t_power_reset(k_s32 on)
{
    k_u8 rst_gpio;
    if(0 > (rst_gpio = CONFIG_MPP_DSI_LCD_RESET_PIN)) {
        return;
    }
    rt_kprintf("rst_gpio is %d \n",rst_gpio);

    kd_pin_mode(rst_gpio, GPIO_DM_OUTPUT);

    if (on)
        kd_pin_write(rst_gpio, GPIO_PV_HIGH); // GPIO_PV_LOW  GPIO_PV_HIGH
    else
        kd_pin_write(rst_gpio, GPIO_PV_LOW); // GPIO_PV_LOW  GPIO_PV_HIGH

}

static void aml020t_set_backlight(k_s32 on)
{
    k_u8 backlight_gpio;
    if(0 > (backlight_gpio = CONFIG_MPP_DSI_LCD_BACKLIGHT_PIN)) {
        return;
    }

    rt_kprintf("backlight_gpio is %d \n",backlight_gpio);

    kd_pin_mode(backlight_gpio, GPIO_DM_OUTPUT);
    if (on)
        kd_pin_write(backlight_gpio, GPIO_PV_HIGH);
    else
        kd_pin_write(backlight_gpio, GPIO_PV_LOW);

}


static k_s32 aml020t_power_on(void* ctx, k_s32 on)
{
    k_s32 ret = 0;
    struct connector_driver_dev* dev = ctx;
    // rst vo;
    k230_display_rst();

    if (on) {

        // rst aml020t
        aml020t_power_reset(1);
        rt_thread_mdelay(g_blacklight_delay_ms);
        aml020t_power_reset(0);
        rt_thread_mdelay(g_blacklight_delay_ms);
        aml020t_power_reset(1);

        g_blacklight_delay_ms = DELAY_MS_BACKLIGHT_DEFAULT;
        //enable backlight
        aml020t_set_backlight(1);
    } else {
        aml020t_set_backlight(0);
    }

    return ret;
}


static k_s32 aml020t_set_phy_freq(k_connectori_phy_attr *phy_attr)
{
    k_vo_mipi_phy_attr mipi_phy_attr;

    memset(&mipi_phy_attr, 0, sizeof(k_vo_mipi_phy_attr));

    mipi_phy_attr.m = phy_attr->m;
    mipi_phy_attr.n = phy_attr->n;
    mipi_phy_attr.hs_freq = phy_attr->hs_freq;
    mipi_phy_attr.voc = phy_attr->voc;
    mipi_phy_attr.phy_lan_num = K_DSI_4LAN;
    connector_set_phy_freq(&mipi_phy_attr);
    rt_kprintf("config phy success phy_attr->m is %d  phy_attr->n is %d phy_attr->hs_freq is %d phy_attr->voc is %d \n", phy_attr->m,  phy_attr->n, phy_attr->hs_freq, phy_attr->voc);
    return 0;
}


static k_s32 aml020t_dsi_resolution_init(k_connector_info *info)
{
    k_vo_dsi_attr attr;
    k_vo_display_resolution resolution;

    memset(&attr, 0, sizeof(k_vo_dsi_attr));
    attr.lan_num = info->lan_num;
    attr.cmd_mode = info->cmd_mode;
    attr.lp_div = 8;
    attr.work_mode = info->work_mode;
    memcpy(&resolution, &info->resolution, sizeof(k_vo_display_resolution));
    memcpy(&attr.resolution, &resolution, sizeof(k_vo_display_resolution));
    connector_set_dsi_attr(&attr);


    if(info->screen_test_mode)
        aml020t_480x360_init(1);
    else
        aml020t_480x360_init(0);

    connector_set_dsi_enable(1);

    if(info->dsi_test_mode == 1)
        connector_set_dsi_test_mode();

    return 0;
}


static k_s32 aml020t_vo_resolution_init(k_vo_display_resolution *resolution, k_u32 bg_color, k_u32 intr_line)
{
    k_vo_display_resolution vo_resolution;
    k_vo_pub_attr attr;

    memset(&attr, 0, sizeof(k_vo_pub_attr));
    attr.bg_color = bg_color;
    attr.intf_sync = K_VO_OUT_1080P30;
    attr.intf_type = K_VO_INTF_MIPI;
    attr.sync_info = resolution;
    rt_kprintf("info->pclk = %d \n",attr.sync_info->pclk);
    connector_set_vo_init();
    connector_set_vtth_intr(1, intr_line);
    connector_set_vo_param(&attr);
    connector_set_vo_enable();

    return 0;
}


k_s32 aml020t_init(void *ctx, k_connector_info *info)
{
    k_s32 ret = 0;
    struct connector_driver_dev* dev = ctx;

    if(info->pixclk_div != 0)
        connector_set_pixclk(info->pixclk_div);
    rt_kprintf("info->connector_name = %s \n",info->connector_name);
    ret |= connector_set_cmd_buff_num(info->buff_num);
    ret |= aml020t_set_phy_freq(&info->phy_attr);
    ret |= aml020t_dsi_resolution_init(info);
    ret |= aml020t_vo_resolution_init(&info->resolution, info->bg_color, info->intr_line);

    return ret;
}

static k_s32 aml020t_get_chip_id(void* ctx, k_u32* chip_id)
{
    k_s32 ret = 0;

    return ret;
}


static k_s32 aml020t_conn_check(void* ctx, k_s32* conn)
{
    k_s32 ret = 0;

    *conn = 1;

    return ret;
}

struct connector_driver_dev aml020t_connector_drv = {
    .connector_name = "aml020t",
    .connector_func = {
        .connector_power = aml020t_power_on,
        .connector_init = aml020t_init,
        .connector_get_chip_id = aml020t_get_chip_id,
        .connector_conn_check = aml020t_conn_check,
    },
};
