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

#include <string.h>

#include <drv_gpio.h>

#include <drivers/i2c.h>

#include "connector_panel.h"

#include "k_autoconf_comm.h"

#define DBG_TAG "lt9611"
#ifdef RT_DEBUG
#define DBG_LVL DBG_LOG
#else
#define DBG_LVL DBG_WARNING
#endif
#define DBG_COLOR
#include <rtdbg.h>

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(array) (sizeof(array) / sizeof(array[0]))
#endif

typedef struct {
    k_u8 addr;
    k_u8 val;
} k_i2c_reg;

typedef struct {
    k_u16                     slave_addr;
    const char*               i2c_name;
    struct rt_i2c_bus_device* i2c_bus;
    k_s32                     write_status;
} k_i2c_info;

struct lt9611_dev {
#define LT9611_PORTA 100600
#define LT9611_PORTB 100601
    k_u32      input_port;
    k_u32      pcr_m;
    k_i2c_info i2c_info;
};

static struct lt9611_dev* g_lt9611_dev = NULL;

// extern k_u32 k230_display_rst(void);

static k_s32 lt9611_read_reg(k_i2c_info* i2c_info, k_u8 reg_addr, k_u8* reg_val)
{
    struct rt_i2c_msg msg[2];

    RT_ASSERT(i2c_info != RT_NULL);
    msg[0].addr  = i2c_info->slave_addr;
    msg[0].flags = RT_I2C_WR;
    msg[0].len   = 1;
    msg[0].buf   = &reg_addr;

    msg[1].addr  = i2c_info->slave_addr;
    msg[1].flags = RT_I2C_RD;
    msg[1].len   = 1;
    msg[1].buf   = reg_val;

    if (rt_i2c_transfer(i2c_info->i2c_bus, msg, 2) != 2)
        return RT_ERROR;

    return RT_EOK;
}

static k_s32 lt9611_write_reg(k_i2c_info* i2c_info, k_u8 reg_addr, k_u8 reg_val)
{
    struct rt_i2c_msg msg;
    k_u8              buf[2];

    RT_ASSERT(i2c_info != RT_NULL);

    if (i2c_info->write_status != RT_EOK)
        return i2c_info->write_status;

    buf[0]    = reg_addr;
    buf[1]    = reg_val;
    msg.addr  = i2c_info->slave_addr;
    msg.flags = RT_I2C_WR;
    msg.len   = 2;
    msg.buf   = buf;

    if (rt_i2c_transfer(i2c_info->i2c_bus, &msg, 1) != 1) {
        i2c_info->write_status = RT_ERROR;
        return i2c_info->write_status;
    }

    return RT_EOK;
}

static k_s32 lt9611_write_multi_reg(k_i2c_info* i2c_info, const k_i2c_reg* reg_list, k_u32 reg_num)
{
    k_s32 ret = 0;
    k_u32 i;

    for (i = 0; i < reg_num; i++) {
        ret = lt9611_write_reg(i2c_info, reg_list[i].addr, reg_list[i].val);
        if (ret)
            return RT_ERROR;
    }

    return ret;
}

static void lt9611_reset(k_u8 lt9611_reset_pin)
{
    if (0 > lt9611_reset_pin) {
        return;
    }

    kd_pin_mode(lt9611_reset_pin, GPIO_DM_OUTPUT);
    kd_pin_write(lt9611_reset_pin, GPIO_PV_LOW);
    rt_thread_mdelay(100);

    kd_pin_write(lt9611_reset_pin, GPIO_PV_HIGH);
    rt_thread_mdelay(100);
}

static k_s32 lt9611_set_interface(struct lt9611_dev* lt9611_dev)
{
    lt9611_write_reg(&lt9611_dev->i2c_info, 0xff, 0x80);
    lt9611_write_reg(&lt9611_dev->i2c_info, 0xee, 0x01);

    return lt9611_dev->i2c_info.write_status;
}

static k_s32 lt9611_init_system(struct lt9611_dev* lt9611_dev)
{
    const k_i2c_reg system_regs[] = {
        { 0xff, 0x81 }, { 0x01, 0x18 }, { 0xff, 0x82 }, { 0x51, 0x11 },

        { 0xff, 0x82 }, { 0x1b, 0x69 }, { 0x1c, 0x78 }, { 0xcb, 0x69 }, { 0xcc, 0x78 },

        { 0xff, 0x80 }, { 0x04, 0xf0 }, { 0x06, 0xf0 }, { 0x0a, 0x80 }, { 0x0b, 0x46 }, { 0x0d, 0xef }, { 0x11, 0xfa },
    };

    return lt9611_write_multi_reg(&lt9611_dev->i2c_info, system_regs, ARRAY_SIZE(system_regs));
}

