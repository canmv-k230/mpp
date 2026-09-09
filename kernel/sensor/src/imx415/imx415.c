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

#include "rtthread.h"

#if defined (CONFIG_MPP_ENABLE_CSI_DEV_0)

#include "sensor_dev.h"
#include "io.h"
#include "drv_gpio.h"

#include <stdio.h>
#include <math.h>

#define pr_info(...) //rt_kprintf(__VA_ARGS__)
#define pr_debug(...) //rt_kprintf(__VA_ARGS__)
#define pr_warn(...)    //rt_kprintf(__VA_ARGS__)
#define pr_err(...)    rt_kprintf(__VA_ARGS__)

/* Sensor private ************************************************************/
/* Streaming Mode */
#define IMX415_REG_MODE_SELECT 0x3000
#define IMX415_MODE_STANDBY 0x01
#define IMX415_MODE_STREAMING 0x00
#define IMX415_REG_XMSTA 0x3002
#define IMX415_XMSTA_START 0x00
#define IMX415_XMSTA_STOP 0x01

/*
 * Presence check via INCKSEL4 @0x311A — NOT a unique chip-ID register.
 * Reset default is 0xE0; 37.125 MHz INCK programming keeps 0xE0
 * (24 MHz tables would set 0xFC — not used on this board).
 * Strict 0xE0 check only before mode/INCK table is applied (probe).
 *
 * Coexistence with other Sony sensors in this SDK:
 * - Waveshare IMX415-98 uses I2C 7-bit 0x37; IMX335/290/675 use 0x1A,
 *   so they do not share the bus address with this module.
 * - IMX335 identity is read from CHIP_ID @0x3912 (different register);
 *   even on a same-address module, matching 0x311A alone would not make
 *   IMX335's own probe succeed as IMX415.
 * Verified: probing IMX415 first does not false-match the SDK IMX335 path.
 */
#define IMX415_REG_INCKSEL4 0x311A
#define IMX415_INCKSEL4_RESET_DEFAULT 0xE0

/* Exposure: SHR0 24-bit LE @0x3050 */
#define IMX415_REG_SHR0_L 0x3050
#define IMX415_REG_SHR0_M 0x3051
#define IMX415_REG_SHR0_H 0x3052
#define IMX415_VMAX_LINEAR 2250 /* 0x08CA, Rockchip 4K30 */
#define IMX415_SHR0_MIN 8

/*
 * Analog gain: 0.3 dB/step @0x3090.
 * Hardware clamp IMX415_AGAIN_REG_MAX=0xf0 (~72 dB / ~3981x linear).
 * AE software ceiling is ae_info.max_gain=100 (~40 dB, reg≈133) — lower
 * on purpose; see sensor_csi0_mode_list.c.
 */
#define IMX415_REG_AGAIN_L 0x3090
#define IMX415_REG_AGAIN_H 0x3091
#define IMX415_AGAIN_STEP (1.0f/256.0f)
#define IMX415_AGAIN_REG_MAX 0xf0

/* Group hold (XM_HOLD): latch multi-byte SHR0/AGAIN updates atomically */
#define IMX415_REG_HOLD 0x3001

/* Mirror / flip: REVERSE @0x3030 bit0=H, bit1=V */
#define IMX415_REG_REVERSE 0x3030

/* Input clock: on-module 37.125 MHz crystal (measured on board) */
#define IMX415_INCLK_RATE 37125000
#define IMX415_POWER_DELAY_MS CANMV_SENSOR_POWER_RESET_DELAY_MS
#define IMX415_POWER_STABLE_DELAY_MS CANMV_SENSOR_POWER_STABLE_DELAY_MS

#define IMX415_NUM_DATA_LANES 4

/* include sensor register configure */
#include "sensor_reg_table.c"

#if defined (CONFIG_MPP_ENABLE_CSI_DEV_0)
    #include "sensor_csi0_mode_list.c"
#endif // CONFIG_MPP_ENABLE_CSI_DEV_0

