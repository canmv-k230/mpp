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

#include "drv_fpioa.h"
#include "drv_gpio.h"

#include "connector_bus.h"
#include "connector_panel.h"
#include "k_autoconf_comm.h"
#include "tick.h"

/* Derive bus name and FPIOA functions from Kconfig bus choice */
#if defined(CONFIG_MPP_QSPI_LCD_BUS_SPI0)
#define QSPI_LCD_BUS_NAME "spi0"
#define QSPI_LCD_CLK_FUNC OSPI_CLK
#define QSPI_LCD_D0_FUNC  OSPI_D0
#define QSPI_LCD_D1_FUNC  OSPI_D1
#define QSPI_LCD_D2_FUNC  OSPI_D2
#define QSPI_LCD_D3_FUNC  OSPI_D3
#elif defined(CONFIG_MPP_QSPI_LCD_BUS_SPI2)
#define QSPI_LCD_BUS_NAME "spi2"
#define QSPI_LCD_CLK_FUNC QSPI1_CLK
#define QSPI_LCD_D0_FUNC  QSPI1_D0
#define QSPI_LCD_D1_FUNC  QSPI1_D1
#define QSPI_LCD_D2_FUNC  QSPI1_D2
#define QSPI_LCD_D3_FUNC  QSPI1_D3
#else /* default: spi1 */
#define QSPI_LCD_BUS_NAME "spi1"
#define QSPI_LCD_CLK_FUNC QSPI0_CLK
#define QSPI_LCD_D0_FUNC  QSPI0_D0
#define QSPI_LCD_D1_FUNC  QSPI0_D1
#define QSPI_LCD_D2_FUNC  QSPI0_D2
#define QSPI_LCD_D3_FUNC  QSPI0_D3
#endif

#define QSPI_MAX_CHUNK_SIZE (64 * 1024)

static struct {
    struct rt_qspi_device* qspi_dev;
    k_s32                  cs_pin;
    k_bool                 initialized;
    k_bool                 dev_allocated;
} qspi_ctx;

static inline void qspi_cs_low(void)
{
    if (qspi_ctx.cs_pin >= 0) {
        kd_pin_write(qspi_ctx.cs_pin, GPIO_PV_LOW);

        cpu_ticks_delay_us(5); /* Short delay to ensure CS setup time */
    }
}

static inline void qspi_cs_high(void)
{
    if (qspi_ctx.cs_pin >= 0) {
        kd_pin_write(qspi_ctx.cs_pin, GPIO_PV_HIGH);

        cpu_ticks_delay_us(5); /* Short delay to ensure CS setup time */
    }
}