static k_s32 lt9611_mipi_input_analog(struct lt9611_dev* lt9611_dev)
{
    const k_i2c_reg mipi_input_analog_regs[] = {
        { 0xff, 0x81 }, { 0x06, 0x60 }, { 0x07, 0x3f }, { 0x08, 0x3f }, { 0x0a, 0xfe }, { 0x0b, 0xbf },

        { 0x11, 0x60 }, { 0x12, 0x3f }, { 0x13, 0x3f }, { 0x15, 0xfe }, { 0x16, 0xbf },

        { 0x1c, 0x03 }, { 0x20, 0x03 },
    };

    return lt9611_write_multi_reg(&lt9611_dev->i2c_info, mipi_input_analog_regs, ARRAY_SIZE(mipi_input_analog_regs));
}

static k_s32 lt9611_mipi_input_digital(struct lt9611_dev* lt9611_dev)
{
    if (lt9611_dev->input_port == LT9611_PORTA) {
        lt9611_write_reg(&lt9611_dev->i2c_info, 0xff, 0x82);
        lt9611_write_reg(&lt9611_dev->i2c_info, 0x50, 0x10);
        lt9611_write_reg(&lt9611_dev->i2c_info, 0xff, 0x83);
        lt9611_write_reg(&lt9611_dev->i2c_info, 0x03, 0x00);
    } else if (lt9611_dev->input_port == LT9611_PORTB) {
        lt9611_write_reg(&lt9611_dev->i2c_info, 0xff, 0x82);
        lt9611_write_reg(&lt9611_dev->i2c_info, 0x50, 0x14);
        lt9611_write_reg(&lt9611_dev->i2c_info, 0xff, 0x83);
        lt9611_write_reg(&lt9611_dev->i2c_info, 0x00, 0x60);
        lt9611_write_reg(&lt9611_dev->i2c_info, 0x03, 0x4f);
        lt9611_write_reg(&lt9611_dev->i2c_info, 0x04, 0x00);
        lt9611_write_reg(&lt9611_dev->i2c_info, 0x07, 0x40);
    }

    lt9611_write_reg(&lt9611_dev->i2c_info, 0xff, 0x82);
    lt9611_write_reg(&lt9611_dev->i2c_info, 0x4f, 0x80);
    lt9611_write_reg(&lt9611_dev->i2c_info, 0xff, 0x83);
    lt9611_write_reg(&lt9611_dev->i2c_info, 0x02, 0x08);
    lt9611_write_reg(&lt9611_dev->i2c_info, 0x06, 0x08);
    lt9611_write_reg(&lt9611_dev->i2c_info, 0x0a, 0x00);

    return lt9611_dev->i2c_info.write_status;
}

static k_s32 lt9611_setup_pll(struct lt9611_dev* lt9611_dev, k_u32 pclk)
{
    k_u32 postdiv = 0;

    const k_i2c_reg pll_regs[] = {
        { 0xff, 0x81 }, { 0x23, 0x40 }, { 0x24, 0x62 }, { 0x25, 0x80 }, { 0x26, 0x55 },
        { 0x2c, 0x37 }, { 0x2f, 0x01 }, { 0x27, 0x66 }, { 0x28, 0x88 }, { 0x2a, 0x20 },
    };

    lt9611_write_multi_reg(&lt9611_dev->i2c_info, pll_regs, ARRAY_SIZE(pll_regs));

    if (pclk > 150000) {
        lt9611_write_reg(&lt9611_dev->i2c_info, 0x2d, 0x88);
        postdiv = 1;
    } else if (pclk > 80000) {
        lt9611_write_reg(&lt9611_dev->i2c_info, 0x2d, 0x99);
        postdiv = 2;
    } else {
        lt9611_write_reg(&lt9611_dev->i2c_info, 0x2d, 0xaa);
        postdiv = 4;
    }
    lt9611_dev->pcr_m = ((pclk * 5 * postdiv) / 27000) - 1;

    lt9611_write_reg(&lt9611_dev->i2c_info, 0xff, 0x83);
    lt9611_write_reg(&lt9611_dev->i2c_info, 0x2d, 0x40);
    lt9611_write_reg(&lt9611_dev->i2c_info, 0x31, 0x08);
    lt9611_write_reg(&lt9611_dev->i2c_info, 0x26, 0x80 | lt9611_dev->pcr_m);

    lt9611_write_reg(&lt9611_dev->i2c_info, 0xff, 0x82);
    pclk = pclk / 2;
    lt9611_write_reg(&lt9611_dev->i2c_info, 0xe3, pclk / 65536);
    pclk = pclk % 65536;
    lt9611_write_reg(&lt9611_dev->i2c_info, 0xe4, pclk / 256);
    lt9611_write_reg(&lt9611_dev->i2c_info, 0xe5, pclk % 256);

    lt9611_write_reg(&lt9611_dev->i2c_info, 0xff, 0x82);
    lt9611_write_reg(&lt9611_dev->i2c_info, 0xde, 0x20);
    lt9611_write_reg(&lt9611_dev->i2c_info, 0xde, 0xe0);

    lt9611_write_reg(&lt9611_dev->i2c_info, 0xff, 0x80);
    lt9611_write_reg(&lt9611_dev->i2c_info, 0x11, 0x5a);
    lt9611_write_reg(&lt9611_dev->i2c_info, 0x11, 0xfa);
    lt9611_write_reg(&lt9611_dev->i2c_info, 0x16, 0xf2);
    lt9611_write_reg(&lt9611_dev->i2c_info, 0x18, 0xdc);
    lt9611_write_reg(&lt9611_dev->i2c_info, 0x18, 0xfc);
    lt9611_write_reg(&lt9611_dev->i2c_info, 0x16, 0xf3);

    return lt9611_dev->i2c_info.write_status;
}