static k_s32 _sensor_read_chip_id_r(struct sensor_driver_dev *dev, k_u32 *chip_id)
{
    k_s32 ret = 0;
    k_u16 id = 0;
    const k_s32 reset_gpio = dev->reset_gpio;

    pr_info("%s enter, %s\n", __func__, dev->sensor_name);

    if(NULL == dev->i2c_info.i2c_bus) {
        pr_err("%s no i2c bus\n", dev->sensor_name);
        return -1;
    }

    if(0 <= reset_gpio) {
        kd_pin_mode(reset_gpio, GPIO_DM_OUTPUT);
        kd_pin_write(reset_gpio, GPIO_PV_HIGH);
    }
    rt_thread_mdelay(1);

    /* Presence check via INCKSEL4 @0x311A (see comment at IMX415_REG_INCKSEL4) */
    ret = sensor_reg_read(&dev->i2c_info, IMX415_REG_INCKSEL4, &id);
    if(chip_id) {
        *chip_id = id;
        pr_info("%s incksel4 0x%x (mode_init=%d)\n",
                __func__, *chip_id, dev->mode_init_flag);
    }

    if(ret) {
        return ret;
    }

    /*
     * After mode/INCK table is applied, INCKSEL4 depends on INCK
     * (37.125 MHz keeps 0xE0; 24 MHz would be 0xFC). Probe already
     * validated reset default once — skip strict check after mode init.
     */
    if(dev->mode_init_flag) {
        return 0;
    }

    if((id & 0xff) != IMX415_INCKSEL4_RESET_DEFAULT) {
        pr_err("%s unexpected INCKSEL4 0x%x before mode init (expect reset 0x%x)\n",
               __func__, id, IMX415_INCKSEL4_RESET_DEFAULT);
        return -1;
    }

    return 0;
}

/* Sensor functions **********************************************************/
static int _sensor_power_state_set(struct sensor_driver_dev *dev, k_s32 on, k_u32 delay)
{
    const k_s32 pwd_gpio = dev->pwd_gpio;
    const k_s32 reset_gpio = dev->reset_gpio;

    pr_info("%s enter, %s\n", __func__, dev->sensor_name);

    if(0x00 > reset_gpio) {
        return 0;
    }

    if(-1 != pwd_gpio) {
        kd_pin_mode(pwd_gpio, GPIO_DM_OUTPUT);
        kd_pin_write(pwd_gpio, GPIO_PV_LOW);
    }

    kd_pin_mode(reset_gpio, GPIO_DM_OUTPUT);

    if (on) {
        kd_pin_write(reset_gpio, GPIO_PV_HIGH);
        rt_thread_mdelay(delay);
        kd_pin_write(reset_gpio, GPIO_PV_LOW);
        rt_thread_mdelay(delay);
        kd_pin_write(reset_gpio, GPIO_PV_HIGH);
        rt_thread_mdelay(IMX415_POWER_STABLE_DELAY_MS);
    } else {
        kd_pin_write(reset_gpio, GPIO_PV_LOW);
    }

    return 0;
}

static k_s32 sensor_power_impl(void *ctx, k_s32 on)
{
    k_s32 ret = 0;
    struct sensor_driver_dev *dev = ctx;

    pr_info("%s enter, %s\n", __func__, dev->sensor_name);

    if (on) {
        if (dev->power_flag) {
            dev->init_flag = K_TRUE;
            return 0;
        }

        ret = _sensor_power_state_set(dev, K_TRUE, IMX415_POWER_DELAY_MS);
        if (!ret) {
            dev->power_flag = K_TRUE;
            dev->init_flag = K_TRUE;
        }
        return ret;
    }

    if (dev->power_flag) {
        ret |= _sensor_power_state_set(dev, K_FALSE, IMX415_POWER_DELAY_MS);
    }

    dev->power_flag = K_FALSE;
    dev->init_flag = K_FALSE;
    dev->mode_init_flag = K_FALSE;

    return ret;
}