static int qspi_bus_init(const struct panel_desc* desc)
{
    struct rt_qspi_configuration cfg;
    struct rt_qspi_device*       qspi_dev;

    if (!desc) {
        return -1;
    }

    if (qspi_ctx.initialized) {
        return 0;
    }

    if (!desc->bus.qspi.qspi_dev_name) {
        rt_kprintf("qspi_bus: qspi_dev_name is NULL\n");
        return -1;
    }

    /* Allocate rt_qspi_device — K230 SPI buses are registered as QSPI */
    qspi_dev = (struct rt_qspi_device*)rt_malloc(sizeof(struct rt_qspi_device));
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

    if (rt_spi_bus_attach_device(&qspi_dev->parent, desc->bus.qspi.qspi_dev_name, QSPI_LCD_BUS_NAME, NULL) != RT_EOK) {
        rt_kprintf("qspi_bus: attach '%s' to bus '%s' failed\n", desc->bus.qspi.qspi_dev_name, QSPI_LCD_BUS_NAME);
        rt_free(qspi_dev);
        return -1;
    }

    /* Configure as QSPI — commands use 1 data line, frames use 4 */
    rt_memset(&cfg, 0, sizeof(cfg));
    cfg.parent.max_hz     = desc->bus.qspi.qspi_speed_hz;
    cfg.parent.data_width = 8;
    cfg.parent.mode       = RT_SPI_MASTER | RT_SPI_MSB;
    cfg.qspi_dl_width     = 4;

    switch (desc->bus.qspi.qspi_mode) {
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

    /* Manual CS control via GPIO (matching SPI bus pattern) */
    cfg.parent.hard_cs = 0;
    cfg.parent.soft_cs = 0;

    if (rt_qspi_configure(qspi_dev, &cfg) != RT_EOK) {
        rt_kprintf("qspi_bus: configure failed\n");
        rt_device_unregister(&qspi_dev->parent.parent);
        rt_free(qspi_dev);
        return -1;
    }

    qspi_ctx.cs_pin = CONFIG_MPP_QSPI_LCD_CS_PIN;
    if (qspi_ctx.cs_pin >= 0) {
        kd_pin_mode(qspi_ctx.cs_pin, GPIO_DM_OUTPUT);
        kd_pin_write(qspi_ctx.cs_pin, GPIO_PV_HIGH);
    }

    qspi_ctx.qspi_dev      = qspi_dev;
    qspi_ctx.dev_allocated = K_TRUE;
    qspi_ctx.initialized   = K_TRUE;

    rt_kprintf("qspi_bus: init ok, bus=%s dev=%s speed=%u Hz\n", QSPI_LCD_BUS_NAME, desc->bus.qspi.qspi_dev_name,
               desc->bus.qspi.qspi_speed_hz);
    return 0;
}

static int qspi_bus_disable(const struct panel_desc* desc)
{
    (void)desc;
    if (qspi_ctx.initialized) {
        if (qspi_ctx.qspi_dev && qspi_ctx.dev_allocated) {
            rt_device_unregister(&qspi_ctx.qspi_dev->parent.parent);
            rt_free(qspi_ctx.qspi_dev);
        }
        qspi_ctx.qspi_dev      = RT_NULL;
        qspi_ctx.cs_pin        = -1;
        qspi_ctx.dev_allocated = K_FALSE;
        qspi_ctx.initialized   = K_FALSE;
    }
    return 0;
}

static int qspi_bus_send_cmd(const struct panel_desc* desc, k_u8 cmd, const k_u8* data, k_u32 len)
{
    struct rt_qspi_message msg;

    (void)desc;
    if (!qspi_ctx.initialized || !qspi_ctx.qspi_dev) {
        return -1;
    }

    rt_memset(&msg, 0, sizeof(msg));

    /* Instruction phase: 0x02 (single-line write) */
    msg.instruction.content    = 0x02;
    msg.instruction.size       = 8;
    msg.instruction.qspi_lines = 1;

    /* Address phase: D/C=0 (command), cmd in bits[15:8] */
    msg.address.content    = (k_u32)cmd << 8;
    msg.address.size       = 24;
    msg.address.qspi_lines = 1;

    /* Dummy cycles - try adding if needed */
    msg.dummy_cycles = 0;

    /* Data phase */
    msg.parent.send_buf   = data;
    msg.parent.recv_buf   = NULL;
    msg.parent.length     = len;
    msg.parent.cs_take    = 0;
    msg.parent.cs_release = 0;
    msg.qspi_data_lines   = 1; /* Command params on 1 line */

    /* Keep CS low for entire transaction */
    qspi_cs_low();
    rt_qspi_transfer_message(qspi_ctx.qspi_dev, &msg);
    qspi_cs_high();

    return 0;
}

static int qspi_panel_set_draw_area(const struct panel_desc* desc, k_u32 x, k_u32 y, k_u32 w, k_u32 h)
{
    k_u16 xs, ys, xe, ye;
    k_u8  col_data[4], row_data[4];

    if (!desc) {
        return -1;
    }

    xs = x;
    ys = y;
    xe = x + w - 1;
    ye = y + h - 1;

    /* Column address set: xs_hi, xs_lo, xe_hi, xe_lo */
    col_data[0] = (k_u8)(xs >> 8);
    col_data[1] = (k_u8)(xs & 0xFF);
    col_data[2] = (k_u8)(xe >> 8);
    col_data[3] = (k_u8)(xe & 0xFF);
    qspi_bus_send_cmd(desc, 0x2A, col_data, 4);

    /* Row address set: ys_hi, ys_lo, ye_hi, ye_lo */
    row_data[0] = (k_u8)(ys >> 8);
    row_data[1] = (k_u8)(ys & 0xFF);
    row_data[2] = (k_u8)(ye >> 8);
    row_data[3] = (k_u8)(ye & 0xFF);
    qspi_bus_send_cmd(desc, 0x2B, row_data, 4);

    /* Memory write command */
    qspi_bus_send_cmd(desc, 0x2C, NULL, 0);

    return 0;
}

static k_u32 qspi_panel_bytes_per_pixel(const struct panel_desc* desc)
{
    if (!desc) {
        return 0;
    }

    switch (desc->bus.qspi.base.pixel_format) {
    case PIXEL_FORMAT_RGB_565:
        return 2;
    case PIXEL_FORMAT_RGB_888:
        return 3;
    default:
        return 0;
    }
}

static int qspi_bus_send_frame(const struct panel_desc* desc, void* data, k_u32 size)
{
    struct rt_qspi_message msg;
    const k_u8*            p;
    k_u32                  remaining, chunk;
    k_u32                  bpp;
    k_u32                  width;
    k_u32                  row_bytes;
    k_u32                  rows_per_chunk;
    k_u32                  chunk_rows;
    k_u32                  total_rows;
    k_u32                  y;

    if (!qspi_ctx.initialized || !qspi_ctx.qspi_dev) {
        return -1;
    }

    if (!desc || !data || size == 0) {
        return -1;
    }

    bpp   = qspi_panel_bytes_per_pixel(desc);
    width = desc->timing.hactive;
    if (bpp == 0 || width == 0) {
        return -1;
    }

    row_bytes = width * bpp;
    if (row_bytes == 0) {
        rt_kprintf("qspi_bus: invalid row_bytes, width=%u bpp=%u\n", width, bpp);
        return -1;
    }

    if (size % row_bytes) {
        rt_kprintf("qspi_bus: partial-row frame is not supported, size=%u row_bytes=%u\n", size, row_bytes);
        return -1;
    }

    total_rows = size / row_bytes;
    if (total_rows == 0 || total_rows > desc->timing.vactive) {
        rt_kprintf("qspi_bus: invalid frame height %u for size=%u\n", total_rows, size);
        return -1;
    }

    rows_per_chunk = QSPI_MAX_CHUNK_SIZE / row_bytes;
    if (rows_per_chunk == 0) {
        rows_per_chunk = 1;
    }

    p         = (const k_u8*)data;
    remaining = size;
    y         = 0;

    while (remaining > 0) {
        chunk_rows = remaining / row_bytes;
        if (chunk_rows > rows_per_chunk)
            chunk_rows = rows_per_chunk;

        chunk = chunk_rows * row_bytes;

        if (qspi_panel_set_draw_area(desc, 0, y, width, chunk_rows) != 0) {
            return -1;
        }

        rt_memset(&msg, 0, sizeof(msg));

        msg.instruction.content    = 0x12;
        msg.instruction.size       = 8;
        msg.instruction.qspi_lines = 1;

        msg.address.content    = 0x002C00;
        msg.address.size       = 24;
        msg.address.qspi_lines = 4;

        msg.dummy_cycles = 0;

        /* Data phase: 4-line QSPI */
        msg.parent.send_buf   = p;
        msg.parent.recv_buf   = NULL;
        msg.parent.length     = chunk;
        msg.parent.cs_take    = 0;
        msg.parent.cs_release = 0;
        msg.qspi_data_lines   = 4;

        qspi_cs_low();
        rt_qspi_transfer_message(qspi_ctx.qspi_dev, &msg);
        qspi_cs_high();

        p += chunk;
        remaining -= chunk;
        y += chunk_rows;
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