static k_s32 lt9611_setup_pcr(struct lt9611_dev* lt9611_dev)
{
    const k_i2c_reg pcr_regs[] = {
        { 0xff, 0x83 }, { 0x0b, 0x01 }, { 0x0c, 0x10 }, { 0x48, 0x00 }, { 0x49, 0x81 }, { 0x21, 0x4a },
        { 0x24, 0x71 }, { 0x25, 0x30 }, { 0x2a, 0x01 }, { 0x4a, 0x40 }, { 0x2d, 0x40 }, { 0x31, 0x08 },
    };
    lt9611_write_multi_reg(&lt9611_dev->i2c_info, pcr_regs, ARRAY_SIZE(pcr_regs));

    lt9611_write_reg(&lt9611_dev->i2c_info, 0xff, 0x83);
    lt9611_write_reg(&lt9611_dev->i2c_info, 0x1d, 0x10);
    lt9611_write_reg(&lt9611_dev->i2c_info, 0x26, lt9611_dev->pcr_m);

    lt9611_write_reg(&lt9611_dev->i2c_info, 0xff, 0x80);
    lt9611_write_reg(&lt9611_dev->i2c_info, 0x11, 0x5a);
    lt9611_write_reg(&lt9611_dev->i2c_info, 0x11, 0xfa);

    return lt9611_dev->i2c_info.write_status;
}

static k_s32 lt9611_setup_timing(struct lt9611_dev* lt9611_dev, k_vo_timing* resolution)
{
    k_u32 hactive      = resolution->hactive;
    k_u32 hsync_len    = resolution->hsync_len;
    k_u32 hback_porch  = resolution->hback_porch;
    k_u32 hfront_porch = resolution->hfront_porch;

    k_u32 htotal = hactive + hsync_len + hback_porch + hfront_porch;

    k_u32 vactive      = resolution->vactive;
    k_u32 vsync_len    = resolution->vsync_len;
    k_u32 vback_porch  = resolution->vback_porch;
    k_u32 vfront_porch = resolution->vfront_porch;

    k_u32 vtotal = vactive + vsync_len + vback_porch + vfront_porch;

    lt9611_write_reg(&lt9611_dev->i2c_info, 0xff, 0x83);
    lt9611_write_reg(&lt9611_dev->i2c_info, 0x0d, (k_u8)(vtotal / 256));
    lt9611_write_reg(&lt9611_dev->i2c_info, 0x0e, (k_u8)(vtotal % 256));

    lt9611_write_reg(&lt9611_dev->i2c_info, 0x0f, (k_u8)(vactive / 256));
    lt9611_write_reg(&lt9611_dev->i2c_info, 0x10, (k_u8)(vactive % 256));

    lt9611_write_reg(&lt9611_dev->i2c_info, 0x11, (k_u8)(htotal / 256));
    lt9611_write_reg(&lt9611_dev->i2c_info, 0x12, (k_u8)(htotal % 256));

    lt9611_write_reg(&lt9611_dev->i2c_info, 0x13, (k_u8)(hactive / 256));
    lt9611_write_reg(&lt9611_dev->i2c_info, 0x14, (k_u8)(hactive % 256));

    lt9611_write_reg(&lt9611_dev->i2c_info, 0x15, (k_u8)(vsync_len % 256));
    lt9611_write_reg(&lt9611_dev->i2c_info, 0x16, (k_u8)(hsync_len % 256));

    lt9611_write_reg(&lt9611_dev->i2c_info, 0x17, (k_u8)(vfront_porch % 256));
    lt9611_write_reg(&lt9611_dev->i2c_info, 0x18, (k_u8)((vsync_len + vback_porch) % 256));

    lt9611_write_reg(&lt9611_dev->i2c_info, 0x19, (k_u8)(hfront_porch % 256));
    lt9611_write_reg(&lt9611_dev->i2c_info, 0x1a, (k_u8)(((hfront_porch / 256) << 4) + (hsync_len + hback_porch) / 256));
    lt9611_write_reg(&lt9611_dev->i2c_info, 0x1b, (k_u8)((hsync_len + hback_porch) % 256));

    return lt9611_dev->i2c_info.write_status;
}