static k_s32 sensor_init_impl(void *ctx, k_sensor_mode mode)
{
    k_s32 i = 0;
    k_s32 ret = 0;

    struct sensor_driver_dev *dev = ctx;
    k_sensor_mode *current_mode = &dev->current_sensor_mode;

    k_vicap_sensor_type type = mode.sensor_type;

    pr_info("%s enter, sensor_type:%d %s\n", __func__, type, dev->sensor_name);

    memset(current_mode, 0, sizeof(k_sensor_mode));

    for(k_u32 i = 0; i < dev->mode_count; i++) {
        if(dev->sensor_mode_list[i].sensor_type == type) {
            memcpy(current_mode, &dev->sensor_mode_list[i], sizeof(k_sensor_mode));
            memcpy(&current_mode->ae_info, current_mode->sensor_ae_info, sizeof(k_sensor_ae_info));
            break;
        }
    }

    if (NULL == current_mode->reg_list) {
        pr_err("%s, can not init for sensor type %d\n", __func__, type);
        return -1;
    }

    /*
     * Mirror / flip via REVERSE @0x3030: bit0=H, bit1=V.
     * Datasheet row/column reverse would remap CFA (H→BGGR, V→RGGB,
     * BOTH→GBRG). On this platform VICAP/ISP already compensates for the
     * mirror path, so keep BAYER_PAT_GBRG for all four settings.
     * Board-verified (H / V / BOTH): preview colors correct with GBRG.
     */
    k_sensor_reg sensor_mirror_reg_list[] = {
        { IMX415_REG_REVERSE, 0x00 },
        { REG_NULL, 0x00 },
    };
    switch (dev->mirror_setting.mirror) {
    case VICAP_MIRROR_NONE:
        sensor_mirror_reg_list[0].val = 0x00;
        current_mode->bayer_pattern = BAYER_PAT_GBRG;
        break;
    case VICAP_MIRROR_HOR:
        sensor_mirror_reg_list[0].val = 0x01;
        current_mode->bayer_pattern = BAYER_PAT_GBRG;
        break;
    case VICAP_MIRROR_VER:
        sensor_mirror_reg_list[0].val = 0x02;
        current_mode->bayer_pattern = BAYER_PAT_GBRG;
        break;
    case VICAP_MIRROR_BOTH:
        sensor_mirror_reg_list[0].val = 0x03;
        current_mode->bayer_pattern = BAYER_PAT_GBRG;
        break;
    default:
        pr_err("%s, not support mirror setting %d\n", __func__, dev->mirror_setting.mirror);
        break;
    }

    ret = sensor_reg_list_write(&dev->i2c_info, current_mode->reg_list);
    ret |= sensor_reg_list_write(&dev->i2c_info, sensor_mirror_reg_list);
    if (ret) {
        pr_err("%s: reg list write failed, ret=%d\n", __func__, ret);
        return ret;
    }

    current_mode->sensor_again = 0;
    current_mode->et_line = 0;

    k_u16 again_h, again_l;
    float again = 0, dgain = 0;

    ret = sensor_reg_read(&dev->i2c_info, IMX415_REG_AGAIN_L, &again_l);
    ret |= sensor_reg_read(&dev->i2c_info, IMX415_REG_AGAIN_H, &again_h);

    uint16_t reg_val = ((again_h & 0xff) << 8) | (again_l & 0xff);
    float dB = reg_val * 0.3f;
    again = powf(10, dB / 20.0f);

    dgain = 1.0;
    current_mode->ae_info.cur_again      = again;
    current_mode->ae_info.cur_long_again = again;
    current_mode->ae_info.cur_vs_again   = again;
    current_mode->ae_info.cur_dgain      = dgain;
    current_mode->ae_info.cur_long_dgain = dgain;
    current_mode->ae_info.cur_vs_dgain   = dgain;
    current_mode->ae_info.cur_gain = again * dgain;
    current_mode->ae_info.cur_long_gain = current_mode->ae_info.cur_gain;
    current_mode->ae_info.cur_vs_gain = current_mode->ae_info.cur_gain;

    if(current_mode->hdr_mode == SENSOR_MODE_LINEAR) {
        k_u16 SHR0_h, SHR0_m, SHR0_l;
        k_u32 shr0, exp_time;
        ret = sensor_reg_read(&dev->i2c_info, IMX415_REG_SHR0_L, &SHR0_l);
        ret |= sensor_reg_read(&dev->i2c_info, IMX415_REG_SHR0_M, &SHR0_m);
        ret |= sensor_reg_read(&dev->i2c_info, IMX415_REG_SHR0_H, &SHR0_h);
        shr0 = ((SHR0_h & 0x0f) << 16) | ((SHR0_m & 0xff) << 8) | (SHR0_l & 0xff);
        exp_time = IMX415_VMAX_LINEAR - shr0;

        current_mode->ae_info.cur_integration_time = current_mode->ae_info.one_line_exp_time * exp_time;
    } else {
        pr_err("%s, unsupport hdr_mode.\n", __func__);
    }

    dev->init_flag = K_TRUE;

    return ret;
}

