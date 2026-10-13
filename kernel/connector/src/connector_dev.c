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

#include "k_autoconf_comm.h"

#include "rtthread.h"

#include "lwp_user_mm.h"

#include "dfs_poll.h"
#include "dfs_posix.h"

#include "k230_pm.h"

#include "k_connector_comm.h"
#include "k_connector_ioctl.h"

#include "connector_bus_dsi.h"
#include "connector_panel.h"

enum connector_state {
    CONNECTOR_STATE_OFF = 0,
    CONNECTOR_STATE_STARTING,
    CONNECTOR_STATE_ON,
    CONNECTOR_STATE_CLEANUP_REQUIRED,
};

/* Connector state is device-global and independent of control descriptors. */
static struct {
    struct rt_device         device;
    struct rt_mutex          lock;
    struct panel_desc        active_panel;
    const struct panel_desc* selected_panel;
    k_bool                   disp_pm_acquired;
    enum connector_state     state;
} g_connector_dev;

extern struct panel_drv mipi_hx8399_drv;
extern struct panel_drv hdmi_lt9611_drv;
extern struct panel_drv mipi_st7701_drv;
extern struct panel_drv mipi_ili9806_drv;
extern struct panel_drv virtual_display_drv;
extern struct panel_drv mipi_ili9881c_drv;
extern struct panel_drv mipi_nt35516_drv;
extern struct panel_drv mipi_nt35532_drv;
extern struct panel_drv mipi_gc9503_drv;
extern struct panel_drv mipi_st7102_drv;
extern struct panel_drv mipi_aml020t_drv;
extern struct panel_drv mipi_jd9852_drv;

extern struct panel_drv spi_st7789_drv;
extern struct panel_drv i8080_st7789_drv;
extern struct panel_drv qspi_nv3030b_drv;

struct panel_drv* connector_drv_list[] = {
#ifdef CONFIG_MPP_DSI_ENABLE_VIRT
    &virtual_display_drv,
#endif

#ifdef CONFIG_MPP_DSI_ENABLE_HDMI_LT9611
    &hdmi_lt9611_drv,
#endif

#ifdef CONFIG_MPP_DSI_ENABLE_LCD_HX8399
    &mipi_hx8399_drv,
#endif

#ifdef CONFIG_MPP_DSI_ENABLE_LCD_ST7701
    &mipi_st7701_drv,
#endif

#ifdef CONFIG_MPP_DSI_ENABLE_LCD_JD9852
    &mipi_jd9852_drv,
#endif

#ifdef CONFIG_MPP_DSI_ENABLE_LCD_ILI9806
    &mipi_ili9806_drv,
#endif

#ifdef CONFIG_MPP_DSI_ENABLE_LCD_ILI9881
    &mipi_ili9881c_drv,
#endif

#ifdef CONFIG_MPP_DSI_ENABLE_LCD_NT35516
    &mipi_nt35516_drv,
#endif

#ifdef CONFIG_MPP_DSI_ENABLE_LCD_NT35532
    &mipi_nt35532_drv,
#endif

#ifdef CONFIG_MPP_DSI_ENABLE_LCD_GC9503
    &mipi_gc9503_drv,
#endif

#ifdef CONFIG_MPP_DSI_ENABLE_LCD_ST7102
    &mipi_st7102_drv,
#endif

#ifdef CONFIG_MPP_DSI_ENABLE_LCD_AML020T
    &mipi_aml020t_drv,
#endif

#ifdef CONFIG_MPP_SPI_ENABLE_LCD_ST7789
    &spi_st7789_drv,
#endif

#ifdef CONFIG_MPP_OSPI_ENABLE_LCD_ST7789
    &i8080_st7789_drv,
#endif

#ifdef CONFIG_MPP_QSPI_ENABLE_LCD_NV3030B
    &qspi_nv3030b_drv,
#endif

    NULL,
};

static const struct panel_desc* find_panel_by_type(k_connector_type type, const char** out_connector_name)
{
    for (k_u32 drv_idx = 0; connector_drv_list[drv_idx] != NULL; drv_idx++) {
        struct panel_drv* drv = connector_drv_list[drv_idx];

        if (drv->panel_variants) {
            for (k_u32 var_idx = 0; drv->panel_variants[var_idx] != NULL; var_idx++) {
                const struct panel_desc* desc = drv->panel_variants[var_idx];
                if (desc->connector_type == type) {
                    if (out_connector_name) {
                        *out_connector_name = drv->connector_name;
                    }
                    return desc;
                }
            }
        } else if (drv->active_panel && drv->active_panel->connector_type == type) {
            if (out_connector_name) {
                *out_connector_name = drv->connector_name;
            }
            return drv->active_panel;
        }
    }
    return NULL;
}

