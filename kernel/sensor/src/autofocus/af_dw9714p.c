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

#include "rtthread.h"
#include "sensor_dev.h"

#include <rtdevice.h>

#define DW9714P_I2C_ADDR (0x0C) // Note: Datasheet for DW9714A specifies 0x18. Using 0x0C as per original driver.

/*
 * DW9714A DLC Mode Configuration Registers (as per datasheet)
 * These commands are sent to the device address (0x0C)
 */
#define PROTECTION_OFF_REG 0xEC
#define PROTECTION_OFF_VAL 0xA3

#define DLC_MCLK_SET_REG 0xA1
// Byte2: 0000 DLC MCLK1 MCLK0 -> DLC=1, MCLK[1:0]=01 (default x1) => 0x0D
#define DLC_MCLK_SET_VAL 0x0D

#define T_SRC_SET_REG 0xF2
// T_SRC[4:0] default value is 5'b=00000. Corresponds to a ~6.33ms step period.
#define T_SRC_SET_VAL 0x00

#define PROTECTION_ON_REG 0xDC
#define PROTECTION_ON_VAL 0x51

#define STEP_SIZE          20
#define MOVEMENT_THRESHOLD (STEP_SIZE * 4)

/* Low-level I2C transfer */
static int af_dw9714p_i2c_transfer(struct rt_i2c_bus_device* bus, struct rt_i2c_msg* msgs, uint32_t msg_cnt)
{
    if (!bus) {
        return -1;
    }
    int ret = rt_i2c_transfer(bus, msgs, msg_cnt);
    return (ret == (int)msg_cnt) ? 0 : -1;
}

/* Helper to write a 2-byte command */
static int af_dw9714p_write_reg(struct rt_i2c_bus_device* bus, uint8_t reg, uint8_t val)
{
    uint8_t buf[2];
    buf[0]                = reg;
    buf[1]                = val;
    struct rt_i2c_msg msg = {
        .addr  = DW9714P_I2C_ADDR,
        .flags = RT_I2C_WR,
        .buf   = buf,
        .len   = 2,
    };

    return af_dw9714p_i2c_transfer(bus, &msg, 1);
}

/* Pack 10-bit code into DW9714P format */
static inline void dw9714p_pack(uint16_t code, uint8_t S, uint8_t pd, uint8_t buf[2])
{
    code &= 0x03FF; // 10 bits
    buf[0] = ((pd & 0x01) << 7) | (0 << 6) | ((code >> 4) & 0x3F); // [PD][FLAG=0][D9..D4]
    buf[1] = ((code & 0x0F) << 4) | (S & 0x0F); // [D3..D0][S3..S0]
}

static int af_dw9714p_set_position_step(void* ctx, uint16_t position)
{
    struct sensor_driver_dev* dev = (struct sensor_driver_dev*)ctx;
    uint8_t                   buf[2];
    uint8_t                   S = 0x03; // S value for DLC mode, adjust if necessary

    dw9714p_pack(position, S, 0, buf);

    struct rt_i2c_msg msg = {
        .addr  = DW9714P_I2C_ADDR,
        .flags = RT_I2C_WR,
        .buf   = buf,
        .len   = 2,
    };

    return af_dw9714p_i2c_transfer(dev->i2c_info.i2c_bus, &msg, 1);
}

/* Get current lens position */
static int af_dw9714p_get_position(void* ctx, k_sensor_focus_pos* pos)
{
    struct sensor_driver_dev* dev = (struct sensor_driver_dev*)ctx;
    if (!pos) {
        return -RT_ERROR;
    }
    k_sensor_af_dev* af = dev->af_dev;

    uintptr_t state = (uintptr_t)af->priv;
    uint16_t  valid = (state >> 16) & 0x1;
    uint16_t  last  = state & 0xFFFF;

    if (!valid) {
        last = 0;
    }

    pos->pos = last;
    return 0;
}