static k_s32 sensor_get_chip_id_impl(void *ctx, k_u32 *chip_id)
{
    k_s32 ret = 0;
    struct sensor_driver_dev *dev = ctx;

    pr_info("%s enter, %s\n", __func__, dev->sensor_name);

    ret = _sensor_read_chip_id_r(dev, chip_id);

    if(ret) {
        pr_err("%s, iic read chip id err \n", __func__);
    }

    return ret;
}

static k_s32 sensor_get_mode_impl(void *ctx, k_sensor_mode *mode)
{
    struct sensor_driver_dev *dev = ctx;
    const k_vicap_sensor_type type = mode->sensor_type;
    k_sensor_mode *current_mode = &dev->current_sensor_mode;

    pr_info("%s enter, sensor_type(%d) %s\n", __func__, mode->sensor_type, dev->sensor_name);

    if(type == current_mode->sensor_type) {
        memcpy(mode, current_mode, sizeof(k_sensor_mode));
        return 0;
    }

    for(k_u32 i = 0; i < dev->mode_count; i++) {
        if(type == dev->sensor_mode_list[i].sensor_type) {
            memcpy(current_mode, &dev->sensor_mode_list[i], sizeof(k_sensor_mode));
            memcpy(&current_mode->ae_info, current_mode->sensor_ae_info, sizeof(k_sensor_ae_info));

            memcpy(mode, current_mode, sizeof(k_sensor_mode));
            return 0;
        }
    }

    pr_info("%s, the mode not exist.\n", __func__);

    return -1;
}

static k_s32 sensor_set_mode_impl(void *ctx, k_sensor_mode mode)
{
    k_s32 ret = 0;
    struct sensor_driver_dev *dev = ctx;
    (void)dev;

    pr_info("%s enter, %s\n", __func__, dev->sensor_name);

    return ret;
}

static k_s32 sensor_enum_mode_impl(void *ctx, k_sensor_enum_mode *enum_mode)
{
    k_s32 ret = 0;
    struct sensor_driver_dev *dev = ctx;
    (void)dev;

    pr_info("%s enter, %s\n", __func__, dev->sensor_name);
    memset(enum_mode, 0, sizeof(k_sensor_enum_mode));

    return ret;
}

static k_s32 sensor_get_caps_impl(void *ctx, k_sensor_caps *caps)
{
    k_s32 ret = 0;
    struct sensor_driver_dev *dev = ctx;
    k_sensor_mode *current_mode = &dev->current_sensor_mode;

    pr_info("%s enter, %s\n", __func__, dev->sensor_name);
    memset(caps, 0, sizeof(k_sensor_caps));

    caps->bit_width = current_mode->bit_width;
    caps->bayer_pattern = current_mode->bayer_pattern;
    caps->resolution.width = current_mode->size.width;
    caps->resolution.height = current_mode->size.height;

    return ret;
}

static k_s32 sensor_conn_check_impl(void *ctx, k_s32 *conn)
{
    k_s32 ret = 0;
    struct sensor_driver_dev *dev = ctx;
    (void)dev;

    pr_info("%s enter, %s\n", __func__, dev->sensor_name);
    *conn = 1;

    return ret;
}

static k_s32 sensor_set_stream_impl(void *ctx, k_s32 enable)
{
    k_s32 ret = 0;
    struct sensor_driver_dev *dev = ctx;

    pr_info("%s enter, enable(%d) %s\n", __func__, enable, dev->sensor_name);

    if (enable) {
        /* Leave standby, wait for internal settle, then XMSTA start */
        ret = sensor_reg_write(&dev->i2c_info, IMX415_REG_MODE_SELECT, IMX415_MODE_STREAMING);
        rt_thread_mdelay(80);
        ret |= sensor_reg_write(&dev->i2c_info, IMX415_REG_XMSTA, IMX415_XMSTA_START);
    } else {
        ret = sensor_reg_write(&dev->i2c_info, IMX415_REG_XMSTA, IMX415_XMSTA_STOP);
        ret |= sensor_reg_write(&dev->i2c_info, IMX415_REG_MODE_SELECT, IMX415_MODE_STANDBY);
    }

    return ret;
}