static k_s32 connector_dev_open(struct dfs_fd* file)
{
    (void)file;
    return 0;
}

static k_s32 connector_release_pm_locked(void)
{
    k_s32 ret;

    if (!g_connector_dev.disp_pm_acquired)
        return 0;

    ret = k230_pm_put(K230_PM_DEV_DISP);
    if (ret == 0)
        g_connector_dev.disp_pm_acquired = K_FALSE;
    return ret;
}

static k_s32 connector_power_off_panel_locked(void)
{
    k_s32 ret;

    if (!g_connector_dev.selected_panel)
        return 0;

    ret = panel_generic_power_off(g_connector_dev.selected_panel);
    /* Power-off completes applicable teardown even when a panel callback
     * fails. The old descriptor is therefore not safe to retry. */
    g_connector_dev.selected_panel = NULL;
    if (ret != 0)
        g_connector_dev.state = CONNECTOR_STATE_CLEANUP_REQUIRED;

    return ret;
}

static k_s32 connector_shutdown_locked(void)
{
    k_s32 panel_ret;
    k_s32 pm_ret;

    panel_ret = connector_power_off_panel_locked();
    pm_ret = connector_release_pm_locked();

    if (!g_connector_dev.selected_panel && !g_connector_dev.disp_pm_acquired)
        g_connector_dev.state = CONNECTOR_STATE_OFF;
    else
        g_connector_dev.state = CONNECTOR_STATE_CLEANUP_REQUIRED;

    return panel_ret != 0 ? panel_ret : pm_ret;
}

static k_s32 connector_dev_close(struct dfs_fd* file)
{
    (void)file;

    /*
     * Connector configuration is device-global and historically survives
     * closing the control descriptor. Existing applications initialize the
     * panel and immediately close this fd while VO keeps scanning out.
     * Power-down is therefore explicit through SET_PANEL_POWER_OFF.
     */
    return 0;
}

