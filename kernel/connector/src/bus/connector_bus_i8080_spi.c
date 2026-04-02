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
#include <rtdevice.h>
#include <drivers/spi.h>

#include "drv_gpio.h"
#include "drv_fpioa.h"

#include "connector_bus.h"
#include "connector_panel.h"
#include "k_autoconf_comm.h"

/* OSPI LCD is always on spi0 (OPI controller) */
#define I8080_LCD_BUS_NAME "spi0"

#define I8080_MAX_CHUNK_SIZE (64 * 1024)

/*
 * I8080-over-SPI bus:
 * Uses SPI for data transport with a DC pin to distinguish
 * command (DC=LOW) vs data (DC=HIGH), same as standard SPI panel.
 * The WR/RD pins are optional for bit-bang I8080 timing; when
 * tunneled over SPI they are typically unused.
 */
static struct {
    struct rt_qspi_device *qspi_dev;
    k_s32                  dc_pin;
    k_s32                  wr_pin;
    k_s32                  rd_pin;
    k_bool                 initialized;
    k_bool                 dev_allocated;
} i8080_ctx;

static int i8080_spi_bus_init(const struct panel_desc *desc)
{
    struct rt_qspi_configuration cfg;
    struct rt_qspi_device *qspi_dev;

    if (!desc)
        return -1;

    if (i8080_ctx.initialized)
        return 0;

    if (!desc->bus.i8080_spi.spi_dev_name) {
        rt_kprintf("i8080_spi_bus: spi_dev_name is NULL\n");
        return -1;
    }

    /* Allocate rt_qspi_device — K230 SPI buses are registered as QSPI */
    qspi_dev = (struct rt_qspi_device *)rt_malloc(sizeof(struct rt_qspi_device));
    if (!qspi_dev) {
        rt_kprintf("i8080_spi_bus: alloc qspi_device failed\n");
        return -1;
    }
    rt_memset(qspi_dev, 0, sizeof(struct rt_qspi_device));

    /* OSPI FPIOA pin mux is hardware-fixed, no configuration needed */

    if (rt_spi_bus_attach_device(&qspi_dev->parent,
                                  desc->bus.i8080_spi.spi_dev_name,
                                  I8080_LCD_BUS_NAME,
                                  NULL) != RT_EOK) {
        rt_kprintf("i8080_spi_bus: attach '%s' to bus '%s' failed\n",
                   desc->bus.i8080_spi.spi_dev_name, I8080_LCD_BUS_NAME);
        rt_free(qspi_dev);
        return -1;
    }

    /* Configure as QSPI with single data line (standard SPI mode) */
    rt_memset(&cfg, 0, sizeof(cfg));
    cfg.parent.max_hz     = desc->bus.i8080_spi.spi_speed_hz;
    cfg.parent.data_width = 8;
    cfg.parent.mode       = RT_SPI_MASTER | RT_SPI_MSB;
    cfg.qspi_dl_width     = 1;

    switch (desc->bus.i8080_spi.spi_mode) {
    case 0: cfg.parent.mode |= RT_SPI_MODE_0; break;
    case 1: cfg.parent.mode |= RT_SPI_MODE_1; break;
    case 2: cfg.parent.mode |= RT_SPI_MODE_2; break;
    case 3: cfg.parent.mode |= RT_SPI_MODE_3; break;
    default: cfg.parent.mode |= RT_SPI_MODE_0; break;
    }

    /* Use K230 SPI driver's built-in soft_cs: bit7 = enable, bits[6:0] = pin */
    if (CONFIG_MPP_OSPI_LCD_CS_PIN >= 0)
        cfg.parent.soft_cs = 0x80 | (CONFIG_MPP_OSPI_LCD_CS_PIN & 0x7F);

    if (rt_qspi_configure(qspi_dev, &cfg) != RT_EOK) {
        rt_kprintf("i8080_spi_bus: configure failed\n");
        rt_device_unregister(&qspi_dev->parent.parent);
        rt_free(qspi_dev);
        return -1;
    }

    /* DC pin */
    i8080_ctx.dc_pin = CONFIG_MPP_OSPI_LCD_DC_PIN;
    if (i8080_ctx.dc_pin >= 0) {
        kd_pin_mode(i8080_ctx.dc_pin, GPIO_DM_OUTPUT);
        kd_pin_write(i8080_ctx.dc_pin, GPIO_PV_HIGH);
    }

    /* WR/RD pins not used in OSPI mode */
    i8080_ctx.wr_pin = -1;
    i8080_ctx.rd_pin = -1;

    i8080_ctx.qspi_dev = qspi_dev;
    i8080_ctx.dev_allocated = K_TRUE;
    i8080_ctx.initialized = K_TRUE;

    rt_kprintf("i8080_spi_bus: init ok, bus=%s dev=%s speed=%u Hz\n",
               I8080_LCD_BUS_NAME, desc->bus.i8080_spi.spi_dev_name,
               desc->bus.i8080_spi.spi_speed_hz);
    return 0;
}