static k_s32 sensor_get_again_impl(void *ctx, k_sensor_gain *gain)
{
    k_s32 ret = 0;

    struct sensor_driver_dev *dev = ctx;
    k_sensor_mode *current_mode = &dev->current_sensor_mode;

    pr_info("%s enter, %s\n", __func__, dev->sensor_name);

    if (current_mode->hdr_mode == SENSOR_MODE_LINEAR) {
        gain->gain[SENSOR_LINEAR_PARAS] = current_mode->ae_info.cur_again;
    } else if (current_mode->hdr_mode == SENSOR_MODE_HDR_STITCH) {
        /* TODO */
        pr_err("%s, unsupport hdr_mode.\n", __func__);
        return -1;
    } else {
        pr_err("%s, unsupport exposure frame.\n", __func__);
        return -1;
    }

    return ret;
}

static k_s32 sensor_set_again_impl(void* ctx, k_sensor_gain gain)
{
    k_s32 ret = 0;
    k_u32 again;

    struct sensor_driver_dev* dev          = ctx;
    k_sensor_mode*            current_mode = &dev->current_sensor_mode;

    pr_info("%s enter, %s\n", __func__, dev->sensor_name);

    if (current_mode->hdr_mode == SENSOR_MODE_LINEAR) {
        float target_gain = gain.gain[SENSOR_LINEAR_PARAS];
        if (target_gain < 1.0f)
            target_gain = 1.0f;
        again = (k_u32)(((20.0f * log10f(target_gain)) / 0.3f) + 0.5f);
        if (again > IMX415_AGAIN_REG_MAX)
            again = IMX415_AGAIN_REG_MAX;
        if (current_mode->sensor_again != again) {
            ret = sensor_reg_write(&dev->i2c_info, IMX415_REG_HOLD, 0x01);
            ret |= sensor_reg_write(&dev->i2c_info, IMX415_REG_AGAIN_L, (again & 0xff));
            ret |= sensor_reg_write(&dev->i2c_info, IMX415_REG_AGAIN_H, (again >> 8) & 0xff);
            ret |= sensor_reg_write(&dev->i2c_info, IMX415_REG_HOLD, 0x00);
            current_mode->sensor_again = again;
            current_mode->ae_info.cur_again      = powf(10.0f, ((float)again * 0.3f) / 20.0f);
            current_mode->ae_info.cur_long_again = current_mode->ae_info.cur_again;
            current_mode->ae_info.cur_vs_again   = current_mode->ae_info.cur_again;
            current_mode->ae_info.cur_gain       = current_mode->ae_info.cur_again * current_mode->ae_info.cur_dgain;
            current_mode->ae_info.cur_long_gain  = current_mode->ae_info.cur_gain;
            current_mode->ae_info.cur_vs_gain    = current_mode->ae_info.cur_gain;
        }
    } else {
        pr_err("%s, unsupport hdr_mode.\n", __func__);
        return -1;
    }

    return ret;
}

static k_s32 sensor_get_dgain_impl(void *ctx, k_sensor_gain *gain)
{
    k_s32 ret = 0;
    struct sensor_driver_dev *dev = ctx;
    k_sensor_mode *current_mode = &dev->current_sensor_mode;

    pr_info("%s enter, %s\n", __func__, dev->sensor_name);

    if (current_mode->hdr_mode == SENSOR_MODE_LINEAR) {
        gain->gain[SENSOR_LINEAR_PARAS] = current_mode->ae_info.cur_dgain;
    } else if (current_mode->hdr_mode == SENSOR_MODE_HDR_STITCH) {
        gain->gain[SENSOR_DUAL_EXP_L_PARAS] = current_mode->ae_info.cur_dgain;
        gain->gain[SENSOR_DUAL_EXP_S_PARAS] = current_mode->ae_info.cur_vs_dgain;
    } else {
        pr_err("%s, unsupport exposure frame.\n", __func__);
        return -1;
    }

    return ret;
}