static k_s32 connector_dev_ioctl(struct dfs_fd* file, k_s32 cmd, void* args)
{
    (void)file;

    k_s32 ret = 0;

    switch (cmd) {
    case KD_IOC_CONNECTOR_GET_PANEL_INFO: {
        k_connector_info         info;
        k_connector_type         query_type;
        const struct panel_desc* found_panel          = NULL;
        const char*              found_connector_name = NULL;

        if (sizeof(info) != lwp_get_from_user(&info, args, sizeof(info))) {
            rt_kprintf("%s:%d lwp_get_from_user err\n", __func__, __LINE__);
            return -1;
        }

        query_type = info.type;

        found_panel = find_panel_by_type(query_type, &found_connector_name);
        if (!found_panel) {
            rt_kprintf("%s: connector_type %d not found\n", __func__, query_type);
            return -1;
        }

        rt_strncpy(info.connector_name, found_connector_name, sizeof(info.connector_name) - 1);
        info.connector_name[sizeof(info.connector_name) - 1] = '\0';
        info.type                                            = query_type;
        rt_memcpy(&info.timing, &found_panel->timing, sizeof(k_vo_timing));

        info.bg_color = found_panel->bg_color;

        if (sizeof(info) != lwp_put_to_user(args, &info, sizeof(info))) {
            rt_kprintf("%s:%d lwp_put_to_user err\n", __func__, __LINE__);
            return -1;
        }

        ret = 0;
        break;
    }

    case KD_IOC_CONNECTOR_SET_PANEL_INIT: {
        k_connector_init_params  params;
        const struct panel_desc* panel = NULL;

        if (sizeof(params) != lwp_get_from_user(&params, args, sizeof(params))) {
            rt_kprintf("%s:%d lwp_get_from_user err\n", __func__, __LINE__);
            return -1;
        }

        panel = find_panel_by_type(params.connector_type, NULL);
        if (!panel) {
            rt_kprintf("%s: panel type %d not found\n", __func__, params.connector_type);
            return -1;
        }

#if defined(CONFIG_MPP_DSI_ENABLE_VIRT) && CONFIG_MPP_DSI_ENABLE_VIRT
        extern struct panel_desc virtdev_runtime_desc;
        extern k_s32 virtdev_calculate_timings(k_u32 hdisplay, k_u32 vdisplay, k_u32 fps, struct panel_desc * runtime_desc);

        if (params.connector_type == VIRTUAL_DISPLAY_DEVICE && params.virtual_hdisplay != 0) {
            ret = virtdev_calculate_timings(params.virtual_hdisplay, params.virtual_vdisplay, params.virtual_fps,
                                            &virtdev_runtime_desc);
            if (ret != 0) {
                rt_kprintf("%s: virtdev_calculate_timings failed: %d\n", __func__, ret);
                return ret;
            }

            panel = &virtdev_runtime_desc;
        }
#else
        if (params.connector_type == VIRTUAL_DISPLAY_DEVICE) {
            rt_kprintf("%s: virtual display requires valid resolution and fps parameters\n", __func__);
            return -1;
        }
#endif

        rt_mutex_take(&g_connector_dev.lock, RT_WAITING_FOREVER);
        if (!g_connector_dev.disp_pm_acquired) {
            ret = k230_pm_get(K230_PM_DEV_DISP);
            if (ret != 0) {
                rt_kprintf("%s: DISP runtime PM get failed: %d\n",
                           __func__, ret);
                rt_mutex_release(&g_connector_dev.lock);
                return ret;
            }
            g_connector_dev.disp_pm_acquired = K_TRUE;
        }

        /* Re-init replaces the current device-global configuration. Keep the
         * display PM reference while switching panels. */
        if (g_connector_dev.selected_panel) {
            rt_kprintf("connector: auto power-off stale session before re-init\n");
            ret = connector_power_off_panel_locked();
            if (ret != 0) {
                rt_mutex_release(&g_connector_dev.lock);
                return ret;
            }
        }

        rt_memcpy(&g_connector_dev.active_panel, panel, sizeof(g_connector_dev.active_panel));
        g_connector_dev.selected_panel = &g_connector_dev.active_panel;
        g_connector_dev.state = CONNECTOR_STATE_STARTING;

        ret = panel_generic_power_on(&g_connector_dev.active_panel);
        if (ret != 0) {
            k_s32 cleanup_ret;

            rt_kprintf("%s: panel_generic_power_on failed: %d\n", __func__, ret);
            cleanup_ret = connector_shutdown_locked();
            if (cleanup_ret != 0) {
                rt_kprintf("%s: partial panel cleanup failed: %d\n",
                           __func__, cleanup_ret);
            }
            rt_mutex_release(&g_connector_dev.lock);
            return ret;
        }

        g_connector_dev.state = CONNECTOR_STATE_ON;
        rt_mutex_release(&g_connector_dev.lock);
        break;
    }

    case KD_IOC_CONNECTOR_SET_PANEL_POWER_OFF: {
        k_s32 power_on;

        if (sizeof(power_on) != lwp_get_from_user(&power_on, args, sizeof(power_on))) {
            rt_kprintf("%s:%d lwp_get_from_user err\n", __func__, __LINE__);
            return -1;
        }

        rt_mutex_take(&g_connector_dev.lock, RT_WAITING_FOREVER);
        if (power_on) {
            /* Legacy ABI: true enables the backlight after INIT. */
            if (g_connector_dev.state != CONNECTOR_STATE_ON ||
                !g_connector_dev.selected_panel) {
                ret = -ENODEV;
            } else {
                /* Preserve the legacy success result for panels without a
                 * controllable backlight. The dedicated backlight ioctl
                 * reports hardware errors to new callers. */
                (void)panel_generic_backlight(g_connector_dev.selected_panel, 1, 0);
                ret = 0;
            }
        } else {
            /* Explicit shutdown is idempotent and retries a failed PM put. */
            ret = connector_shutdown_locked();
        }
        rt_mutex_release(&g_connector_dev.lock);
        break;
    }

    case KD_IOC_CONNECTOR_GET_PANEL_ID: {
        k_u32 id = 0;

        rt_mutex_take(&g_connector_dev.lock, RT_WAITING_FOREVER);
        if (g_connector_dev.state != CONNECTOR_STATE_ON ||
            !g_connector_dev.selected_panel) {
            rt_kprintf("%s: no panel selected (call INIT first)\n", __func__);
            rt_mutex_release(&g_connector_dev.lock);
            return -1;
        }

        if (g_connector_dev.selected_panel->ops->read_chip_id) {
            id = g_connector_dev.selected_panel->ops->read_chip_id(g_connector_dev.selected_panel);
        }

        if (0x00 != lwp_put_to_user_ex(args, &id, sizeof(id))) {
            rt_kprintf("%s:%d lwp_put_to_user err\n", __func__, __LINE__);
            rt_mutex_release(&g_connector_dev.lock);
            return -EFAULT;
        }

        rt_mutex_release(&g_connector_dev.lock);
        ret = 0;
        break;
    }

    case KD_IOC_CONNECTOR_SET_PANEL_BACKLIGHT: {
        k_connector_backlight_attr attr;

        if (sizeof(attr) != lwp_get_from_user(&attr, args, sizeof(attr))) {
            rt_kprintf("%s:%d lwp_get_from_user err\n", __func__, __LINE__);
            return -EFAULT;
        }

        rt_mutex_take(&g_connector_dev.lock, RT_WAITING_FOREVER);
        if (g_connector_dev.state != CONNECTOR_STATE_ON ||
            !g_connector_dev.selected_panel) {
            rt_kprintf("%s: panel not initialized\n", __func__);
            rt_mutex_release(&g_connector_dev.lock);
            return -ENODEV;
        }

        ret = panel_generic_backlight(g_connector_dev.selected_panel, attr.mode, attr.duty);
        if (ret != 0) {
            rt_kprintf("%s: panel_generic_backlight failed: %d\n", __func__, ret);
        }
        rt_mutex_release(&g_connector_dev.lock);
        break;
    }

    default:
        rt_kprintf("%s: unknown cmd 0x%x\n", __func__, cmd);
        ret = -EINVAL;
        break;
    }

    return ret;
}