static k_s32 lt9611_hdmi_tx_digital(struct lt9611_dev* lt9611_dev)
{
    lt9611_write_reg(&lt9611_dev->i2c_info, 0xff, 0x84);
    lt9611_write_reg(&lt9611_dev->i2c_info, 0x43, 0x21);
    lt9611_write_reg(&lt9611_dev->i2c_info, 0x45, 0x40);
    lt9611_write_reg(&lt9611_dev->i2c_info, 0x47, 0x34);
    lt9611_write_reg(&lt9611_dev->i2c_info, 0x3d, 0x0a);

    lt9611_write_reg(&lt9611_dev->i2c_info, 0xff, 0x82);
    lt9611_write_reg(&lt9611_dev->i2c_info, 0xd6, 0x8e);
    lt9611_write_reg(&lt9611_dev->i2c_info, 0xd7, 0x04);

    return lt9611_dev->i2c_info.write_status;
}

static k_s32 lt9611_hdmi_tx_phy(struct lt9611_dev* lt9611_dev)
{
    const k_i2c_reg hdmi_tx_phy_regs[] = {
        { 0xff, 0x81 }, { 0x30, 0x6a }, { 0x31, 0x44 }, { 0x32, 0x4a }, { 0x33, 0x0b },
        { 0x34, 0x00 }, { 0x35, 0x00 }, { 0x36, 0x00 }, { 0x37, 0x44 }, { 0x3f, 0x0f },
        { 0x40, 0x98 }, { 0x41, 0x98 }, { 0x42, 0x98 }, { 0x43, 0x98 }, { 0x44, 0x0a },
    };

    return lt9611_write_multi_reg(&lt9611_dev->i2c_info, hdmi_tx_phy_regs, ARRAY_SIZE(hdmi_tx_phy_regs));
}

static k_s32 lt9611_irq_init(struct lt9611_dev* lt9611_dev)
{
    lt9611_write_reg(&lt9611_dev->i2c_info, 0xff, 0x82);
    lt9611_write_reg(&lt9611_dev->i2c_info, 0x58, 0x0a);
    lt9611_write_reg(&lt9611_dev->i2c_info, 0x59, 0x00);
    lt9611_write_reg(&lt9611_dev->i2c_info, 0x9e, 0xf7);

    return lt9611_dev->i2c_info.write_status;
}

static k_s32 lt9611_enable_hpd_interrupts(struct lt9611_dev* lt9611_dev)
{
    lt9611_write_reg(&lt9611_dev->i2c_info, 0xff, 0x82);
    lt9611_write_reg(&lt9611_dev->i2c_info, 0x07, 0xff);
    lt9611_write_reg(&lt9611_dev->i2c_info, 0x07, 0x3f);
    lt9611_write_reg(&lt9611_dev->i2c_info, 0x03, 0x3f);

    return lt9611_dev->i2c_info.write_status;
}

static k_s32 lt9611_enable_hdmi_out(struct lt9611_dev* lt9611_dev)
{
    lt9611_write_reg(&lt9611_dev->i2c_info, 0xff, 0x81);
    lt9611_write_reg(&lt9611_dev->i2c_info, 0x23, 0x40);

    lt9611_write_reg(&lt9611_dev->i2c_info, 0xff, 0x82);
    lt9611_write_reg(&lt9611_dev->i2c_info, 0xde, 0x20);
    lt9611_write_reg(&lt9611_dev->i2c_info, 0xde, 0xe0);

    lt9611_write_reg(&lt9611_dev->i2c_info, 0xff, 0x80);
    lt9611_write_reg(&lt9611_dev->i2c_info, 0x18, 0xdc);
    lt9611_write_reg(&lt9611_dev->i2c_info, 0x18, 0xfc);
    lt9611_write_reg(&lt9611_dev->i2c_info, 0x16, 0xf1);
    lt9611_write_reg(&lt9611_dev->i2c_info, 0x16, 0xf3);
    lt9611_write_reg(&lt9611_dev->i2c_info, 0x11, 0x5a);
    lt9611_write_reg(&lt9611_dev->i2c_info, 0x11, 0xfa);

    lt9611_write_reg(&lt9611_dev->i2c_info, 0xff, 0x81);
    lt9611_write_reg(&lt9611_dev->i2c_info, 0x30, 0xea);

    return lt9611_dev->i2c_info.write_status;
}

static struct lt9611_dev* lt9611_dev_create(k_u32 input_port, k_u16 slave_addr, const char* i2c_name)
{
    struct rt_i2c_bus_device* i2c_bus;
    struct lt9611_dev*        lt9611_dev;

    lt9611_dev = rt_malloc(sizeof(struct lt9611_dev));
    if (lt9611_dev == RT_NULL)
        return RT_NULL;