static k_s32 sensor_set_dgain_impl(void *ctx, k_sensor_gain gain)
{
    k_s32 ret = 0;
    k_u32 dgain;
    struct sensor_driver_dev *dev = ctx;
    k_sensor_mode *current_mode = &dev->current_sensor_mode;

    pr_info("%s enter, hdr_mode(%d) %s\n", __func__, current_mode->hdr_mode, dev->sensor_name);

    if (current_mode->hdr_mode == SENSOR_MODE_LINEAR) {
        dgain = (k_u32)(gain.gain[SENSOR_LINEAR_PARAS] * 1024);
        current_mode->ae_info.cur_dgain = dgain / 1024.0f;
    } else if (current_mode->hdr_mode == SENSOR_MODE_HDR_STITCH) {
        dgain = (k_u32)(gain.gain[SENSOR_DUAL_EXP_L_PARAS] * 1024);
        current_mode->ae_info.cur_long_dgain = dgain / 1024.0f;

        //dgain = (k_u32)(gain.gain[SENSOR_DUAL_EXP_S_PARAS] * 1024);
        // TODO wirte vs gain register
        current_mode->ae_info.cur_dgain = current_mode->ae_info.cur_long_dgain;
        current_mode->ae_info.cur_vs_dgain = current_mode->ae_info.cur_long_dgain;
    } else {
        pr_err("%s, unsupport hdr_mode.\n", __func__);
        return -1;
    }
    current_mode->ae_info.cur_gain = current_mode->ae_info.cur_again * current_mode->ae_info.cur_dgain;
    current_mode->ae_info.cur_long_gain = current_mode->ae_info.cur_gain;
    current_mode->ae_info.cur_vs_gain = current_mode->ae_info.cur_gain;

    pr_debug("%s,cur_gain(%d)\n", __func__, (k_u32)(current_mode->ae_info.cur_gain * 10000));

    return ret;
}

static k_s32 sensor_get_intg_time_impl(void *ctx, k_sensor_intg_time *time)
{
    k_s32 ret = 0;
    struct sensor_driver_dev *dev = ctx;
    k_sensor_mode *current_mode = &dev->current_sensor_mode;

    pr_info("%s enter, %s\n", __func__, dev->sensor_name);

    if (current_mode->hdr_mode == SENSOR_MODE_LINEAR) {
        time->intg_time[SENSOR_LINEAR_PARAS] = current_mode->ae_info.cur_integration_time;
    } else if (current_mode->hdr_mode == SENSOR_MODE_HDR_STITCH) {
        pr_err("%s, unsupport hdr_mode.\n", __func__);
        return -1;
    } else {
        pr_err("%s, unsupport hdr_mode.\n", __func__);
        return -1;
    }

    return ret;
}

static k_s32 sensor_set_intg_time_impl(void *ctx, k_sensor_intg_time time)
{
    k_s32 ret = 0;
    k_u16 exp_line = 0;
    float integraion_time = 0;
    struct sensor_driver_dev *dev = ctx;
    k_sensor_mode *current_mode = &dev->current_sensor_mode;

    pr_info("%s enter, %s\n", __func__, dev->sensor_name);

    if (current_mode->hdr_mode == SENSOR_MODE_LINEAR) {
        k_u32 SHR0;
        integraion_time = time.intg_time[SENSOR_LINEAR_PARAS];
        exp_line = integraion_time / current_mode->ae_info.one_line_exp_time;
        exp_line = MIN(current_mode->ae_info.max_integraion_line, MAX(current_mode->ae_info.min_integraion_line, exp_line));
        if (current_mode->et_line != exp_line) {
            SHR0 = IMX415_VMAX_LINEAR - exp_line;
            if (SHR0 < IMX415_SHR0_MIN)
                SHR0 = IMX415_SHR0_MIN;
            /* Hold around 3-byte SHR0 so shadow cannot latch a torn value */
            ret = sensor_reg_write(&dev->i2c_info, IMX415_REG_HOLD, 0x01);
            ret |= sensor_reg_write(&dev->i2c_info, IMX415_REG_SHR0_L, SHR0 & 0xff);
            ret |= sensor_reg_write(&dev->i2c_info, IMX415_REG_SHR0_M, (SHR0 >> 8) & 0xff);
            ret |= sensor_reg_write(&dev->i2c_info, IMX415_REG_SHR0_H, (SHR0 >> 16) & 0x0f);
            ret |= sensor_reg_write(&dev->i2c_info, IMX415_REG_HOLD, 0x00);
            current_mode->et_line = exp_line;
            current_mode->ae_info.cur_integration_time = (float)current_mode->et_line * current_mode->ae_info.one_line_exp_time;
        }
    } else {
        pr_err("%s, unsupport hdr_mode.\n", __func__);
        return -1;
    }

    return ret;
}

static k_s32 sensor_get_exp_parm_impl(void *ctx, k_sensor_exposure_param *exp_parm)
{
    k_s32 ret = 0;
    struct sensor_driver_dev *dev = ctx;
    (void)dev;

    pr_info("%s enter, %s\n", __func__, dev->sensor_name);
    memset(exp_parm, 0, sizeof(k_sensor_exposure_param));

    return ret;
}

