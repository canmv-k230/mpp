/* Copyright (c) 2023, Canaan Bright Sight Co., Ltd
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

#include "sensor_dev.h"
#include "drv_gpio.h"

#define IRCUT_PULSE_MS_DEFAULT 200

static k_s32 ircut_pin_write(k_s32 gpio, k_u32 level)
{
    if (gpio < 0)
        return -1;
    if (kd_pin_mode(gpio, GPIO_DM_OUTPUT) != 0)
        return -1;
    if (kd_pin_write(gpio, level ? GPIO_PV_HIGH : GPIO_PV_LOW) != 0)
        return -1;
    return 0;
}

static k_s32 ircut_ctrl_set_level(const k_sensor_ircut_caps *caps, k_ircut_mode mode)
{
    gpio_pin_value_t level;

    if (caps->level.gpio < 0)
        return -1;

    if (mode == K_IRCUT_MODE_COLOR)
        level = caps->level.color_level ? GPIO_PV_HIGH : GPIO_PV_LOW;
    else
        level = caps->level.color_level ? GPIO_PV_LOW : GPIO_PV_HIGH;

    if (kd_pin_mode(caps->level.gpio, GPIO_DM_OUTPUT) != 0)
        return -1;
    if (kd_pin_write(caps->level.gpio, level) != 0)
        return -1;

    return 0;
}

static k_s32 ircut_ctrl_set_pulse(const k_sensor_ircut_caps *caps, k_ircut_mode mode)
{
    k_u32 open_level;
    k_u32 close_level;
    k_u32 pulse_ms;

    if (caps->pulse.open_gpio < 0 || caps->pulse.close_gpio < 0 ||
        caps->pulse.open_gpio == caps->pulse.close_gpio)
        return -1;

    open_level = (mode == K_IRCUT_MODE_COLOR) ? 1 : 0;
    close_level = open_level ? 0 : 1;
    pulse_ms = caps->pulse.pulse_ms ? caps->pulse.pulse_ms : IRCUT_PULSE_MS_DEFAULT;

    if (ircut_pin_write(caps->pulse.open_gpio, open_level) != 0)
        return -1;
    if (ircut_pin_write(caps->pulse.close_gpio, close_level) != 0)
        return -1;

    rt_thread_mdelay(pulse_ms);

    if (ircut_pin_write(caps->pulse.open_gpio, caps->pulse.idle_level) != 0)
        return -1;
    if (ircut_pin_write(caps->pulse.close_gpio, caps->pulse.idle_level) != 0)
        return -1;

    return 0;
}

k_s32 ircut_ctrl_set(const char *name, const k_sensor_ircut_caps *caps, k_ircut_mode mode)
{
    (void)name;

    if (!caps)
        return -1;
    if (mode != K_IRCUT_MODE_COLOR && mode != K_IRCUT_MODE_IR)
        return -1;

    if (caps->type == K_IRCUT_CTRL_LEVEL)
        return ircut_ctrl_set_level(caps, mode);
    if (caps->type == K_IRCUT_CTRL_PULSE)
        return ircut_ctrl_set_pulse(caps, mode);
    return -1;
}

k_s32 ircut_ctrl_get(const k_sensor_ircut_caps *caps, k_ircut_mode stored, k_ircut_mode *mode)
{
    if (!caps || !mode)
        return -1;
    if (!caps->isSupport || caps->type == K_IRCUT_CTRL_NONE)
        return -1;

    *mode = stored;
    return 0;
}