    i2c_bus = rt_i2c_bus_device_find(i2c_name);
    if (i2c_bus == RT_NULL) {
        LOG_E("can't find %s deivce \n", i2c_name);
        return RT_NULL;
    }

    lt9611_dev->input_port            = input_port;
    lt9611_dev->i2c_info.i2c_bus      = i2c_bus;
    lt9611_dev->i2c_info.i2c_name     = i2c_name;
    lt9611_dev->i2c_info.slave_addr   = slave_addr;
    lt9611_dev->i2c_info.write_status = RT_EOK;

    return lt9611_dev;
}

static int lt9611_panel_init(const struct panel_desc* desc)
{
    k_vo_timing timing = desc->timing;

    k_u32 pclk = timing.pclk_khz;
    k_s32 ret;

    // k230_display_rst();
    lt9611_reset(CONFIG_MPP_DSI_HDMI_RESET_PIN);

    if (g_lt9611_dev == NULL) {
        g_lt9611_dev = lt9611_dev_create(LT9611_PORTB, CONFIG_MPP_DSI_LT9611_I2C_SLV_ADDR, CONFIG_MPP_DSI_HDMI_I2C_DEV);
        if (g_lt9611_dev == NULL) {
            LOG_E("lt9611_dev_create failed \n");
            return K_FAILED;
        }
    }

    g_lt9611_dev->i2c_info.write_status = RT_EOK;

    ret = lt9611_set_interface(g_lt9611_dev);
    if (ret)
        goto init_failed;

    ret = lt9611_init_system(g_lt9611_dev);
    if (ret)
        goto init_failed;

    ret = lt9611_mipi_input_analog(g_lt9611_dev);
    if (ret)
        goto init_failed;

    ret = lt9611_mipi_input_digital(g_lt9611_dev);
    if (ret)
        goto init_failed;

    ret = lt9611_setup_pll(g_lt9611_dev, pclk);
    if (ret)
        goto init_failed;

    ret = lt9611_setup_pcr(g_lt9611_dev);
    if (ret)
        goto init_failed;

    ret = lt9611_setup_timing(g_lt9611_dev, &timing);
    if (ret)
        goto init_failed;

    ret = lt9611_hdmi_tx_digital(g_lt9611_dev);
    if (ret)
        goto init_failed;

    ret = lt9611_hdmi_tx_phy(g_lt9611_dev);
    if (ret)
        goto init_failed;

    ret = lt9611_irq_init(g_lt9611_dev);
    if (ret)
        goto init_failed;

    ret = lt9611_enable_hpd_interrupts(g_lt9611_dev);
    if (ret)
        goto init_failed;

    ret = lt9611_enable_hdmi_out(g_lt9611_dev);
    if (ret)
        goto init_failed;

    return K_SUCCESS;

init_failed:
    g_panel_init_status = K_FALSE;
    LOG_W("lt9611 init failed, but we treat it as non-fatal\n");
    return K_SUCCESS;
}

static const struct panel_ops lt9611_ops = {
    .reset        = NULL, // user custom reset
    .init         = lt9611_panel_init,
    .power_off    = NULL,
    .read_chip_id = NULL,
};

/* K230 Custom Timings */
static const struct panel_desc lt9611_panel_desc_res_1080p60 = {
    .name = "lt9611_1080p60",
    .connector_type = LT9611_1920_1080_HDMI_V2,
    .bus_type = PANEL_BUS_DSI,
    .timing = {
        .pclk_khz = 148500,
        .hactive = 1920,
        .hsync_len = 25,
        .hback_porch = 50,
        .hfront_porch = 25,
        .vactive = 1080,
        .vsync_len = 36,
        .vback_porch = 54,
        .vfront_porch = 55,
    },
    .bg_color = PANEL_BG_COLOR_BLACK,
    .bus.dsi = {
        .lanes = K_DSI_4LANE,
        .cmd_mode = K_DSI_CMD_LP_MODE,
        .video_mode = K_DSI_VIDEO_BURST_MODE,
        .vc_id = 0,
    },
    .gpio = {
        .reset_pin = CONFIG_MPP_DSI_HDMI_RESET_PIN,
        .backlight_pin = -1,
        .reset_delay_ms = 100,
        .backlight_delay_ms = 0,
        .reset_active_low = K_TRUE,
        .backlight_active_low = K_FALSE,
    },
    .bus_ops = &dsi_bus_ops,
    .ops = &lt9611_ops,
};

