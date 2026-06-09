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

/* OSPI LCD is always on spi0 (OPI controller). */
#define I8080_LCD_BUS_NAME "spi0"

#define I8080_MAX_CHUNK_SIZE     (64 * 1024)
#define I8080_FRAME_PREFIX_BYTES (4)

/*
 * I8080-over-OSPI bus:
 * Uses OSPI data lines D0..D7 plus a DC GPIO to emulate an i8080-style write bus.
 */
static struct {
    struct rt_qspi_device* qspi_dev;
    k_s32                  cs_pin;
    k_s32                  dc_pin;
    k_bool                 initialized;
    k_bool                 dev_allocated;
} i8080_ctx;

static inline void i8080_spi_dc_cmd(void)
{
    if (i8080_ctx.dc_pin >= 0) {
        kd_pin_write(i8080_ctx.dc_pin, GPIO_PV_LOW);
    }
}

static inline void i8080_spi_dc_data(void)
{
    if (i8080_ctx.dc_pin >= 0) {
        kd_pin_write(i8080_ctx.dc_pin, GPIO_PV_HIGH);
    }
}

static inline void i8080_spi_cs_low(void)
{
    if (i8080_ctx.cs_pin >= 0) {
        kd_pin_write(i8080_ctx.cs_pin, GPIO_PV_LOW);
    }
}

static inline void i8080_spi_cs_high(void)
{
    if (i8080_ctx.cs_pin >= 0) {
        kd_pin_write(i8080_ctx.cs_pin, GPIO_PV_HIGH);
    }
}

