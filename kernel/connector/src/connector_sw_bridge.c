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
#include <stdio.h>
#include <string.h>

#include <cache.h>
#include <ioremap.h>

#include "k_type.h"
#include "k_video_comm.h"
#include "k_vo_comm.h"

#include "connector_panel.h"
#include "connector_sw_bridge.h"
#include "pixfmt_convert.h"

/* Access WBC API from VO module */
#include "drv_vo_wbc.h"
#include "rthw.h"
#include "tick.h"

#define DBG_TAG "sw_bridge"
// #define DBG_LVL DBG_LOG
#define DBG_LVL DBG_WARNING
#define DBG_COLOR
#include <rtdbg.h>

#define SW_BRIDGE_THREAD_NAME     "sw_bridge"
#define SW_BRIDGE_THREAD_STACK    (256 * 1024)
#define SW_BRIDGE_THREAD_PRIO     27 // lower than userapps
#define SW_BRIDGE_DUMP_TIMEOUT_MS 100
#define SW_BRIDGE_WBC_BLK_CNT     3

/* Map k_pixel_format from panel config to bridge_pixel_format */
static enum bridge_pixel_format panel_pixfmt_to_bridge(k_u32 panel_pixel_format)
{
    switch (panel_pixel_format) {
    case PIXEL_FORMAT_RGB_565:
        return BRIDGE_PIXFMT_RGB565;
    case PIXEL_FORMAT_RGB_888:
        return BRIDGE_PIXFMT_RGB888;
    default:
        /* Default to RGB565 for SPI panels */
        return BRIDGE_PIXFMT_RGB565;
    }
}

/* Get the panel pixel format field from the bus config union */
static k_u32 get_panel_pixel_format(const struct panel_desc* desc)
{
    switch (desc->bus_type) {
    case PANEL_BUS_SPI:
        return desc->bus.spi.pixel_format;
    case PANEL_BUS_QSPI:
        return desc->bus.qspi.pixel_format;
    case PANEL_BUS_I8080_SPI:
        return desc->bus.i8080_spi.pixel_format;
    default:
        return PIXEL_FORMAT_RGB_565;
    }
}

/* Singleton bridge context */
static struct {
    volatile k_bool          running;
    rt_thread_t              thread;
    k_bool                   thread_created;
    const struct panel_desc* panel;

    wbc_drv_t*        wbc;
    wbc_subscriber_t* subscriber;

    /* Conversion output buffer (CPU-only, no DMA needed) */
    void* conv_virt;
    k_u32 conv_size;

    /* Target format */
    enum bridge_pixel_format target_fmt;
    k_u32                    width;
    k_u32                    height;
    k_bool                   byte_swap; /* SPI needs byte-swapped RGB565 */

    /* Frame pacing */
    k_u32 frame_interval_ms; /* Minimum ms per frame (0 = unlimited) */

    /* Persistent thread synchronization */
    struct rt_semaphore wake_sem;   /* start() releases to wake thread */
    struct rt_semaphore done_sem;   /* thread releases when session cleanup is done */
} g_bridge;

/**
 * panel_bridge_begin_frame - Send standard display window commands
 * @desc: Panel descriptor
 *
 * Sends the standard sequence for setting the display write window and
 * starting memory write:
 *   0x2A: Column Address Set (xs=0, xe=width-1)
 *   0x2B: Page Address Set   (ys=0, ye=height-1)
 *   0x2C: Memory Write
 */
int panel_bridge_begin_frame(const struct panel_desc* desc)
{
    k_u16 xe, ye;
    k_u8  col_data[4], row_data[4];

    if (!desc || !desc->bus_ops || !desc->bus_ops->send_cmd)
        return -1;

    xe = desc->timing.hactive - 1;
    ye = desc->timing.vactive - 1;

    /* Column address set: xs_hi, xs_lo, xe_hi, xe_lo */
    col_data[0] = 0x00;
    col_data[1] = 0x00;
    col_data[2] = (k_u8)(xe >> 8);
    col_data[3] = (k_u8)(xe & 0xFF);
    desc->bus_ops->send_cmd(desc, 0x2A, col_data, 4);

    /* Row address set: ys_hi, ys_lo, ye_hi, ye_lo */
    row_data[0] = 0x00;
    row_data[1] = 0x00;
    row_data[2] = (k_u8)(ye >> 8);
    row_data[3] = (k_u8)(ye & 0xFF);
    desc->bus_ops->send_cmd(desc, 0x2B, row_data, 4);

    /* Memory write command */
    desc->bus_ops->send_cmd(desc, 0x2C, NULL, 0);

    return 0;
}