static const struct panel_desc lt9611_panel_desc_res_1080p30 = {
    .name = "lt9611_1080p30",
    .connector_type = LT9611_1920_1080_HDMI_V1,
    .bus_type = PANEL_BUS_DSI,
    .timing = {
        .pclk_khz = 74250,
        .hactive = 1920,
        .hsync_len = 25,
        .hback_porch = 50,
        .hfront_porch = 25,
        .vactive = 1080,
        .vsync_len = 36,
        .vback_porch = 54,
        .vfront_porch = 54,
    },
    .bg_color = PANEL_BG_COLOR_BLACK,
    .bus.dsi = {
        .lanes = K_DSI_4LANE,
        .cmd_mode = K_DSI_CMD_LP_MODE,
        .video_mode = K_DSI_VIDEO_BURST_MODE,
        .vc_id = 0,
    },
    .gpio = {
        .reset_pin = CONFIG_MPP_DSI_HDMI_RESET_PIN,
        .backlight_pin = -1,
        .reset_delay_ms = 100,
        .backlight_delay_ms = 0,
        .reset_active_low = K_TRUE,
        .backlight_active_low = K_FALSE,
    },
    .bus_ops = &dsi_bus_ops,
    .ops = &lt9611_ops,
};

static const struct panel_desc lt9611_panel_desc_res_720p60 = {
    .name = "lt9611_720p60",
    .connector_type = LT9611_1280_720_HDMI_V1,
    .bus_type = PANEL_BUS_DSI,
    .timing = {
        .pclk_khz = 66000,
        .hactive = 1280,
        .hsync_len = 25,
        .hback_porch = 50,
        .hfront_porch = 25,
        .vactive = 720,
        .vsync_len = 19,
        .vback_porch = 28,
        .vfront_porch = 30,
    },
    .bg_color = PANEL_BG_COLOR_BLACK,
    .bus.dsi = {
        .lanes = K_DSI_4LANE,
        .cmd_mode = K_DSI_CMD_LP_MODE,
        .video_mode = K_DSI_VIDEO_BURST_MODE,
        .vc_id = 0,
    },
    .gpio = {
        .reset_pin = CONFIG_MPP_DSI_HDMI_RESET_PIN,
        .backlight_pin = -1,
        .reset_delay_ms = 100,
        .backlight_delay_ms = 0,
        .reset_active_low = K_TRUE,
        .backlight_active_low = K_FALSE,
    },
    .bus_ops = &dsi_bus_ops,
    .ops = &lt9611_ops,
};

static const struct panel_desc lt9611_panel_desc_res_720p50 = {
    .name = "lt9611_720p50",
    .connector_type = LT9611_1280_720_HDMI_V2,
    .bus_type = PANEL_BUS_DSI,
    .timing = {
        .pclk_khz = 54000,
        .hactive = 1280,
        .hsync_len = 25,
        .hback_porch = 50,
        .hfront_porch = 25,
        .vactive = 720,
        .vsync_len = 15,
        .vback_porch = 23,
        .vfront_porch = 24,
    },
    .bg_color = PANEL_BG_COLOR_BLACK,
    .bus.dsi = {
        .lanes = K_DSI_4LANE,
        .cmd_mode = K_DSI_CMD_LP_MODE,
        .video_mode = K_DSI_VIDEO_BURST_MODE,
        .vc_id = 0,
    },
    .gpio = {
        .reset_pin = CONFIG_MPP_DSI_HDMI_RESET_PIN,
        .backlight_pin = -1,
        .reset_delay_ms = 100,
        .backlight_delay_ms = 0,
        .reset_active_low = K_TRUE,
        .backlight_active_low = K_FALSE,
    },
    .bus_ops = &dsi_bus_ops,
    .ops = &lt9611_ops,
};

static const struct panel_desc lt9611_panel_desc_res_720p30 = {
    .name = "lt9611_720p30",
    .connector_type = LT9611_1280_720_HDMI_V3,
    .bus_type = PANEL_BUS_DSI,
    .timing = {
        .pclk_khz = 33000,
        .hactive = 1280,
        .hsync_len = 25,
        .hback_porch = 50,
        .hfront_porch = 25,
        .vactive = 720,
        .vsync_len = 19,
        .vback_porch = 28,
        .vfront_porch = 30,
    },
    .bg_color = PANEL_BG_COLOR_BLACK,
    .bus.dsi = {
        .lanes = K_DSI_4LANE,
        .cmd_mode = K_DSI_CMD_LP_MODE,
        .video_mode = K_DSI_VIDEO_BURST_MODE,
        .vc_id = 0,
    },
    .gpio = {
        .reset_pin = CONFIG_MPP_DSI_HDMI_RESET_PIN,
        .backlight_pin = -1,
        .reset_delay_ms = 100,
        .backlight_delay_ms = 0,
        .reset_active_low = K_TRUE,
        .backlight_active_low = K_FALSE,
    },
    .bus_ops = &dsi_bus_ops,
    .ops = &lt9611_ops,
};

