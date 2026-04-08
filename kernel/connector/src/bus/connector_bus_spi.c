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

#include <drivers/spi.h>
#include <rtdevice.h>
#include <rtthread.h>
#include <stdio.h>

#include "drv_fpioa.h"
#include "drv_gpio.h"

#include "connector_bus.h"
#include "connector_panel.h"
#include "k_autoconf_comm.h"

/* Derive bus name and FPIOA functions from Kconfig bus choice */
#if defined(CONFIG_MPP_SPI_LCD_BUS_SPI0)
#define SPI_LCD_BUS_NAME "spi0"
#define SPI_LCD_CLK_FUNC OSPI_CLK
#define SPI_LCD_D0_FUNC  OSPI_D0
#elif defined(CONFIG_MPP_SPI_LCD_BUS_SPI2)
#define SPI_LCD_BUS_NAME "spi2"
#define SPI_LCD_CLK_FUNC QSPI1_CLK
#define SPI_LCD_D0_FUNC  QSPI1_D0
#else /* default: spi1 */
#define SPI_LCD_BUS_NAME "spi1"
#define SPI_LCD_CLK_FUNC QSPI0_CLK
#define SPI_LCD_D0_FUNC  QSPI0_D0
#endif

/* SPI transfer chunk size (64KB) — K230 single-line DMA max is 0x10000 */
#define SPI_MAX_CHUNK_SIZE (64 * 1024)

/* Runtime context for the active SPI panel */
static struct {
    struct rt_qspi_device* qspi_dev;
    k_s32                  cs_pin;
    k_s32                  dc_pin;
    k_bool                 initialized;
    k_bool                 dev_allocated;
} spi_ctx;

static inline void spi_dc_cmd(void)
{
    if (spi_ctx.dc_pin >= 0)
        kd_pin_write(spi_ctx.dc_pin, GPIO_PV_LOW);
}

static inline void spi_dc_data(void)
{
    if (spi_ctx.dc_pin >= 0)
        kd_pin_write(spi_ctx.dc_pin, GPIO_PV_HIGH);
}

static inline void spi_cs_low(void)
{
    if (spi_ctx.cs_pin >= 0)
        kd_pin_write(spi_ctx.cs_pin, GPIO_PV_LOW);
}

static inline void spi_cs_high(void)
{
    if (spi_ctx.cs_pin >= 0)
        kd_pin_write(spi_ctx.cs_pin, GPIO_PV_HIGH);
}

static int spi_bus_init(const struct panel_desc* desc)
{
    struct rt_qspi_configuration cfg;
    struct rt_qspi_device*       qspi_dev;

    if (!desc)
        return -1;

    if (spi_ctx.initialized) {
        rt_kprintf("spi_bus: already initialized\n");
        return 0;
    }

    if (!desc->bus.spi.spi_dev_name) {
        rt_kprintf("spi_bus: spi_dev_name is NULL\n");
        return -1;
    }

    /* Allocate rt_qspi_device — K230 SPI buses are registered as QSPI */
    qspi_dev = (struct rt_qspi_device*)rt_malloc(sizeof(struct rt_qspi_device));
    if (!qspi_dev) {
        rt_kprintf("spi_bus: alloc qspi_device failed\n");
        return -1;
    }
    rt_memset(qspi_dev, 0, sizeof(struct rt_qspi_device));

    /* Configure FPIOA pin mux for bus signals (derived from Kconfig bus choice) */
    drv_fpioa_set_pin_func(CONFIG_MPP_SPI_LCD_CLK_PIN, SPI_LCD_CLK_FUNC);
    drv_fpioa_set_pin_func(CONFIG_MPP_SPI_LCD_D0_PIN, SPI_LCD_D0_FUNC);

    if (rt_spi_bus_attach_device(&qspi_dev->parent, desc->bus.spi.spi_dev_name, SPI_LCD_BUS_NAME, NULL) != RT_EOK) {
        rt_kprintf("spi_bus: attach '%s' to bus '%s' failed\n", desc->bus.spi.spi_dev_name, SPI_LCD_BUS_NAME);
        rt_free(qspi_dev);
        return -1;
    }

    /* Configure as QSPI with single data line (standard SPI mode) */
    rt_memset(&cfg, 0, sizeof(cfg));
    cfg.parent.max_hz     = desc->bus.spi.spi_speed_hz;
    cfg.parent.data_width = 8;
    cfg.parent.mode       = RT_SPI_MASTER | RT_SPI_MSB;
    cfg.qspi_dl_width     = 1;

    switch (desc->bus.spi.spi_mode) {
    case 0:
        cfg.parent.mode |= RT_SPI_MODE_0;
        break;
    case 1:
        cfg.parent.mode |= RT_SPI_MODE_1;
        break;
    case 2:
        cfg.parent.mode |= RT_SPI_MODE_2;
        break;
    case 3:
        cfg.parent.mode |= RT_SPI_MODE_3;
        break;
    default:
        cfg.parent.mode |= RT_SPI_MODE_0;
        break;
    }

    /* Use K230 SPI driver's built-in soft_cs: bit7 = enable, bits[6:0] = pin */
    cfg.parent.hard_cs = 0;
    cfg.parent.soft_cs = 0;

    if (rt_qspi_configure(qspi_dev, &cfg) != RT_EOK) {
        rt_kprintf("spi_bus: configure failed\n");
        rt_device_unregister(&qspi_dev->parent.parent);
        rt_free(qspi_dev);
        return -1;
    }

    spi_ctx.cs_pin = CONFIG_MPP_SPI_LCD_CS_PIN;
    if (spi_ctx.cs_pin >= 0) {
        kd_pin_mode(spi_ctx.cs_pin, GPIO_DM_OUTPUT);
        kd_pin_write(spi_ctx.cs_pin, GPIO_PV_HIGH);
    }

    /* Configure DC (Data/Command) GPIO pin */
    spi_ctx.dc_pin = CONFIG_MPP_SPI_LCD_DC_PIN;
    if (spi_ctx.dc_pin >= 0) {
        kd_pin_mode(spi_ctx.dc_pin, GPIO_DM_OUTPUT);
        kd_pin_write(spi_ctx.dc_pin, GPIO_PV_HIGH);
    }

    spi_ctx.qspi_dev      = qspi_dev;
    spi_ctx.dev_allocated = K_TRUE;
    spi_ctx.initialized   = K_TRUE;

    rt_kprintf("spi_bus: init ok, bus=%s dev=%s speed=%u Hz\n", SPI_LCD_BUS_NAME, desc->bus.spi.spi_dev_name,
               desc->bus.spi.spi_speed_hz);
    return 0;
}