static int i8080_spi_bus_configure(const struct panel_desc* desc)
{
    struct rt_qspi_configuration cfg;
    struct rt_qspi_device*       qspi_dev = i8080_ctx.qspi_dev;

    if (!qspi_dev) {
        return -1;
    }

    rt_memset(&cfg, 0, sizeof(cfg));
    cfg.parent.max_hz     = desc->bus.i8080_spi.spi_speed_hz;
    cfg.parent.data_width = 8;
    cfg.parent.mode       = RT_SPI_MASTER | RT_SPI_MSB;
    cfg.qspi_dl_width     = 8;

    switch (desc->bus.i8080_spi.spi_mode) {
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

    /* Manual CS control keeps cmd/param/pixel phases in one transaction. */
    cfg.parent.hard_cs = 0;
    cfg.parent.soft_cs = 0;

    if (rt_qspi_configure(qspi_dev, &cfg) != RT_EOK) {
        rt_kprintf("i8080_spi_bus: configure failed\n");
        return -1;
    }

    return 0;
}

static void i8080_spi_prepare_prefix(struct rt_qspi_message* msg, const k_u8* buf, k_u32 prefix_len)
{
    k_u32 address = 0;
    k_u32 i;

    if (prefix_len == 0) {
        return;
    }

    msg->instruction.content    = buf[0];
    msg->instruction.size       = 8;
    msg->instruction.qspi_lines = 8;

    for (i = 1; i < prefix_len; i++) {
        address = (address << 8) | buf[i];
    }

    if (prefix_len > 1) {
        msg->address.content    = address;
        msg->address.size       = (prefix_len - 1) * 8;
        msg->address.qspi_lines = 8;
    }
}

static rt_size_t i8080_spi_prepare_write_message(struct rt_qspi_message* msg, const k_u8* buf, k_u32 len, k_u32 prefix_cap)
{
    k_u32 prefix_len;

    if (!msg || !buf || len == 0) {
        return 0;
    }

    rt_memset(msg, 0, sizeof(*msg));

    prefix_len = len > prefix_cap ? prefix_cap : len;
    i8080_spi_prepare_prefix(msg, buf, prefix_len);

    msg->parent.send_buf   = prefix_len < len ? buf + prefix_len : RT_NULL;
    msg->parent.recv_buf   = RT_NULL;
    msg->parent.length     = len - prefix_len;
    msg->parent.cs_take    = 0;
    msg->parent.cs_release = 0;
    msg->qspi_data_lines   = 8;

    return msg->parent.length ? msg->parent.length : prefix_len;
}

static rt_size_t i8080_spi_xfer_data(const k_u8* buf, k_u32 len, k_u32 prefix_cap)
{
    rt_size_t              expected;
    rt_size_t              ret;
    struct rt_qspi_message msg;

    if (!buf || !i8080_ctx.qspi_dev || len == 0) {
        return 0;
    }

    expected = i8080_spi_prepare_write_message(&msg, buf, len, prefix_cap);
    if (expected == 0) {
        return -1;
    }

    ret = rt_qspi_transfer_message(i8080_ctx.qspi_dev, &msg);

    return ret == expected ? 0 : -1;
}

static int i8080_spi_bus_send_cmd(const struct panel_desc* desc, k_u8 cmd, const k_u8* data, k_u32 len)
{
    int ret;

    if (!desc || !i8080_ctx.initialized || !i8080_ctx.qspi_dev) {
        return -1;
    }

    i8080_spi_cs_low();

    i8080_spi_dc_cmd();
    ret = i8080_spi_xfer_data(&cmd, 1, I8080_FRAME_PREFIX_BYTES);
    i8080_spi_dc_data();

    if ((0x00 == ret) && (0x00 != len)) {
        k_u32 prefix_cap = I8080_FRAME_PREFIX_BYTES;

        if (len == I8080_FRAME_PREFIX_BYTES) {
            prefix_cap = I8080_FRAME_PREFIX_BYTES - 1;
        }
        ret = i8080_spi_xfer_data(data, len, prefix_cap);
    }

    i8080_spi_cs_high();

    return ret;
}

static int i8080_panel_set_draw_area(const struct panel_desc* desc, k_u32 x, k_u32 y, k_u32 w, k_u32 h)
{
    k_u16 xs, ys, xe, ye;
    k_u8  col_data[4], row_data[4];

    if (!desc) {
        return -1;
    }

    xs = x + desc->bus.i8080_spi.base.x_offset;
    ys = y + desc->bus.i8080_spi.base.y_offset;
    xe = xs + w - 1;
    ye = ys + h - 1;

    col_data[0] = (k_u8)(xs >> 8);
    col_data[1] = (k_u8)(xs & 0xFF);
    col_data[2] = (k_u8)(xe >> 8);
    col_data[3] = (k_u8)(xe & 0xFF);
    if (i8080_spi_bus_send_cmd(desc, 0x2A, col_data, 4) != 0) {
        return -1;
    }

    row_data[0] = (k_u8)(ys >> 8);
    row_data[1] = (k_u8)(ys & 0xFF);
    row_data[2] = (k_u8)(ye >> 8);
    row_data[3] = (k_u8)(ye & 0xFF);
    if (i8080_spi_bus_send_cmd(desc, 0x2B, row_data, 4) != 0) {
        return -1;
    }

    if (i8080_spi_bus_send_cmd(desc, 0x2C, NULL, 0) != 0) {
        return -1;
    }

    return 0;
}

static int i8080_spi_bus_init(const struct panel_desc* desc)
{
    struct rt_qspi_device* qspi_dev;

    if (!desc) {
        return -1;
    }

    if (i8080_ctx.initialized) {
        return 0;
    }

    if (!desc->bus.i8080_spi.spi_dev_name) {
        rt_kprintf("i8080_spi_bus: spi_dev_name is NULL\n");
        return -1;
    }

    if (CONFIG_MPP_OSPI_LCD_CS_PIN < 0) {
        rt_kprintf("i8080_spi_bus: manual CS GPIO is required for i8080-over-SPI\n");
        return -1;
    }

    qspi_dev = (struct rt_qspi_device*)rt_malloc(sizeof(struct rt_qspi_device));
    if (!qspi_dev) {
        rt_kprintf("i8080_spi_bus: alloc qspi_device failed\n");
        return -1;
    }
    rt_memset(qspi_dev, 0, sizeof(struct rt_qspi_device));

    drv_fpioa_set_pin_func(15, OSPI_CLK);
    drv_fpioa_set_pin_func(16, OSPI_D0);
    drv_fpioa_set_pin_func(17, OSPI_D1);
    drv_fpioa_set_pin_func(18, OSPI_D2);
    drv_fpioa_set_pin_func(19, OSPI_D3);
    drv_fpioa_set_pin_func(20, OSPI_D4);
    drv_fpioa_set_pin_func(21, OSPI_D5);
    drv_fpioa_set_pin_func(22, OSPI_D6);
    drv_fpioa_set_pin_func(23, OSPI_D7);

    if (rt_spi_bus_attach_device(&qspi_dev->parent, desc->bus.i8080_spi.spi_dev_name, I8080_LCD_BUS_NAME, NULL) != RT_EOK) {
        rt_kprintf("i8080_spi_bus: attach '%s' to bus '%s' failed\n", desc->bus.i8080_spi.spi_dev_name, I8080_LCD_BUS_NAME);
        rt_free(qspi_dev);
        return -1;
    }

    i8080_ctx.qspi_dev = qspi_dev;

    if (i8080_spi_bus_configure(desc) != 0) {
        rt_device_unregister(&qspi_dev->parent.parent);
        rt_free(qspi_dev);
        i8080_ctx.qspi_dev = RT_NULL;
        return -1;
    }

    i8080_ctx.cs_pin = CONFIG_MPP_OSPI_LCD_CS_PIN;
    kd_pin_mode(i8080_ctx.cs_pin, GPIO_DM_OUTPUT);
    kd_pin_write(i8080_ctx.cs_pin, GPIO_PV_HIGH);

    i8080_ctx.dc_pin = CONFIG_MPP_OSPI_LCD_DC_PIN;
    if (i8080_ctx.dc_pin >= 0) {
        kd_pin_mode(i8080_ctx.dc_pin, GPIO_DM_OUTPUT);
        kd_pin_write(i8080_ctx.dc_pin, GPIO_PV_HIGH);
    }

    i8080_ctx.dev_allocated = K_TRUE;
    i8080_ctx.initialized   = K_TRUE;

    rt_kprintf("i8080_spi_bus: init ok, bus=%s dev=%s speed=%u Hz width=%u\n", I8080_LCD_BUS_NAME,
               desc->bus.i8080_spi.spi_dev_name, desc->bus.i8080_spi.spi_speed_hz, desc->bus.i8080_spi.bus_width);

    return 0;
}

static int i8080_spi_bus_disable(const struct panel_desc* desc)
{
    (void)desc;

    if (i8080_ctx.initialized) {
        if (i8080_ctx.qspi_dev && i8080_ctx.dev_allocated) {
            rt_device_unregister(&i8080_ctx.qspi_dev->parent.parent);
            rt_free(i8080_ctx.qspi_dev);
        }
        i8080_ctx.qspi_dev      = RT_NULL;
        i8080_ctx.cs_pin        = -1;
        i8080_ctx.dc_pin        = -1;
        i8080_ctx.dev_allocated = K_FALSE;
        i8080_ctx.initialized   = K_FALSE;
    }

    return 0;
}

static k_u32 i8080_panel_bytes_per_pixel(const struct panel_desc* desc)
{
    if (!desc) {
        return 0;
    }

    switch (desc->bus.i8080_spi.base.pixel_format) {
    case PIXEL_FORMAT_RGB_565:
        return 2;
    case PIXEL_FORMAT_RGB_888:
        return 3;
    default:
        return 0;
    }
}

static k_u32 i8080_spi_align_chunk_rows(k_u32 row_bytes, k_u32 max_rows)
{
    k_u32 align_rows = 1;

    if (row_bytes == 0 || max_rows == 0) {
        return 0;
    }

    while (align_rows < max_rows && ((row_bytes * align_rows) % RT_CPU_CACHE_LINE_SZ)) {
        align_rows++;
    }

    if ((row_bytes * align_rows) % RT_CPU_CACHE_LINE_SZ) {
        return max_rows;
    }

    max_rows -= max_rows % align_rows;
    return max_rows ? max_rows : align_rows;
}

static int i8080_spi_bus_send_frame(const struct panel_desc* desc, void* data, k_u32 size)
{
    struct rt_qspi_message msg;
    const k_u8*            p;
    k_u32                  bpp;
    k_u32                  width;
    k_u32                  row_bytes;
    k_u32                  total_rows;
    k_u32                  max_chunk_rows;
    k_u32                  remaining_rows;
    k_u32                  y;

    if (!desc) {
        return -1;
    }

    if (!i8080_ctx.initialized || !i8080_ctx.qspi_dev) {
        return -1;
    }

    if (!data || size == 0) {
        return -1;
    }

    bpp   = i8080_panel_bytes_per_pixel(desc);
    width = desc->timing.hactive;
    if (bpp == 0 || width == 0) {
        return -1;
    }

    row_bytes = width * bpp;
    if (row_bytes == 0) {
        rt_kprintf("i8080_spi_bus: invalid row_bytes, width=%u bpp=%u\n", width, bpp);
        return -1;
    }

    if (row_bytes > I8080_MAX_CHUNK_SIZE) {
        rt_kprintf("i8080_spi_bus: row is too large, row_bytes=%u max=%u\n", row_bytes, I8080_MAX_CHUNK_SIZE);
        return -1;
    }

    if (size % row_bytes) {
        rt_kprintf("i8080_spi_bus: partial-row frame is not supported, size=%u row_bytes=%u\n", size, row_bytes);
        return -1;
    }

    total_rows = size / row_bytes;
    if (total_rows == 0 || total_rows > desc->timing.vactive) {
        rt_kprintf("i8080_spi_bus: invalid frame height %u for size=%u\n", total_rows, size);
        return -1;
    }

    max_chunk_rows = i8080_spi_align_chunk_rows(row_bytes, I8080_MAX_CHUNK_SIZE / row_bytes);
    if (max_chunk_rows == 0) {
        return -1;
    }

    p              = (const k_u8*)data;
    remaining_rows = total_rows;
    y              = 0;

    if (i8080_panel_set_draw_area(desc, 0, 0, width, total_rows) != 0) {
        return -1;
    }

    i8080_spi_cs_low();
    i8080_spi_dc_data();

    while (remaining_rows > 0) {
        k_u32     chunk_rows  = remaining_rows > max_chunk_rows ? max_chunk_rows : remaining_rows;
        k_u32     chunk_bytes = chunk_rows * row_bytes;
        rt_size_t expected;
        rt_size_t ret;

        expected = i8080_spi_prepare_write_message(&msg, p, chunk_bytes, I8080_FRAME_PREFIX_BYTES);
        if (expected == 0) {
            i8080_spi_cs_high();
            return -1;
        }

        ret = rt_qspi_transfer_message(i8080_ctx.qspi_dev, &msg);

        if (ret != expected) {
            i8080_spi_cs_high();
            rt_kprintf("i8080_spi_bus: frame chunk transfer failed, y=%u rows=%u bytes=%u ret=%u\n", y, chunk_rows, chunk_bytes,
                       (k_u32)ret);
            return -1;
        }

        p += chunk_bytes;
        y += chunk_rows;
        remaining_rows -= chunk_rows;
    }

    i8080_spi_cs_high();

    return 0;
}

const struct panel_bus_ops i8080_spi_bus_ops = {
    .init       = i8080_spi_bus_init,
    .enable     = NULL,
    .disable    = i8080_spi_bus_disable,
    .send_cmd   = i8080_spi_bus_send_cmd,
    .send_frame = i8080_spi_bus_send_frame,
};
