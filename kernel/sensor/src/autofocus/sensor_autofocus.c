/* Copyright (c) 2025, Canaan Bright Sight Co., Ltd
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

#include "k_autoconf_comm.h"

///////////////////////////////////////////////////////////////////////////////
// Dummpy af device ///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
static int af_dummpy_probe(void* ctx)
{
    (void)ctx;

    rt_kprintf("Probe dummy af driver\n");

    return 0;
}

static int af_dummpy_set_position(void* ctx, k_sensor_focus_pos* pos)
{
    (void)ctx;
    (void)pos;

    // rt_kprintf("%s->%d SHOULD NOT REACH.\n", __FUNCTION__, __LINE__);

    return -1;
}

static int af_dummpy_get_position(void* ctx, k_sensor_focus_pos* pos)
{
    (void)ctx;
    (void)pos;

    // rt_kprintf("%s->%d SHOULD NOT REACH.\n", __FUNCTION__, __LINE__);

    return -1;
}

static int af_dummpy_get_capability(void* ctx, k_sensor_autofocus_caps* caps)
{
    (void)ctx;

    caps->isSupport = 0;
    caps->minPos    = 0;
    caps->maxPos    = 0;

    return 0;
}

static int af_dummpy_power(void* ctx, int on_off)
{
    (void)ctx;
    (void)on_off;

    return 0;
}

static const k_sensor_af_dev af_dev_dummpy = {
    .drv_name       = NULL,
    .probe          = af_dummpy_probe,
    .set_position   = af_dummpy_set_position,
    .get_position   = af_dummpy_get_position,
    .get_capability = af_dummpy_get_capability,
    .power          = af_dummpy_power,
    .priv           = NULL,
};
///////////////////////////////////////////////////////////////////////////////
// AutoFocus APIs /////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
static const k_sensor_af_dev* af_devs[] = {
#ifdef CONFIG_MPP_ENABLE_SENSOR_AF_DRV_DW9714P
    &af_dev_dw9714p,
#endif // CONFIG_MPP_ENABLE_SENSOR_AF_DRV_DW9714P

    &af_dev_dummpy, // must be last one.
};

k_s32 sensor_autofocus_dev_probe(struct sensor_driver_dev* dev)
{
    k_sensor_af_dev* af_dev = NULL;

    for (int i = 0; i < sizeof(af_devs) / sizeof(af_devs[0]); i++) {
        af_dev = (k_sensor_af_dev*)af_devs[i];

        if (af_dev && af_dev->probe) {
            if (0x00 == af_dev->probe(dev)) {
                dev->af_dev = af_dev;

                if (af_dev->drv_name) {
                    rt_kprintf("find af driver %s\n", af_dev->drv_name);
                }
                return 0;
            }
        }
    }

    dev->af_dev = NULL;

    return -1;
}

k_s32 sensor_autofocus_dev_set_position(void* ctx, k_sensor_focus_pos* pos)
{
    struct sensor_driver_dev* dev = (struct sensor_driver_dev*)ctx;

    if (!dev || !pos) {
        return -1;
    }
    if (!dev->af_dev) {
        return -1;
    }
    if (!dev->af_dev->set_position) {
        return -1;
    }
    return dev->af_dev->set_position(dev, pos);
}

k_s32 sensor_autofocus_dev_get_position(void* ctx, k_sensor_focus_pos* pos)
{
    struct sensor_driver_dev* dev = (struct sensor_driver_dev*)ctx;

    if (!dev || !pos) {
        return -1;
    }
    if (!dev->af_dev) {
        return -1;
    }
    if (!dev->af_dev->get_position) {
        return -1;
    }
    return dev->af_dev->get_position(dev, pos);
}

k_s32 sensor_autofocus_dev_get_capability(void* ctx, k_sensor_autofocus_caps* caps)
{
    struct sensor_driver_dev* dev = (struct sensor_driver_dev*)ctx;

    if (!dev || !caps) {
        return -1;
    }
    if (!dev->af_dev) {
        return -1;
    }
    if (!dev->af_dev->get_capability) {
        return -1;
    }
    return dev->af_dev->get_capability(dev, caps);
}

k_s32 sensor_autofocus_dev_power(void* ctx, int on_off)
{
    struct sensor_driver_dev* dev = (struct sensor_driver_dev*)ctx;

    if (!dev) {
        return -1;
    }
    if (!dev->af_dev) {
        return -1;
    }
    if (!dev->af_dev->power) {
        return -1;
    }
    return dev->af_dev->power(dev, on_off);
}
