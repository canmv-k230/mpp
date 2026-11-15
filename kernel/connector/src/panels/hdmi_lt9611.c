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
#include "connector_bus_dsi.h"
#include "hdmi_edid.h"

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
/* Between init and power-off: a mode is up, the chip must not be reset. */
static k_bool g_lt9611_running = K_FALSE;

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

static void lt9611_reset(k_s32 lt9611_reset_pin)
{
    if (lt9611_reset_pin < 0) {
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

/* The chip ID registers (bank 0x80, 0x00..0x02, readable with 0xee = 1). A
 * failed read means nothing answers at the address: no chip, no power or a
 * wrong address, told apart from a chip that answers but shows no picture. */
static k_s32 lt9611_read_id(struct lt9611_dev* lt9611_dev, k_u32* id)
{
    k_u8 b0, b1, b2;

    lt9611_dev->i2c_info.write_status = RT_EOK;
    if (lt9611_set_interface(lt9611_dev) != RT_EOK ||
        lt9611_read_reg(&lt9611_dev->i2c_info, 0x00, &b0) != RT_EOK ||
        lt9611_read_reg(&lt9611_dev->i2c_info, 0x01, &b1) != RT_EOK ||
        lt9611_read_reg(&lt9611_dev->i2c_info, 0x02, &b2) != RT_EOK)
        return RT_ERROR;
    *id = ((k_u32)b0 << 16) | ((k_u32)b1 << 8) | b2;
    return RT_EOK;
}

/* A 16-bit input timing value the chip measured (bank 0x82). */
static k_s32 lt9611_read_video_check(struct lt9611_dev* lt9611_dev, k_u8 reg, k_u32* val)
{
    k_u8 hi, lo;

    if (lt9611_write_reg(&lt9611_dev->i2c_info, 0xff, 0x82) != RT_EOK ||
        lt9611_read_reg(&lt9611_dev->i2c_info, reg, &hi) != RT_EOK ||
        lt9611_read_reg(&lt9611_dev->i2c_info, reg + 1, &lo) != RT_EOK)
        return RT_ERROR;
    *val = ((k_u32)hi << 8) | lo;
    return RT_EOK;
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
    /* Written as 0x80 | pcr_m: 7 bits. */
    if (pclk == 0 || (pclk * 5 * postdiv) / 27000 < 1 || (pclk * 5 * postdiv) / 27000 - 1 > 0x7f) {
        LOG_E("pixel clock %u kHz is outside the PCR range\n", pclk);
        return -RT_EINVAL;
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
        LOG_E("can't find %s device\n", i2c_name);
        rt_free(lt9611_dev);
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
    g_lt9611_running = K_TRUE;
    lt9611_reset(CONFIG_MPP_DSI_HDMI_RESET_PIN);

    if (g_lt9611_dev == NULL) {
        g_lt9611_dev = lt9611_dev_create(LT9611_PORTB, CONFIG_MPP_DSI_LT9611_I2C_SLV_ADDR, CONFIG_MPP_DSI_HDMI_I2C_DEV);
        if (g_lt9611_dev == NULL) {
            LOG_E("lt9611_dev_create failed \n");
            return K_FAILED;
        }
    }

    {
        k_u32 id;

        if (lt9611_read_id(g_lt9611_dev, &id) != RT_EOK) {
            LOG_E("no LT9611 answers on %s at 0x%02x: not fitted, unpowered or wrong address\n",
                  CONFIG_MPP_DSI_HDMI_I2C_DEV, CONFIG_MPP_DSI_LT9611_I2C_SLV_ADDR);
            ret = RT_ERROR;
            goto init_failed;
        }
        rt_kprintf("lt9611: chip id %06x\n", id);
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

    /* The PCR recovers the pixel clock from the MIPI input, which starts
     * with the DSI video after init: set up in post_enable. */

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

    /* HDMI output goes on in post_enable, once the input is there. */
    return K_SUCCESS;

init_failed:
    g_panel_init_status = K_FALSE;
    LOG_W("lt9611 init failed, but we treat it as non-fatal\n");
    return K_SUCCESS;
}

/*
 * After the DSI video started: set up the PCR on the running MIPI clock, as
 * the Linux lontium-lt9611 driver does, enable HDMI output, then wait for the
 * chip to measure the input and compare it with the mode. A mismatch is
 * logged, not fatal: "chip present, no or wrong video in" rather than a
 * silent black screen.
 */
static int lt9611_panel_post_enable(const struct panel_desc* desc)
{
    struct lt9611_dev* dev = g_lt9611_dev;
    const k_vo_timing* t   = &desc->timing;
    k_u32 vtotal = t->vactive + t->vsync_len + t->vback_porch + t->vfront_porch;
    k_u32 vactive = 0, vtot = 0;
    k_s32 ret;

    if (dev == NULL || !g_panel_init_status)
        return K_SUCCESS;   /* init failed, already reported */

    dev->i2c_info.write_status = RT_EOK;
    ret = lt9611_setup_pcr(dev);
    if (ret)
        goto failed;

    /* Output on first: its PLL and PCR resets let the slow modes (33 MHz
     * and below) settle; measured before it, they read 0 lines. */
    ret = lt9611_enable_hdmi_out(dev);
    if (ret)
        goto failed;

    /* INIT holds g_connector_dev.lock: keep PCR startup and input checking
     * serialized with power-off and mode changes. These waits add up to
     * 500 ms, plus I2C time; a matching measurement ends the loop early. */
    for (int i = 0; i < 10; i++) {
        rt_thread_mdelay(50);
        if (lt9611_read_video_check(dev, 0x82, &vactive) != RT_EOK ||
            lt9611_read_video_check(dev, 0x6c, &vtot) != RT_EOK) {
            ret = RT_ERROR;
            goto failed;
        }
        if (vactive == t->vactive && vtot == vtotal)
            break;
    }
    if (vactive == t->vactive && vtot == vtotal)
        rt_kprintf("lt9611: MIPI input %u/%u lines\n", vactive, vtot);
    else
        LOG_W("lt9611: MIPI input %u/%u lines, mode %u/%u: no or wrong video in\n",
              vactive, vtot, t->vactive, vtotal);
    return K_SUCCESS;

failed:
    g_panel_init_status = K_FALSE;
    LOG_W("lt9611 enable failed, but we treat it as non-fatal\n");
    return K_SUCCESS;
}

/* HDMI output off, then the chip held in reset until the next init. */
static int lt9611_panel_power_off(const struct panel_desc* desc)
{
    k_s32 ret = RT_EOK;

    g_lt9611_running = K_FALSE;
    if (g_lt9611_dev != NULL) {
        g_lt9611_dev->i2c_info.write_status = RT_EOK;
        lt9611_write_reg(&g_lt9611_dev->i2c_info, 0xff, 0x81);
        lt9611_write_reg(&g_lt9611_dev->i2c_info, 0x30, 0x6a);
        ret = g_lt9611_dev->i2c_info.write_status;
    }
    if (desc->gpio.reset_pin >= 0) {
        kd_pin_mode(desc->gpio.reset_pin, GPIO_DM_OUTPUT);
        kd_pin_write(desc->gpio.reset_pin, desc->gpio.reset_active_low ? GPIO_PV_LOW : GPIO_PV_HIGH);
    }
    return ret == RT_EOK ? 0 : -1;
}

/* The sink's EDID (base block and one extension) through the chip's DDC
 * master, as the Linux lontium-lt9611 driver reads it. */
static k_s32 lt9611_read_edid(struct lt9611_dev* dev, k_u8* edid)
{
    k_i2c_info* i2c = &dev->i2c_info;
    k_u8 st = 0;
    k_s32 ret = RT_EOK;

    i2c->write_status = RT_EOK;
    lt9611_write_reg(i2c, 0xff, 0x85);
    lt9611_write_reg(i2c, 0x03, 0xc9);
    lt9611_write_reg(i2c, 0x04, 0xa0);     /* EDID at DDC address 0xa0 */
    lt9611_write_reg(i2c, 0x05, 0x00);
    lt9611_write_reg(i2c, 0x06, 32);       /* 32 bytes per access */
    lt9611_write_reg(i2c, 0x14, 0x7f);
    for (int i = 0; i < HDMI_EDID_LEN / 32 && ret == RT_EOK; i++) {
        lt9611_write_reg(i2c, 0x05, i * 32);
        lt9611_write_reg(i2c, 0x07, 0x36);
        lt9611_write_reg(i2c, 0x07, 0x31);
        lt9611_write_reg(i2c, 0x07, 0x37);
        /* DDC settling delay per 32-byte transaction: up to 80 ms for the
         * base block and one extension, in addition to the I2C transfers. */
        rt_thread_mdelay(10);
        if (i2c->write_status != RT_EOK || lt9611_read_reg(i2c, 0x40, &st) != RT_EOK || !(st & 0x02)) {
            LOG_W("EDID block %d not read: status 0x%02x%s\n", i, st, (st & 0x50) ? " (no DDC ack)" : "");
            ret = RT_ERROR;
            break;
        }
        for (int j = 0; j < 32; j++) {
            if (lt9611_read_reg(i2c, 0x83, &edid[i * 32 + j]) != RT_EOK) {
                ret = RT_ERROR;
                break;
            }
        }
    }
    lt9611_write_reg(i2c, 0x07, 0x1f);
    return ret;
}

/*
 * What the K230 path carries for LT9611_HDMI_AUTO. The pixel clock must be
 * one VO makes (594 MHz / N, through dsi_correct_pclk()) within 0.5%. The
 * DSI lane rate is derived from it (6 bits per pixel on 4 lanes): the LT9611
 * locks only when it equals the pixel bandwidth, and receives nothing below
 * about 300 Mbps, so at least 54 MHz (324 Mbps, measured). The top is the
 * LT9611's 4K30 (297 MHz). Only modes up to 1080p60 were tried on a board.
 */
#define LT9611_AUTO_MIN_PCLK_KHZ 54000
#define LT9611_AUTO_MAX_PCLK_KHZ 297000

static k_u32 lt9611_auto_clock(k_u32 pclk_khz, void* arg)
{
    k_u32 khz = dsi_correct_pclk(pclk_khz * 1000, K_DSI_4LANE, 0) / 1000;
    k_u32 diff;

    (void)arg;
    if (khz == 0)
        return 0;
    diff = khz > pclk_khz ? khz - pclk_khz : pclk_khz - khz;
    return diff * 200 <= pclk_khz ? khz : 0;
}

/* The last resolved timing: a GET_PANEL_INFO right before INIT (as
 * kd_display_init() does) reuses it, and so does one while a mode is up.
 * Running modes do not renegotiate on HPD changes; see connector/README.md. */
static struct {
    k_bool      valid;
    rt_tick_t   tick;
    k_vo_timing timing;
} g_lt9611_auto;

/*
 * LT9611_HDMI_AUTO: the timing from the sink's EDID (hdmi_edid_choose()),
 * its vertical front porch made VIDEO1-safe; without a sink or a usable
 * EDID, the descriptor's own timing (the K230 custom 1080p60).
 */
static int lt9611_panel_resolve(const struct panel_desc* desc, struct panel_desc* out)
{
    struct hdmi_edid_limits lim = {
        .min_pclk_khz = LT9611_AUTO_MIN_PCLK_KHZ,
        .max_pclk_khz = LT9611_AUTO_MAX_PCLK_KHZ,
        .max_hactive  = 3840,
        .max_vactive  = 2160,
        .clock        = lt9611_auto_clock,
    };
    static k_u8 edid[HDMI_EDID_LEN];
    struct hdmi_edid_info info;
    k_vo_timing t;
    k_u32 id;
    k_u8 hpd = 0;
    k_bool video1_ok;

    *out = *desc;
    if (g_lt9611_running ||
        (g_lt9611_auto.valid && rt_tick_get() - g_lt9611_auto.tick < rt_tick_from_millisecond(2000))) {
        if (g_lt9611_auto.valid)
            out->timing = g_lt9611_auto.timing;
        return 0;
    }

    lt9611_reset(desc->gpio.reset_pin);
    if (g_lt9611_dev == NULL)
        g_lt9611_dev = lt9611_dev_create(LT9611_PORTB, CONFIG_MPP_DSI_LT9611_I2C_SLV_ADDR, CONFIG_MPP_DSI_HDMI_I2C_DEV);
    if (g_lt9611_dev == NULL || lt9611_read_id(g_lt9611_dev, &id) != RT_EOK) {
        LOG_W("lt9611 auto: no chip, using %ux%u\n", desc->timing.hactive, desc->timing.vactive);
        goto done;
    }
    (void)lt9611_init_system(g_lt9611_dev);
    g_lt9611_dev->i2c_info.write_status = RT_EOK;
    if (lt9611_write_reg(&g_lt9611_dev->i2c_info, 0xff, 0x82) != RT_EOK ||
        lt9611_read_reg(&g_lt9611_dev->i2c_info, 0x5e, &hpd) != RT_EOK || !(hpd & 0x05)) {
        LOG_W("lt9611 auto: no HDMI sink, using %ux%u\n", desc->timing.hactive, desc->timing.vactive);
        goto done;
    }
    if (lt9611_read_edid(g_lt9611_dev, edid) != RT_EOK ||
        hdmi_edid_choose(edid, HDMI_EDID_LEN, &lim, &t, &info) != 0) {
        LOG_W("lt9611 auto: no usable EDID mode, using %ux%u\n", desc->timing.hactive, desc->timing.vactive);
        goto done;
    }
    video1_ok = panel_timing_video1_vporch(&t);
    out->timing = t;
    {
        k_u32 ht = t.hactive + t.hsync_len + t.hback_porch + t.hfront_porch;
        k_u32 vt = t.vactive + t.vsync_len + t.vback_porch + t.vfront_porch;

        rt_kprintf("lt9611: EDID %s: %ux%u, %u.%03u MHz, %u Hz, v %u/%u/%u\n", info.name[0] ? info.name : "sink",
                   t.hactive, t.vactive, t.pclk_khz / 1000, t.pclk_khz % 1000,
                   (k_u32)(((k_u64)t.pclk_khz * 1000 + (k_u64)ht * vt / 2) / ((k_u64)ht * vt)),
                   t.vfront_porch, t.vsync_len, t.vback_porch);
        if (!video1_ok)
            LOG_W("lt9611 auto: %u lines after active, hardware VIDEO1 needs %u for a full-height layer\n",
                  t.vfront_porch + t.vsync_len, PANEL_VIDEO1_MIN_TAIL_LINES);
    }
done:
    g_lt9611_auto.valid  = K_TRUE;
    g_lt9611_auto.tick   = rt_tick_get();
    g_lt9611_auto.timing = out->timing;
    return 0;
}

static k_u32 lt9611_panel_read_chip_id(const struct panel_desc* desc)
{
    k_u32 id;

    (void)desc;
    if (g_lt9611_dev == NULL || lt9611_read_id(g_lt9611_dev, &id) != RT_EOK)
        return 0;
    return id;
}

static const struct panel_ops lt9611_ops = {
    .reset        = NULL, // user custom reset
    .init         = lt9611_panel_init,
    .power_off    = lt9611_panel_power_off,
    .read_chip_id = lt9611_panel_read_chip_id,
    .post_enable  = lt9611_panel_post_enable,
};

static const struct panel_ops lt9611_auto_ops = {
    .reset        = NULL,
    .init         = lt9611_panel_init,
    .power_off    = lt9611_panel_power_off,
    .read_chip_id = lt9611_panel_read_chip_id,
    .post_enable  = lt9611_panel_post_enable,
    .resolve      = lt9611_panel_resolve,
};

/* The mode from the sink's EDID; the fixed custom 1080p60 timing below is
 * the fallback without one. */
static const struct panel_desc lt9611_panel_desc_auto = {
    .name = "lt9611_auto",
    .connector_type = LT9611_HDMI_AUTO,
    .bus_type = PANEL_BUS_DSI,
    .timing = {
        .pclk_khz = 148500,
        .hactive = 1920,
        .hsync_len = 44,
        .hback_porch = 148,
        .hfront_porch = 88,
        .vactive = 1080,
        .vsync_len = 5,
        .vback_porch = 12,
        .vfront_porch = 28,
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
    .ops = &lt9611_auto_ops,
};

/* K230 Custom Timings */
static const struct panel_desc lt9611_panel_desc_res_1080p60 = {
    .name = "lt9611_1080p60",
    .connector_type = LT9611_1920_1080_HDMI_V2,
    .bus_type = PANEL_BUS_DSI,
    /* Keep the 2200x1125 totals and sync of 1080p60: the former 2020x1225
     * frame flickers or stays black on VX2880-4K-HDU despite clean WBC.
     * Move 24 back-porch lines to the front, as AUTO does for this mode,
     * so front porch + sync also meets PANEL_VIDEO1_MIN_TAIL_LINES. */
    .timing = {
        .pclk_khz = 148500,
        .hactive = 1920,
        .hsync_len = 44,
        .hback_porch = 148,
        .hfront_porch = 88,
        .vactive = 1080,
        .vsync_len = 5,
        .vback_porch = 12,
        .vfront_porch = 28,
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
    /* 74.25 MHz, so the DSI lanes run at 445.5 Mbps: the former 33 MHz
     * timing put them at 198 Mbps, where the LT9611 receives nothing (see
     * docs/issues/k230-lt9611-driver.md in the SDK). Not the CEA-861 720p30
     * (lt9611_720p30_vesa): its vertical blank is 30 lines, and a
     * full-height VIDEO1 layer needs PANEL_VIDEO1_MIN_TAIL_LINES (33) after
     * the active area. Here 3000 x 825 at 74.25 MHz is 30.0 Hz, with 85. */
    .timing = {
        .pclk_khz = 74250,
        .hactive = 1280,
        .hsync_len = 40,
        .hback_porch = 220,
        .hfront_porch = 1460,
        .vactive = 720,
        .vsync_len = 5,
        .vback_porch = 20,
        .vfront_porch = 80,
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

    /* 54 MHz (the lowest clock with the DSI lanes at 324 Mbps, where the LT9611
     * locks) and 60 Hz: the extra time is horizontal front porch. */
    .timing = {
        .pclk_khz = 54000,
        .hactive = 640,
        .hsync_len = 25,
        .hback_porch = 50,
        .hfront_porch = 967,
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
    .connector_type = LT9611_1920_1080_HDMI_V4,     /* 60 fps, as k_connector_comm.h and CanMV */
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
    .connector_type = LT9611_1920_1080_HDMI_V3,     /* 30 fps */
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

    /* Not the 25.175 MHz DMT timing, which the VO divider cannot make and which
     * puts the DSI lanes below what the LT9611 receives. At 54 MHz, 1716 x 525
     * gives the DMT line and frame rates (31.47 kHz, 59.94 Hz); sync and back
     * porch keep their DMT durations, the rest is front porch. */
    .timing = {
        .pclk_khz = 54000,
        .hactive = 640,
        .hsync_len = 206,
        .hback_porch = 103,
        .hfront_porch = 767,
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
    &lt9611_panel_desc_auto,
    NULL,
};

struct panel_drv hdmi_lt9611_drv = {
    .connector_name = "lt9611",
    .panel_variants = lt9611_panel_variants,
    .active_panel   = &lt9611_panel_desc_res_1080p60,
};