static int i8080_spi_bus_disable(const struct panel_desc *desc)
{
    (void)desc;
    if (i8080_ctx.initialized) {
        if (i8080_ctx.qspi_dev && i8080_ctx.dev_allocated) {
            rt_device_unregister(&i8080_ctx.qspi_dev->parent.parent);
            rt_free(i8080_ctx.qspi_dev);
        }
        i8080_ctx.qspi_dev = RT_NULL;
        i8080_ctx.dc_pin = -1;
        i8080_ctx.wr_pin = -1;
        i8080_ctx.rd_pin = -1;
        i8080_ctx.dev_allocated = K_FALSE;
        i8080_ctx.initialized = K_FALSE;
    }
    return 0;
}

static int i8080_spi_bus_send_cmd(const struct panel_desc *desc, k_u8 cmd, const k_u8 *data, k_u32 len)
{
    (void)desc;
    if (!i8080_ctx.initialized || !i8080_ctx.qspi_dev)
        return -1;

    /* Send command byte with DC=LOW; driver handles CS via soft_cs */
    if (i8080_ctx.dc_pin >= 0)
        kd_pin_write(i8080_ctx.dc_pin, GPIO_PV_LOW);
    rt_spi_send(&i8080_ctx.qspi_dev->parent, &cmd, 1);

    if (data && len > 0) {
        if (i8080_ctx.dc_pin >= 0)
            kd_pin_write(i8080_ctx.dc_pin, GPIO_PV_HIGH);
        rt_spi_send(&i8080_ctx.qspi_dev->parent, data, len);
    }
    return 0;
}

static int i8080_spi_bus_send_frame(const struct panel_desc *desc, const void *data, k_u32 size)
{
    const k_u8 *p;
    k_u32       remaining, chunk;

    (void)desc;
    if (!i8080_ctx.initialized || !i8080_ctx.qspi_dev)
        return -1;
    if (!data || size == 0)
        return -1;

    /* DC=HIGH for pixel data; driver handles CS via soft_cs per chunk */
    if (i8080_ctx.dc_pin >= 0)
        kd_pin_write(i8080_ctx.dc_pin, GPIO_PV_HIGH);

    p = (const k_u8 *)data;
    remaining = size;
    while (remaining > 0) {
        chunk = (remaining > I8080_MAX_CHUNK_SIZE) ? I8080_MAX_CHUNK_SIZE : remaining;
        rt_spi_send(&i8080_ctx.qspi_dev->parent, p, chunk);
        p += chunk;
        remaining -= chunk;
    }

    return 0;
}

const struct panel_bus_ops i8080_spi_bus_ops = {
    .init       = i8080_spi_bus_init,
    .enable     = NULL,
    .disable    = i8080_spi_bus_disable,
    .send_cmd   = i8080_spi_bus_send_cmd,
    .send_frame = i8080_spi_bus_send_frame,
};