static int af_dw9714p_set_position(void* ctx, k_sensor_focus_pos* pos)
{
    struct sensor_driver_dev* dev = (struct sensor_driver_dev*)ctx;

    if (!dev || !dev->i2c_info.i2c_bus || !pos) {
        return -RT_ERROR;
    }
    k_sensor_af_dev* af = dev->af_dev;

    uint16_t           current_pos;
    k_sensor_focus_pos current_focus_pos;
    af_dw9714p_get_position(ctx, &current_focus_pos);
    current_pos = current_focus_pos.pos;

    uint16_t target = pos->pos & 0x3FF;
    int      ret    = 0;

    int diff = target - current_pos;

    if (abs(diff) > MOVEMENT_THRESHOLD) {
        // Move slowly for large position changes
        int steps = abs(diff) / STEP_SIZE;
        int sign  = (diff > 0) ? 1 : -1;

        for (int i = 0; i < steps; ++i) {
            current_pos += sign * STEP_SIZE;
            ret = af_dw9714p_set_position_step(ctx, current_pos);
            if (ret != 0) {
                goto update_state;
            }
            rt_thread_mdelay(1);
        }
    }

    // Final direct move to the target position
    ret = af_dw9714p_set_position_step(ctx, target);

update_state:
    if (ret == 0) {
        uintptr_t state = ((uintptr_t)1 << 16) | target;
        af->priv        = (void*)state;
    }

    return ret;
}

/* Report capabilities */
static int af_dw9714p_get_capability(void* ctx, k_sensor_autofocus_caps* caps)
{
    (void)ctx;

    if (!caps) {
        return -1;
    }

    caps->isSupport = 1;
    caps->minPos    = 0;
    caps->maxPos    = 1023;

    return 0;
}

static int af_dw9714p_power(void* ctx, int on_off)
{
    struct sensor_driver_dev* dev = (struct sensor_driver_dev*)ctx;

    if (!dev || !dev->i2c_info.i2c_bus) {
        return -RT_ERROR;
    }

    k_sensor_af_dev* af       = dev->af_dev;
    uintptr_t        state    = (uintptr_t)af->priv;
    uint16_t         last_pos = state & 0xFFFF;
    int              ret      = 0;

    if (on_off) {
        // 1. Protection OFF
        ret = af_dw9714p_write_reg(dev->i2c_info.i2c_bus, PROTECTION_OFF_REG, PROTECTION_OFF_VAL);
        if (ret != 0)
            goto power_fail;

        // 2. Enable DLC and set MCLK
        ret = af_dw9714p_write_reg(dev->i2c_info.i2c_bus, DLC_MCLK_SET_REG, DLC_MCLK_SET_VAL);
        if (ret != 0)
            goto power_fail;

        // 3. Set T_SRC for DLC step period
        ret = af_dw9714p_write_reg(dev->i2c_info.i2c_bus, T_SRC_SET_REG, T_SRC_SET_VAL);
        if (ret != 0)
            goto power_fail;

        // 4. Protection ON
        ret = af_dw9714p_write_reg(dev->i2c_info.i2c_bus, PROTECTION_ON_REG, PROTECTION_ON_VAL);
        if (ret != 0) {
            goto power_fail;
        }
    } else {
        /* Power OFF: set PD=1 (power down mode) using direct write */
        uint8_t buf[2];
        dw9714p_pack(0, 0, 1, buf);
        struct rt_i2c_msg msg = {
            .addr  = DW9714P_I2C_ADDR,
            .flags = RT_I2C_WR,
            .buf   = buf,
            .len   = 2,
        };
        ret = af_dw9714p_i2c_transfer(dev->i2c_info.i2c_bus, &msg, 1);
    }

power_fail:
    if (ret == 0) {
        /* Update private state: keep position but set valid flag based on power state */
        uintptr_t new_state = ((uintptr_t)on_off << 16) | last_pos;
        af->priv            = (void*)new_state;
    }

    return ret;
}

/* Probe: power off the DW9714P to ensure a known state */
static int af_dw9714p_probe(void* ctx) { return af_dw9714p_power(ctx, 0); }

/* Driver struct */
const k_sensor_af_dev af_dev_dw9714p = {
    .drv_name       = "DW9714P",
    .probe          = af_dw9714p_probe,
    .set_position   = af_dw9714p_set_position,
    .get_position   = af_dw9714p_get_position,
    .get_capability = af_dw9714p_get_capability,
    .power          = af_dw9714p_power,
    .priv           = NULL,
};