static const struct dfs_file_ops connector_dev_fops = {
    .open  = connector_dev_open,
    .close = connector_dev_close,
    .ioctl = connector_dev_ioctl,
};

k_s32 connector_device_init(void)
{
    k_s32 ret;

    rt_memset(&g_connector_dev, 0, sizeof(g_connector_dev));

    ret = rt_mutex_init(&g_connector_dev.lock, "conn_lk", RT_IPC_FLAG_PRIO);
    if (ret != RT_EOK)
        return ret;

    ret = rt_device_register(&g_connector_dev.device, "connector", RT_DEVICE_FLAG_RDWR);
    if (ret) {
        rt_kprintf("connector: failed to register device\n");
        rt_mutex_detach(&g_connector_dev.lock);
        return ret;
    }

    g_connector_dev.device.fops = &connector_dev_fops;

    return 0;
}

static void list_connector(int argc, char** argv)
{
    int shown = 0;

    (void)argc;
    (void)argv;

    rt_kprintf("Connector Type List:\n");
    rt_kprintf("%12s  %-20s  %-12s  %-7s\n", "TYPE", "CONNECTOR", "RESOLUTION", "FPS");
    rt_kprintf("%12s  %-20s  %-12s  %-7s\n", "------------", "--------------------", "------------", "-------");

    for (k_u32 drv_idx = 0; connector_drv_list[drv_idx] != NULL; drv_idx++) {
        struct panel_drv* drv = connector_drv_list[drv_idx];

        if (drv->panel_variants) {
            for (k_u32 var_idx = 0; drv->panel_variants[var_idx] != NULL; var_idx++) {
                const struct panel_desc* panel = drv->panel_variants[var_idx];
                int                      fps   = panel_calculate_fps(&panel->timing);

                rt_kprintf("%12d  %-20s  %4ux%-4u      %3d fps\n", panel->connector_type, panel->name,
                           panel->timing.hactive, panel->timing.vactive, fps);
                shown++;
            }
        } else if (drv->active_panel) {
            const struct panel_desc* panel = drv->active_panel;
            int                      fps   = panel_calculate_fps(&panel->timing);

            rt_kprintf("%12d  %-20s  %4ux%-4u      %3d fps\n", panel->connector_type, drv->connector_name,
                       panel->timing.hactive, panel->timing.vactive, fps);
            shown++;
        }
    }

    if (shown == 0) {
        rt_kprintf("No connector panels registered.\n");
    }

    return;
}

MSH_CMD_EXPORT(list_connector, list connector type)
