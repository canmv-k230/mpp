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

/* Derive bus name and FPIOA functions from Kconfig bus choice */
#if defined(CONFIG_MPP_QSPI_LCD_BUS_SPI0)
#define QSPI_LCD_BUS_NAME  "spi0"
#define QSPI_LCD_CLK_FUNC  OSPI_CLK
#define QSPI_LCD_D0_FUNC   OSPI_D0
#define QSPI_LCD_D1_FUNC   OSPI_D1
#define QSPI_LCD_D2_FUNC   OSPI_D2
#define QSPI_LCD_D3_FUNC   OSPI_D3
#elif defined(CONFIG_MPP_QSPI_LCD_BUS_SPI2)
#define QSPI_LCD_BUS_NAME  "spi2"
#define QSPI_LCD_CLK_FUNC  QSPI1_CLK
#define QSPI_LCD_D0_FUNC   QSPI1_D0
#define QSPI_LCD_D1_FUNC   QSPI1_D1
#define QSPI_LCD_D2_FUNC   QSPI1_D2
#define QSPI_LCD_D3_FUNC   QSPI1_D3
#else /* default: spi1 */
#define QSPI_LCD_BUS_NAME  "spi1"
#define QSPI_LCD_CLK_FUNC  QSPI0_CLK
#define QSPI_LCD_D0_FUNC   QSPI0_D0
#define QSPI_LCD_D1_FUNC   QSPI0_D1
#define QSPI_LCD_D2_FUNC   QSPI0_D2
#define QSPI_LCD_D3_FUNC   QSPI0_D3
#endif

#define QSPI_MAX_CHUNK_SIZE (64 * 1024)

static struct {
    struct rt_qspi_device *qspi_dev;
    k_s32                  dc_pin;
    k_bool                 initialized;
    k_bool                 dev_allocated;
} qspi_ctx;

static int qspi_bus_init(const struct panel_desc *desc)
{
    struct rt_qspi_configuration cfg;
    struct rt_qspi_device *qspi_dev;

    if (!desc)
        return -1;

    if (qspi_ctx.initialized)
        return 0;

    if (!desc->bus.qspi.qspi_dev_name) {
        rt_kprintf("qspi_bus: qspi_dev_name is NULL\n");
        return -1;
    }

    /* Allocate rt_qspi_device — K230 SPI buses are registered as QSPI */
    qspi_dev = (struct rt_qspi_device *)rt_malloc(sizeof(struct rt_qspi_device));
    if (!qspi_dev) {
        rt_kprintf("qspi_bus: alloc qspi_device failed\n");
        return -1;
    }
    rt_memset(qspi_dev, 0, sizeof(struct rt_qspi_device));

    /* Configure FPIOA pin mux for bus signals (derived from Kconfig bus choice) */
    drv_fpioa_set_pin_func(CONFIG_MPP_QSPI_LCD_CLK_PIN, QSPI_LCD_CLK_FUNC);
    drv_fpioa_set_pin_func(CONFIG_MPP_QSPI_LCD_D0_PIN, QSPI_LCD_D0_FUNC);
    drv_fpioa_set_pin_func(CONFIG_MPP_QSPI_LCD_D1_PIN, QSPI_LCD_D1_FUNC);
    drv_fpioa_set_pin_func(CONFIG_MPP_QSPI_LCD_D2_PIN, QSPI_LCD_D2_FUNC);
    drv_fpioa_set_pin_func(CONFIG_MPP_QSPI_LCD_D3_PIN, QSPI_LCD_D3_FUNC);

    if (rt_spi_bus_attach_device(&qspi_dev->parent,
                                  desc->bus.qspi.qspi_dev_name,
                                  QSPI_LCD_BUS_NAME,
                                  NULL) != RT_EOK) {
        rt_kprintf("qspi_bus: attach '%s' to bus '%s' failed\n",
                   desc->bus.qspi.qspi_dev_name, QSPI_LCD_BUS_NAME);
        rt_free(qspi_dev);
        return -1;
    }

    /* Configure as QSPI with single data line (standard SPI mode) */
    rt_memset(&cfg, 0, sizeof(cfg));
    cfg.parent.max_hz     = desc->bus.qspi.qspi_speed_hz;
    cfg.parent.data_width = 8;
    cfg.parent.mode       = RT_SPI_MASTER | RT_SPI_MSB;
    cfg.qspi_dl_width     = 1;

    switch (desc->bus.qspi.qspi_mode) {
    case 0: cfg.parent.mode |= RT_SPI_MODE_0; break;
    case 1: cfg.parent.mode |= RT_SPI_MODE_1; break;
    case 2: cfg.parent.mode |= RT_SPI_MODE_2; break;
    case 3: cfg.parent.mode |= RT_SPI_MODE_3; break;
    default: cfg.parent.mode |= RT_SPI_MODE_0; break;
    }

    /* Use K230 SPI driver's built-in soft_cs: bit7 = enable, bits[6:0] = pin */
    if (CONFIG_MPP_QSPI_LCD_CS_PIN >= 0)
        cfg.parent.soft_cs = 0x80 | (CONFIG_MPP_QSPI_LCD_CS_PIN & 0x7F);

    if (rt_qspi_configure(qspi_dev, &cfg) != RT_EOK) {
        rt_kprintf("qspi_bus: configure failed\n");
        rt_device_unregister(&qspi_dev->parent.parent);
        rt_free(qspi_dev);
        return -1;
    }

    qspi_ctx.dc_pin = CONFIG_MPP_QSPI_LCD_DC_PIN;
    if (qspi_ctx.dc_pin >= 0) {
        kd_pin_mode(qspi_ctx.dc_pin, GPIO_DM_OUTPUT);
        kd_pin_write(qspi_ctx.dc_pin, GPIO_PV_HIGH);
    }

    qspi_ctx.qspi_dev = qspi_dev;
    qspi_ctx.dev_allocated = K_TRUE;
    qspi_ctx.initialized = K_TRUE;

    rt_kprintf("qspi_bus: init ok, bus=%s dev=%s speed=%u Hz\n",
               QSPI_LCD_BUS_NAME, desc->bus.qspi.qspi_dev_name,
               desc->bus.qspi.qspi_speed_hz);
    return 0;
}