static void sw_bridge_cleanup_session(void)
{
    /* Unsubscribe before deinit */
    if (g_bridge.subscriber) {
        vo_wbc_unsubscribe(g_bridge.wbc, g_bridge.subscriber);
        g_bridge.subscriber = NULL;
    }

    /* Deinit WBC (ref-counted, only tears down if last user) */
    if (g_bridge.wbc) {
        vo_wbc_deinit(g_bridge.wbc);
    }

    /* Free conversion buffer */
    if (g_bridge.conv_virt) {
        rt_free_align(g_bridge.conv_virt);
        g_bridge.conv_virt = NULL;
    }
}

static void sw_bridge_thread_entry(void* param)
{
    (void)param;

    while (1) {
        /* Sleep until sw_bridge_start() wakes us */
        rt_sem_take(&g_bridge.wake_sem, RT_WAITING_FOREVER);

        const struct panel_desc* panel = g_bridge.panel;

        rt_kprintf("sw_bridge: session started, %ux%u, fmt=%d, interval=%ums\n",
                   g_bridge.width, g_bridge.height, g_bridge.target_fmt,
                   g_bridge.frame_interval_ms);

        while (g_bridge.running) {
            k_s32              ret;
            k_video_frame_info frame;
            uint64_t           frame_start = cpu_ticks_ms();

            /* 1. Dump a composited frame from WBC */
            ret = vo_wbc_dump_frame(g_bridge.wbc, g_bridge.subscriber, &frame, SW_BRIDGE_DUMP_TIMEOUT_MS);
            if (ret != 0) {
                /* Timeout or not ready — just retry */
                continue;
            }

            /* 2. Map WBC frame physical addresses to virtual for CPU conversion */
            k_u64 y_phys  = frame.v_frame.phys_addr[0];
            k_u64 uv_phys = frame.v_frame.phys_addr[1];
            k_u32 y_size  = g_bridge.width * g_bridge.height;
            k_u32 uv_size = y_size / 2;

            void* y_virt  = y_phys + PV_OFFSET;
            void* uv_virt = uv_phys + PV_OFFSET;

            /* Invalidate dcache for DMA-written WBC buffers before CPU read */
            rt_hw_cpu_dcache_invalidate(y_virt, y_size);
            rt_hw_cpu_dcache_invalidate(uv_virt, uv_size);

            /* 3. Convert YUV420SP -> target RGB format */
            #if DBG_LVL == DBG_LOG
            uint64_t conv_start = cpu_ticks_ms();
            #endif

            ret = pixfmt_convert_yuv420sp_to_rgb((const k_u8*)y_virt, (const k_u8*)uv_virt, (k_u8*)g_bridge.conv_virt,
                                                 g_bridge.width, g_bridge.height, g_bridge.target_fmt,
                                                 g_bridge.byte_swap);

            #if DBG_LVL == DBG_LOG
            uint64_t conv_ms = cpu_ticks_ms() - conv_start;
            #endif

            vo_wbc_dump_release(g_bridge.wbc, &frame);

            if (ret != 0) {
                rt_kprintf("sw_bridge: conversion failed\n");
                continue;
            }

            #if DBG_LVL == DBG_LOG
            LOG_D("convert %ux%u took %u ms",
                  g_bridge.width, g_bridge.height,
                  (k_u32)conv_ms);
            #endif

            /* 4. Send begin_frame command to panel */
            if (panel->ops && panel->ops->begin_frame) {
                panel->ops->begin_frame(panel);
            }

            /* 5. Send converted pixel data to panel */
            if (panel->bus_ops && panel->bus_ops->send_frame) {
                panel->bus_ops->send_frame(panel, g_bridge.conv_virt, g_bridge.conv_size);
            }

            /* 6. Frame pacing — sleep for remainder of target interval */
            if (g_bridge.frame_interval_ms > 0) {
                uint64_t elapsed_ms = cpu_ticks_ms() - frame_start;
                if (elapsed_ms < g_bridge.frame_interval_ms) {
                    rt_thread_mdelay(g_bridge.frame_interval_ms - elapsed_ms);
                }
            }
        }

        rt_kprintf("sw_bridge: session stopped\n");

        sw_bridge_cleanup_session();

        /* Signal sw_bridge_stop() that cleanup is complete */
        rt_sem_release(&g_bridge.done_sem);
    }
}

static int sw_bridge_ensure_thread(void)
{
    if (g_bridge.thread_created) {
        return 0;
    }

    rt_sem_init(&g_bridge.wake_sem, "swb_wake", 0, RT_IPC_FLAG_FIFO);
    rt_sem_init(&g_bridge.done_sem, "swb_done", 0, RT_IPC_FLAG_FIFO);

    g_bridge.thread = rt_thread_create(SW_BRIDGE_THREAD_NAME, sw_bridge_thread_entry, NULL,
                                       SW_BRIDGE_THREAD_STACK, SW_BRIDGE_THREAD_PRIO, 10);
    if (!g_bridge.thread) {
        rt_kprintf("sw_bridge: create thread failed\n");
        return -1;
    }

    rt_thread_startup(g_bridge.thread);
    g_bridge.thread_created = K_TRUE;

    return 0;
}