static const struct panel_desc lt9611_panel_desc_res_480p60 = {
    .name = "lt9611_480p60",
    .connector_type = LT9611_640_480_HDMI_V1,
    .bus_type = PANEL_BUS_DSI,

    .timing = {
        .pclk_khz = 23760,
        .hactive = 640,
        .hsync_len = 25,
        .hback_porch = 50,
        .hfront_porch = 25,
        .vactive = 480,
        .vsync_len = 13,
        .vback_porch = 20,
        .vfront_porch = 22,
    },

    .bg_color = PANEL_BG_COLOR_BLACK,

    .bus.dsi = {
        .lanes = K_DSI_4LANE,
        .cmd_mode = K_DSI_CMD_LP_MODE,
        .video_mode = K_DSI_VIDEO_BURST_MODE,
        .vc_id = 0,
    },

    .gpio = {
        .reset_pin = CONFIG_MPP_DSI_HDMI_RESET_PIN,
        .backlight_pin = -1,
        .reset_delay_ms = 100,
        .backlight_delay_ms = 0,
        .reset_active_low = K_TRUE,
        .backlight_active_low = K_FALSE,
    },

    .bus_ops = &dsi_bus_ops,
    .ops = &lt9611_ops,
};

/* VESA Timings */
static const struct panel_desc lt9611_panel_desc_res_1080p60_vesa = {
    .name = "lt9611_1080p60_vesa",
    .connector_type = LT9611_1920_1080_HDMI_V3,
    .bus_type = PANEL_BUS_DSI,
    .timing = {
        .pclk_khz = 148500,
        .hactive = 1920,
        .hsync_len = 44,
        .hback_porch = 148,
        .hfront_porch = 88,
        .vactive = 1080,
        .vsync_len = 5,
        .vback_porch = 36,
        .vfront_porch = 4,
    },
    .bg_color = PANEL_BG_COLOR_BLACK,
    .bus.dsi = {
        .lanes = K_DSI_4LANE,
        .cmd_mode = K_DSI_CMD_LP_MODE,
        .video_mode = K_DSI_VIDEO_BURST_MODE,
        .vc_id = 0,
    },
    .gpio = {
        .reset_pin = CONFIG_MPP_DSI_HDMI_RESET_PIN,
        .backlight_pin = -1,
        .reset_delay_ms = 100,
        .backlight_delay_ms = 0,
        .reset_active_low = K_TRUE,
        .backlight_active_low = K_FALSE,
    },
    .bus_ops = &dsi_bus_ops,
    .ops = &lt9611_ops,
};

static const struct panel_desc lt9611_panel_desc_res_1080p30_vesa = {
    .name = "lt9611_1080p30_vesa",
    .connector_type = LT9611_1920_1080_HDMI_V4,
    .bus_type = PANEL_BUS_DSI,
    .timing = {
        .pclk_khz = 74250,
        .hactive = 1920,
        .hsync_len = 44,
        .hback_porch = 148,
        .hfront_porch = 88,
        .vactive = 1080,
        .vsync_len = 5,
        .vback_porch = 36,
        .vfront_porch = 4,
    },
    .bg_color = PANEL_BG_COLOR_BLACK,
    .bus.dsi = {
        .lanes = K_DSI_4LANE,
        .cmd_mode = K_DSI_CMD_LP_MODE,
        .video_mode = K_DSI_VIDEO_BURST_MODE,
        .vc_id = 0,
    },
    .gpio = {
        .reset_pin = CONFIG_MPP_DSI_HDMI_RESET_PIN,
        .backlight_pin = -1,
        .reset_delay_ms = 100,
        .backlight_delay_ms = 0,
        .reset_active_low = K_TRUE,
        .backlight_active_low = K_FALSE,
    },
    .bus_ops = &dsi_bus_ops,
    .ops = &lt9611_ops,
};

static const struct panel_desc lt9611_panel_desc_res_720p60_vesa = {
    .name = "lt9611_720p60_vesa",
    .connector_type = LT9611_1280_720_HDMI_V4,
    .bus_type = PANEL_BUS_DSI,
    .timing = {
        .pclk_khz = 74250,
        .hactive = 1280,
        .hsync_len = 40,
        .hback_porch = 220,
        .hfront_porch = 110,
        .vactive = 720,
        .vsync_len = 5,
        .vback_porch = 20,
        .vfront_porch = 5,
    },
    .bg_color = PANEL_BG_COLOR_BLACK,
    .bus.dsi = {
        .lanes = K_DSI_4LANE,
        .cmd_mode = K_DSI_CMD_LP_MODE,
        .video_mode = K_DSI_VIDEO_BURST_MODE,
        .vc_id = 0,
    },
    .gpio = {
        .reset_pin = CONFIG_MPP_DSI_HDMI_RESET_PIN,
        .backlight_pin = -1,
        .reset_delay_ms = 100,
        .backlight_delay_ms = 0,
        .reset_active_low = K_TRUE,
        .backlight_active_low = K_FALSE,
    },
    .bus_ops = &dsi_bus_ops,
    .ops = &lt9611_ops,
};

