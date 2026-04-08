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

#include "drv_gpio.h"

#include "connector_bus_dsi.h"
#include "connector_panel.h"
#include "connector_sw_bridge.h"

extern void kd_vo_reset(void);
extern void kd_vo_wrap_init(void);
extern void kd_vo_set_config_mix(void);
extern void kd_vo_set_frame_intr(k_bool status);
extern void kd_vo_set_timing(const k_vo_timing* timing);
extern void kd_vo_set_pixclk(k_u32 div);
extern void kd_vo_set_background(k_u32 rgb888);
extern void kd_vo_enable(void);

static inline k_u32 panel_calc_pixclk_div(k_u32 pclk_khz)
{
    return (pclk_khz == 0) ? 0 : (VO_PIXEL_CLOCK_HZ / (pclk_khz * 1000)) - 1;
}

/**
 * panel_vo_init - Initialize VO (Video Output) resolution
 * @desc: Panel descriptor containing resolution and timing parameters
 *
 * Sets up VO subsystem with panel resolution and timing configuration
 * Reuses connector_comm.c APIs for actual VO configuration
 */
static k_s32 panel_vo_init(const struct panel_desc* desc)
{
    kd_vo_wrap_init();

    kd_vo_set_config_mix();

    kd_vo_set_frame_intr(K_TRUE);

    kd_vo_set_timing(&desc->timing);

    kd_vo_set_background(desc->bg_color);

    kd_vo_enable();

    return 0;
}

k_u32 panel_correct_pclk(k_u32 pclk_hz)
{
    k_u32 ratio;

    if (pclk_hz == 0) {
        return 0;
    }

    // Round to nearest integer divider of the 594 MHz source
    ratio = (VO_PIXEL_CLOCK_HZ + pclk_hz / 2) / pclk_hz;
    if (ratio == 0) {
        ratio = 1;
    }

    return VO_PIXEL_CLOCK_HZ / ratio;
}

static int calculate_panel_fps(k_vo_timing* timing)
{
    if (!timing) {
        return 0;
    }

    k_u32 htotal = timing->hactive + timing->hsync_len + timing->hback_porch + timing->hfront_porch;
    k_u32 vtotal = timing->vactive + timing->vsync_len + timing->vback_porch + timing->vfront_porch;

    return (timing->pclk_khz * 1000) / htotal / vtotal;
}

/**
 * panel_generic_reset - Apply GPIO reset sequence to panel
 * @desc: Panel descriptor containing GPIO configuration
 *
 * Applies the standard reset sequence: HIGH → delay → LOW → delay → HIGH → delay
 * Respects reset_active_low flag to invert logic if needed
 */
int panel_generic_reset(const struct panel_desc* desc)
{
    k_s32  reset_pin;
    k_u32  reset_delay_ms;
    k_bool reset_active_low;
    k_u8   inactive_level, active_level;

    if (!desc) {
        return -1;
    }

    reset_pin = desc->gpio.reset_pin;
    if (reset_pin < 0) {
        return 0; /* Reset not used */
    }

    reset_delay_ms   = desc->gpio.reset_delay_ms;
    reset_active_low = desc->gpio.reset_active_low;

    /* Determine active/inactive levels based on reset polarity */
    if (reset_active_low) {
        active_level   = GPIO_PV_LOW;
        inactive_level = GPIO_PV_HIGH;
    } else {
        active_level   = GPIO_PV_HIGH;
        inactive_level = GPIO_PV_LOW;
    }

    kd_pin_mode(reset_pin, GPIO_DM_OUTPUT);

    /* Standard reset sequence: inactive → active → inactive */
    kd_pin_write(reset_pin, inactive_level);
    rt_thread_mdelay(3);

    kd_pin_write(reset_pin, active_level);
    rt_thread_mdelay(7);

    kd_pin_write(reset_pin, inactive_level);
    rt_thread_mdelay(reset_delay_ms);

    return 0;
}

/**
 * panel_generic_backlight_over_gpio - Control panel backlight GPIO
 * @desc: Panel descriptor containing GPIO configuration
 * @on: 1 to enable backlight, 0 to disable
 *
 * Controls backlight GPIO with optional delay
 * Respects backlight_active_low flag to invert logic if needed
 */
static int panel_generic_backlight_over_gpio(const struct panel_desc* desc, k_u32 mode, k_u32 duty)
{
    k_s32  backlight_pin;
    k_u32  backlight_delay_ms;
    k_bool backlight_active_low;
    k_u8   target_level;
    k_u8   on;

    if (!desc) {
        return -1;
    }

    backlight_pin = desc->gpio.backlight_pin;
    if (backlight_pin < 0) {
        return -1; /* Backlight not used */
    }

    if (0x00 == mode) {
        on = 0;
    } else if (0x01 == mode) {
        on = 1;
    } else {
        on = duty / 128; // convert to 0/1
    }

    backlight_delay_ms   = desc->gpio.backlight_delay_ms;
    backlight_active_low = desc->gpio.backlight_active_low;

    /* Determine target level based on backlight polarity and on/off state */
    if (backlight_active_low) {
        target_level = on ? GPIO_PV_LOW : GPIO_PV_HIGH;
    } else {
        target_level = on ? GPIO_PV_HIGH : GPIO_PV_LOW;
    }

    kd_pin_mode(backlight_pin, GPIO_DM_OUTPUT);
    kd_pin_write(backlight_pin, target_level);

    /* Apply delay if configured */
    if (backlight_delay_ms > 0) {
        rt_thread_mdelay(backlight_delay_ms);
    }

    return 0;
}