int sw_bridge_start(const struct panel_desc* desc)
{
    k_u32         bpp;
    k_vo_wbc_attr wbc_attr;

    if (!desc) {
        rt_kprintf("sw_bridge: desc is NULL\n");
        return -1;
    }

    if (g_bridge.running) {
        rt_kprintf("sw_bridge: already running\n");
        return -1;
    }

    /* Get the WBC driver from the VO module */
    g_bridge.wbc = drv_vo_get_wbc();
    if (!g_bridge.wbc) {
        rt_kprintf("sw_bridge: WBC not available (VO not initialized?)\n");
        return -1;
    }

    g_bridge.panel      = desc;
    g_bridge.width      = desc->timing.hactive;
    g_bridge.height     = desc->timing.vactive;
    g_bridge.target_fmt = panel_pixfmt_to_bridge(get_panel_pixel_format(desc));
    g_bridge.byte_swap  = (desc->bus_type == PANEL_BUS_SPI) ? K_TRUE : K_FALSE;

    /* Compute target frame interval from panel timing */
    {
        k_u32 htotal = desc->timing.hactive + desc->timing.hsync_len +
                       desc->timing.hback_porch + desc->timing.hfront_porch;
        k_u32 vtotal = desc->timing.vactive + desc->timing.vsync_len +
                       desc->timing.vback_porch + desc->timing.vfront_porch;
        k_u32 fps = 0;
        if (htotal && vtotal && desc->timing.pclk_khz)
            fps = (desc->timing.pclk_khz * 1000) / (htotal * vtotal);
        g_bridge.frame_interval_ms = fps ? (1000 / fps) : 0;
    }

    bpp                = bridge_pixfmt_bpp(g_bridge.target_fmt);
    g_bridge.conv_size = g_bridge.width * g_bridge.height * bpp;

    /* Initialize WBC */
    wbc_attr.blk_cnt = SW_BRIDGE_WBC_BLK_CNT;
    if (vo_wbc_set_attr(g_bridge.wbc, &wbc_attr) != K_SUCCESS) {
        rt_kprintf("sw_bridge: vo_wbc_set_attr failed\n");
        return -1;
    }

    if (vo_wbc_init(g_bridge.wbc) != K_SUCCESS) {
        rt_kprintf("sw_bridge: vo_wbc_init failed\n");
        return -1;
    }

    /* Subscribe to WBC for frame delivery */
    g_bridge.subscriber = vo_wbc_subscribe(g_bridge.wbc);
    if (!g_bridge.subscriber) {
        rt_kprintf("sw_bridge: vo_wbc_subscribe failed\n");
        goto err_wbc;
    }

    /* Allocate conversion output buffer (CPU-only, sent over SPI) */
    g_bridge.conv_virt = rt_malloc_align(g_bridge.conv_size, RT_CPU_CACHE_LINE_SZ);
    if (!g_bridge.conv_virt) {
        rt_kprintf("sw_bridge: alloc conv buf failed (%u bytes)\n", g_bridge.conv_size);
        goto err_sub;
    }

    /* Ensure persistent thread exists */
    if (sw_bridge_ensure_thread() != 0) {
        goto err_buf;
    }

    /* Wake the thread */
    g_bridge.running = K_TRUE;
    rt_sem_release(&g_bridge.wake_sem);

    rt_kprintf("sw_bridge: started, %ux%u, %u bytes/frame\n", g_bridge.width, g_bridge.height, g_bridge.conv_size);
    return 0;

err_buf:
    rt_free_align(g_bridge.conv_virt);
    g_bridge.conv_virt = NULL;
err_sub:
    vo_wbc_unsubscribe(g_bridge.wbc, g_bridge.subscriber);
    g_bridge.subscriber = NULL;
err_wbc:
    vo_wbc_deinit(g_bridge.wbc);
    return -1;
}

int sw_bridge_stop(void)
{
    if (!g_bridge.running)
        return 0;

    /* Signal thread to stop */
    g_bridge.running = K_FALSE;

    /* Wait for thread to finish cleanup */
    rt_sem_take(&g_bridge.done_sem, rt_tick_from_millisecond(SW_BRIDGE_DUMP_TIMEOUT_MS + 500));

    g_bridge.wbc   = NULL;
    g_bridge.panel = NULL;

    rt_kprintf("sw_bridge: stopped\n");
    return 0;
}

k_bool sw_bridge_is_running(void) { return g_bridge.running ? K_TRUE : K_FALSE; }