static int spi_bus_enable(const struct panel_desc* desc)
{
    (void)desc;
    return 0;
}

static int spi_bus_disable(const struct panel_desc* desc)
{
    (void)desc;

    if (spi_ctx.initialized) {
        if (spi_ctx.qspi_dev && spi_ctx.dev_allocated) {
            rt_device_unregister(&spi_ctx.qspi_dev->parent.parent);
            rt_free(spi_ctx.qspi_dev);
        }
        spi_ctx.qspi_dev      = RT_NULL;
        spi_ctx.dc_pin        = -1;
        spi_ctx.dev_allocated = K_FALSE;
        spi_ctx.initialized   = K_FALSE;
    }
    return 0;
}

static int spi_bus_transfer(struct rt_qspi_device* device, const void* tx_data, void* rx_data, size_t len)
{
    struct rt_qspi_message msg;

    rt_memset(&msg, 0, sizeof(msg));

    msg.parent.send_buf = tx_data;
    msg.parent.recv_buf = rx_data;
    msg.parent.length   = len;
    msg.parent.next     = NULL;

    msg.parent.cs_take    = 0;
    msg.parent.cs_release = 0;

    msg.qspi_data_lines = 1;

    return rt_qspi_transfer_message(device, &msg);
}

static int spi_bus_send(struct rt_qspi_device* device, const void* buf, size_t len)
{
    return spi_bus_transfer(device, buf, NULL, len);
}

static int spi_bus_send_cmd(const struct panel_desc* desc, k_u8 cmd, const k_u8* data, k_u32 len)
{
    (void)desc;

    if (!spi_ctx.initialized || !spi_ctx.qspi_dev)
        return -1;

    spi_cs_low();

    /* Send command byte with DC=LOW; driver handles CS via soft_cs */
    spi_dc_cmd();
    spi_bus_send(spi_ctx.qspi_dev, &cmd, 1);

    spi_dc_data();

    /* Send parameter data with DC=HIGH */
    if (data && len > 0) {
        spi_bus_send(spi_ctx.qspi_dev, data, len);
    }

    spi_cs_high();

    return 0;
}

static int spi_panel_set_draw_area(const struct panel_desc* desc, k_u32 x, k_u32 y, k_u32 w, k_u32 h)
{
    k_u16 xs, ys, xe, ye;
    k_u8  col_data[4], row_data[4];

    if (!desc)
        return -1;

    xs = x;
    ys = y;
    xe = x + w - 1;
    ye = y + h - 1;

    /* Column address set: xs_hi, xs_lo, xe_hi, xe_lo */
    col_data[0] = (k_u8)(xs >> 8);
    col_data[1] = (k_u8)(xs & 0xFF);
    col_data[2] = (k_u8)(xe >> 8);
    col_data[3] = (k_u8)(xe & 0xFF);
    spi_bus_send_cmd(desc, 0x2A, col_data, 4);

    /* Row address set: ys_hi, ys_lo, ye_hi, ye_lo */
    row_data[0] = (k_u8)(ys >> 8);
    row_data[1] = (k_u8)(ys & 0xFF);
    row_data[2] = (k_u8)(ye >> 8);
    row_data[3] = (k_u8)(ye & 0xFF);
    spi_bus_send_cmd(desc, 0x2B, row_data, 4);

    /* Memory write command */
    spi_bus_send_cmd(desc, 0x2C, NULL, 0);

    return 0;
}

static int spi_bus_send_frame(const struct panel_desc* desc, void* data, k_u32 size)
{
    const k_u8* p;
    k_u32       remaining, chunk;

    (void)desc;

    if (!spi_ctx.initialized || !spi_ctx.qspi_dev)
        return -1;
    if (!data || size == 0)
        return -1;

    /* Prepare panel for receiving pixel data */
    spi_panel_set_draw_area(desc, 0, 0, desc->timing.hactive, desc->timing.vactive);

    /* DC=HIGH for pixel data; driver handles CS via soft_cs per chunk */
    spi_dc_data();

    spi_cs_low();

    /* Send in chunks to stay within SPI DMA limits */
    p = (const k_u8*)data;

    remaining = size;
    while (remaining > 0) {
        chunk = (remaining > SPI_MAX_CHUNK_SIZE) ? SPI_MAX_CHUNK_SIZE : remaining;
        spi_bus_send(spi_ctx.qspi_dev, p, chunk);
        p += chunk;
        remaining -= chunk;
    }

    spi_cs_high();

    return 0;
}

const struct panel_bus_ops spi_bus_ops = {
    .init       = spi_bus_init,
    .enable     = spi_bus_enable,
    .disable    = spi_bus_disable,
    .send_cmd   = spi_bus_send_cmd,
    .send_frame = spi_bus_send_frame,
};