static k_s32 sensor_set_exp_parm_impl(void *ctx, k_sensor_exposure_param exp_parm)
{
    k_s32 ret = 0;
    struct sensor_driver_dev *dev = ctx;
    (void)dev;

    pr_info("%s enter, %s\n", __func__, dev->sensor_name);

    return ret;
}

static k_s32 sensor_get_fps_impl(void *ctx, k_u32 *fps)
{
    k_s32 ret = 0;
    struct sensor_driver_dev *dev = ctx;
    (void)dev;

    pr_info("%s enter, %s\n", __func__, dev->sensor_name);
    *fps = 30000;

    return ret;
}

static k_s32 sensor_set_fps_impl(void *ctx, k_u32 fps)
{
    k_s32 ret = 0;
    struct sensor_driver_dev *dev = ctx;
    (void)dev;

    pr_info("%s enter, %s\n", __func__, dev->sensor_name);

    return ret;
}

static k_s32 sensor_get_isp_status_impl(void *ctx, k_sensor_isp_status *staus)
{
    k_s32 ret = 0;
    struct sensor_driver_dev *dev = ctx;
    (void)dev;

    pr_info("%s enter, %s\n", __func__, dev->sensor_name);
    memset(staus, 0, sizeof(k_sensor_isp_status));

    return ret;
}

static k_s32 sensor_set_blc_impl(void *ctx, k_sensor_blc blc)
{
    k_s32 ret = 0;
    struct sensor_driver_dev *dev = ctx;
    (void)dev;

    pr_info("%s enter, %s\n", __func__, dev->sensor_name);

    return ret;
}

static k_s32 sensor_set_wb_impl(void *ctx, k_sensor_white_balance wb)
{
    k_s32 ret = 0;
    struct sensor_driver_dev *dev = ctx;
    (void)dev;

    pr_info("%s enter, %s\n", __func__, dev->sensor_name);

    return ret;
}

static k_s32 sensor_get_tpg_impl(void *ctx, k_sensor_test_pattern *tpg)
{
    k_s32 ret = 0;
    struct sensor_driver_dev *dev = ctx;
    (void)dev;

    pr_info("%s enter, %s\n", __func__, dev->sensor_name);
    memset(tpg, 0, sizeof(k_sensor_test_pattern));

    return ret;
}

static k_s32 sensor_set_tpg_impl(void *ctx, k_sensor_test_pattern tpg)
{
    k_s32 ret = 0;
    struct sensor_driver_dev *dev = ctx;
    (void)dev;

    pr_info("%s enter, %s\n", __func__, dev->sensor_name);

    return ret;
}

static k_s32 sensor_get_expand_curve_impl(void *ctx, k_sensor_compand_curve *curve)
{
    k_s32 ret = 0;
    struct sensor_driver_dev *dev = ctx;
    (void)dev;

    pr_info("%s enter, %s\n", __func__, dev->sensor_name);
    memset(curve, 0, sizeof(k_sensor_compand_curve));

    return ret;
}

static k_s32 sensor_get_otp_data_impl(void *ctx, void *data)
{
    k_s32 ret = 0;
    struct sensor_driver_dev *dev = ctx;
    (void)dev;

    pr_info("%s enter, %s\n", __func__, dev->sensor_name);
    memset(data, 0, sizeof(void *));

    return ret;
}

static k_s32 sensor_mirror_set_impl(void *ctx, k_vicap_mirror_mode mirror)
{
    k_s32 ret = 0;
    struct sensor_driver_dev *dev = ctx;

    pr_info("mirror is %d , sensor tpye is %d, name is %s\n", mirror.mirror, mirror.sensor_type, dev->sensor_name);

    dev->mirror_setting = mirror;

    return ret;
}