static int qspi_bus_disable(const struct panel_desc *desc)
{
    (void)desc;
    if (qspi_ctx.initialized) {
        if (qspi_ctx.qspi_dev && qspi_ctx.dev_allocated) {
            rt_device_unregister(&qspi_ctx.qspi_dev->parent.parent);
            rt_free(qspi_ctx.qspi_dev);
        }
        qspi_ctx.qspi_dev = RT_NULL;
        qspi_ctx.dc_pin = -1;
        qspi_ctx.dev_allocated = K_FALSE;
        qspi_ctx.initialized = K_FALSE;
    }
    return 0;
}

static int qspi_bus_send_cmd(const struct panel_desc *desc, k_u8 cmd, const k_u8 *data, k_u32 len)
{
    (void)desc;
    if (!qspi_ctx.initialized || !qspi_ctx.qspi_dev)
        return -1;

    /* Send command byte with DC=LOW; driver handles CS via soft_cs */
    if (qspi_ctx.dc_pin >= 0)
        kd_pin_write(qspi_ctx.dc_pin, GPIO_PV_LOW);
    rt_spi_send(&qspi_ctx.qspi_dev->parent, &cmd, 1);

    if (data && len > 0) {
        if (qspi_ctx.dc_pin >= 0)
            kd_pin_write(qspi_ctx.dc_pin, GPIO_PV_HIGH);
        rt_spi_send(&qspi_ctx.qspi_dev->parent, data, len);
    }
    return 0;
}

static int qspi_bus_send_frame(const struct panel_desc *desc, const void *data, k_u32 size)
{
    const k_u8 *p;
    k_u32       remaining, chunk;

    (void)desc;
    if (!qspi_ctx.initialized || !qspi_ctx.qspi_dev)
        return -1;
    if (!data || size == 0)
        return -1;

    /* DC=HIGH for pixel data; driver handles CS via soft_cs per chunk */
    if (qspi_ctx.dc_pin >= 0)
        kd_pin_write(qspi_ctx.dc_pin, GPIO_PV_HIGH);

    p = (const k_u8 *)data;
    remaining = size;
    while (remaining > 0) {
        chunk = (remaining > QSPI_MAX_CHUNK_SIZE) ? QSPI_MAX_CHUNK_SIZE : remaining;
        rt_spi_send(&qspi_ctx.qspi_dev->parent, p, chunk);
        p += chunk;
        remaining -= chunk;
    }

    return 0;
}

const struct panel_bus_ops qspi_bus_ops = {
    .init       = qspi_bus_init,
    .enable     = NULL,
    .disable    = qspi_bus_disable,
    .send_cmd   = qspi_bus_send_cmd,
    .send_frame = qspi_bus_send_frame,
};