// static
int panel_generic_backlight_over_pwm(const struct panel_desc* desc, k_u32 mode, k_u32 duty)
{
    (void)desc;
    (void)mode;
    (void)duty;

    return -1;
}

int panel_generic_backlight(const struct panel_desc* desc, k_u32 mode, k_u32 duty)
{
    if (!desc) {
        return -1;
    }

    return panel_generic_backlight_over_gpio(desc, mode, duty);
}

/**
 * panel_generic_power_on - Generic panel initialization sequence
 * @desc: Panel descriptor containing all configuration
 *
 * Orchestrates full panel init sequence (called from connector_init IOCTL):
 * 1. Set pixel clock divider
 * 2. DSI PHY frequency setup
 * 3. DSI attribute setup + init command sequence + DSI enable
 * 4. VO resolution setup
 *
 * Note: GPIO reset and backlight are handled in connector_power IOCTL,
 * NOT here. This matches the original driver split between power and init.
 */
k_s32 panel_generic_power_on(struct panel_desc* desc)
{
    k_s32 ret = 0;
    k_u32 pixclk_div;
    int   fps;

    if (!desc) {
        return -1;
    }

    kd_vo_reset();

    /* Bus and panel initialization */
    if (!desc->bus_ops) {
        rt_kprintf("panel_generic_power_on: missing bus_ops\n");
        return -1;
    }

    if (desc->ops && desc->ops->reset) {
        ret = desc->ops->reset(desc);
        if (ret != 0) {
            rt_kprintf("panel_generic_power_on: ops->reset failed: %d\n", ret);
            return ret;
        }
    }

    // correct the pclk, user maybe set invalid one.
#if defined(CONFIG_MPP_ENABLE_DSI_LCD) && CONFIG_MPP_ENABLE_DSI_LCD
    if (desc->bus_type == PANEL_BUS_DSI) {
        desc->timing.pclk_khz = dsi_correct_pclk(desc->timing.pclk_khz * 1000, desc->bus.dsi.lanes) / 1000;
    } else
#endif
    {
        desc->timing.pclk_khz = panel_correct_pclk(desc->timing.pclk_khz * 1000) / 1000;
    }

    fps = calculate_panel_fps(&desc->timing);
    rt_kprintf("panel %s, pixelclock %u khz, resolution %dx%d@%d\n", desc->name, desc->timing.pclk_khz, desc->timing.hactive,
               desc->timing.vactive, fps);

    // old code first set pixclk div. why?
    pixclk_div = panel_calc_pixclk_div(desc->timing.pclk_khz);
    if (pixclk_div != 0) {
        kd_vo_set_pixclk(pixclk_div);
    }

    if (desc->bus_ops->init) {
        ret = desc->bus_ops->init(desc);
        if (ret != 0) {
            rt_kprintf("panel_generic_power_on: bus_ops->init failed: %d\n", ret);
            return ret;
        }
    }

    /* Panel-specific initialization (mandatory ops->init) */
    if (!desc->ops || !desc->ops->init) {
        rt_kprintf("panel_generic_power_on: panel missing mandatory ops->init\n");
        return -1;
    }

    ret = desc->ops->init(desc);
    if (ret != 0) {
        rt_kprintf("panel_generic_power_on: ops->init failed: %d\n", ret);
        return ret;
    }

    if (desc->bus_ops->enable) {
        ret = desc->bus_ops->enable(desc);
        if (ret != 0) {
            rt_kprintf("panel_generic_power_on: bus_ops->enable failed: %d\n", ret);
            return ret;
        }
    }

    /* VO resolution initialization (common for both DSI and SPI) */
    ret = panel_vo_init(desc);
    if (ret != 0) {
        rt_kprintf("panel_generic_power_on: panel_vo_init failed: %d\n", ret);
        return ret;
    }

    /* For non-DSI buses, start the software display bridge.
     * DSI panels use hardware video stream and don't need the bridge. */
    if ((desc->bus_type != PANEL_BUS_DSI) && (PANEL_BUS_NONE != desc->bus_type)) {
        ret = sw_bridge_start(desc);
        if (ret != 0) {
            rt_kprintf("panel_generic_power_on: sw_bridge_start failed: %d\n", ret);
            return ret;
        }
    }

    return 0;
}

/**
 * panel_generic_power_off - Generic panel power-off sequence
 * @desc: Panel descriptor containing optional custom_power_off callback
 *
 * Executes panel power-off sequence:
 * - If custom_power_off defined, calls it
 * - Otherwise disables backlight
 */
k_s32 panel_generic_power_off(const struct panel_desc* desc)
{
    k_s32 ret       = 0;
    k_s32 first_err = 0;

    if (!desc) {
        return -1;
    }

    /* Always disable backlight */
    panel_generic_backlight(desc, 0, 0);

    /* Stop software display bridge if running (non-DSI panels) */
    if (sw_bridge_is_running()) {
        sw_bridge_stop();
    }

    /* If panel has custom power-off, use it.
     * Log errors but ALWAYS continue to bus disable — never leave
     * bus hardware in an initialized state. */
    if (desc->ops && desc->ops->power_off) {
        ret = desc->ops->power_off(desc);
        if (ret != 0) {
            rt_kprintf("panel_generic_power_off: ops->power_off failed: %d\n", ret);
            first_err = ret;
        }
    }

    /* Always disable bus hardware regardless of panel power-off result */
    if (desc->bus_ops && desc->bus_ops->disable) {
        ret = desc->bus_ops->disable(desc);
        if (ret != 0) {
            rt_kprintf("panel_generic_power_off: bus_ops->disable failed: %d\n", ret);
            if (!first_err) {
                first_err = ret;
            }
        }
    }

    return first_err;
}