static const k_sensor_function sensor_functions = {
    .sensor_power = sensor_power_impl,
    .sensor_init = sensor_init_impl,
    .sensor_get_chip_id = sensor_get_chip_id_impl,
    .sensor_get_mode = sensor_get_mode_impl,
    .sensor_set_mode = sensor_set_mode_impl,
    .sensor_enum_mode = sensor_enum_mode_impl,
    .sensor_get_caps = sensor_get_caps_impl,
    .sensor_conn_check = sensor_conn_check_impl,
    .sensor_set_stream = sensor_set_stream_impl,
    .sensor_get_again = sensor_get_again_impl,
    .sensor_set_again = sensor_set_again_impl,
    .sensor_get_dgain = sensor_get_dgain_impl,
    .sensor_set_dgain = sensor_set_dgain_impl,
    .sensor_get_intg_time = sensor_get_intg_time_impl,
    .sensor_set_intg_time = sensor_set_intg_time_impl,
    .sensor_get_exp_parm = sensor_get_exp_parm_impl,
    .sensor_set_exp_parm = sensor_set_exp_parm_impl,
    .sensor_get_fps = sensor_get_fps_impl,
    .sensor_set_fps = sensor_set_fps_impl,
    .sensor_get_isp_status = sensor_get_isp_status_impl,
    .sensor_set_blc = sensor_set_blc_impl,
    .sensor_set_wb = sensor_set_wb_impl,
    .sensor_get_tpg = sensor_get_tpg_impl,
    .sensor_set_tpg = sensor_set_tpg_impl,
    .sensor_get_expand_curve = sensor_get_expand_curve_impl,
    .sensor_get_otp_data = sensor_get_otp_data_impl,
    .sensor_mirror_set = sensor_mirror_set_impl,

    .sensor_set_focus_pos = sensor_autofocus_dev_set_position,
    .sensor_get_focus_pos = sensor_autofocus_dev_get_position,
    .sensor_get_foucs_cap = sensor_autofocus_dev_get_capability,
    .sensor_set_focus_power = sensor_autofocus_dev_power,
};
/*****************************************************************************/
k_s32 sensor_imx415_probe(struct k_sensor_probe_cfg *cfg, struct sensor_driver_dev *dev)
{
    k_u32 chip_id = 0;
    const k_sensor_mode *sensor_mode = NULL;

    /* Only CSI0 4-lane is supported (aggregates PHY1; CSI1 must stay unused). */
#if defined (CONFIG_MPP_ENABLE_CSI_DEV_0)
    if(0x00 == cfg->csi_num) {
        dev->mode_count = sizeof(sensor_csi0_mode_list) / sizeof(sensor_csi0_mode_list[0]);
        dev->sensor_mode_list = &sensor_csi0_mode_list[0];
        sensor_mode = &dev->sensor_mode_list[0];
    }
#endif // CONFIG_MPP_ENABLE_CSI_DEV_0

    if(0x00 == dev->mode_count) {
        goto _on_failed;
    }

    if(NULL == sensor_mode) {
        rt_kprintf("FATAL error, %s\n", __func__);
        goto _on_failed;
    }

    /* update dev */
    dev->pwd_gpio = cfg->pwd_gpio;
    dev->reset_gpio = cfg->reset_gpio;

    if(NULL == (dev->i2c_info.i2c_bus = rt_i2c_bus_device_find(cfg->i2c_name))) {
        rt_kprintf("Can't find %s\n", cfg->i2c_name);
        goto _on_failed;
    }
    strncpy(&dev->i2c_info.i2c_name[0], cfg->i2c_name, sizeof(dev->i2c_info.i2c_name));
    memcpy(&dev->sensor_func, &sensor_functions, sizeof(k_sensor_function));

    /* probe sensor — on-module 37.125 MHz crystal; mclk_setting is off */
    sensor_set_mclk(&sensor_mode->mclk_setting[0]);

    snprintf(dev->sensor_name, sizeof(dev->sensor_name), "imx415_csi%d", cfg->csi_num);

    _sensor_power_state_set(dev, 1, 1);
    dev->power_flag = K_TRUE;
    dev->init_flag = K_TRUE;

    dev->i2c_info.reg_addr_size = SENSOR_REG_VALUE_16BIT;
    dev->i2c_info.reg_val_size = SENSOR_REG_VALUE_8BIT;
    /* Waveshare IMX415-98: 7-bit 0x37 (not Sony default 0x1A used by IMX335) */
    dev->i2c_info.slave_addr = 0x37;

    if(0x00 != _sensor_read_chip_id_r(dev, &chip_id)) {
        goto _on_failed;
    }

    return 0;

_on_failed:
    memset(dev, 0, sizeof(*dev));

    return -1;
}

#endif // CONFIG_MPP_ENABLE_CSI_DEV_0