static const struct panel_desc lt9611_panel_desc_res_720p50_vesa = {
    .name = "lt9611_720p50_vesa",
    .connector_type = LT9611_1280_720_HDMI_V5,
    .bus_type = PANEL_BUS_DSI,
    .timing = {
        .pclk_khz = 74250,
        .hactive = 1280,
        .hsync_len = 40,
        .hback_porch = 220,
        .hfront_porch = 440,
        .vactive = 720,
        .vsync_len = 5,
        .vback_porch = 20,
        .vfront_porch = 5,
    },
    .bg_color = PANEL_BG_COLOR_BLACK,
    .bus.dsi = {
        .lanes = K_DSI_4LANE,
        .cmd_mode = K_DSI_CMD_LP_MODE,
        .video_mode = K_DSI_VIDEO_BURST_MODE,
        .vc_id = 0,
    },
    .gpio = {
        .reset_pin = CONFIG_MPP_DSI_HDMI_RESET_PIN,
        .backlight_pin = -1,
        .reset_delay_ms = 100,
        .backlight_delay_ms = 0,
        .reset_active_low = K_TRUE,
        .backlight_active_low = K_FALSE,
    },
    .bus_ops = &dsi_bus_ops,
    .ops = &lt9611_ops,
};

static const struct panel_desc lt9611_panel_desc_res_720p30_vesa = {
    .name = "lt9611_720p30_vesa",
    .connector_type = LT9611_1280_720_HDMI_V6,
    .bus_type = PANEL_BUS_DSI,
    .timing = {
        .pclk_khz = 74250,
        .hactive = 1280,
        .hsync_len = 40,
        .hback_porch = 220,
        .hfront_porch = 1760,
        .vactive = 720,
        .vsync_len = 5,
        .vback_porch = 20,
        .vfront_porch = 5,
    },
    .bg_color = PANEL_BG_COLOR_BLACK,
    .bus.dsi = {
        .lanes = K_DSI_4LANE,
        .cmd_mode = K_DSI_CMD_LP_MODE,
        .video_mode = K_DSI_VIDEO_BURST_MODE,
        .vc_id = 0,
    },
    .gpio = {
        .reset_pin = CONFIG_MPP_DSI_HDMI_RESET_PIN,
        .backlight_pin = -1,
        .reset_delay_ms = 100,
        .backlight_delay_ms = 0,
        .reset_active_low = K_TRUE,
        .backlight_active_low = K_FALSE,
    },
    .bus_ops = &dsi_bus_ops,
    .ops = &lt9611_ops,
};

static const struct panel_desc lt9611_panel_desc_res_480p60_vesa = {
    .name = "lt9611_480p60_vesa",
    .connector_type = LT9611_640_480_HDMI_V2,
    .bus_type = PANEL_BUS_DSI,

    .timing = {
        .pclk_khz = 25175,
        .hactive = 640,
        .hsync_len = 96,
        .hback_porch = 48,
        .hfront_porch = 16,
        .vactive = 480,
        .vsync_len = 2,
        .vback_porch = 33,
        .vfront_porch = 10,
    },

    .bg_color = PANEL_BG_COLOR_BLACK,

    .bus.dsi = {
        .lanes = K_DSI_4LANE,
        .cmd_mode = K_DSI_CMD_LP_MODE,
        .video_mode = K_DSI_VIDEO_BURST_MODE,
        .vc_id = 0,
    },

    .gpio = {
        .reset_pin = CONFIG_MPP_DSI_HDMI_RESET_PIN,
        .backlight_pin = -1,
        .reset_delay_ms = 100,
        .backlight_delay_ms = 0,
        .reset_active_low = K_TRUE,
        .backlight_active_low = K_FALSE,
    },

    .bus_ops = &dsi_bus_ops,
    .ops = &lt9611_ops,
};

static const struct panel_desc* lt9611_panel_variants[] = {
    /* K230 Custom Timings */
    &lt9611_panel_desc_res_1080p60,
    &lt9611_panel_desc_res_1080p30,
    &lt9611_panel_desc_res_720p60,
    &lt9611_panel_desc_res_720p50,
    &lt9611_panel_desc_res_720p30,
    &lt9611_panel_desc_res_480p60,
    /* VESA Timings */
    &lt9611_panel_desc_res_1080p60_vesa,
    &lt9611_panel_desc_res_1080p30_vesa,
    &lt9611_panel_desc_res_720p60_vesa,
    &lt9611_panel_desc_res_720p50_vesa,
    &lt9611_panel_desc_res_720p30_vesa,
    &lt9611_panel_desc_res_480p60_vesa,
    NULL,
};

struct panel_drv hdmi_lt9611_drv = {
    .connector_name = "lt9611",
    .panel_variants = lt9611_panel_variants,
    .active_panel   = &lt9611_panel_desc_res_1080p60,
};
