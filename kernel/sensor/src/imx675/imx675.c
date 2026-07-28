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
#include <math.h>
#include <string.h>

#include "io.h"
#include "rtthread.h"

#include "drv_gpio.h"

#include "sensor_dev.h"

#define pr_info(...)  /* rt_kprintf(__VA_ARGS__) */
#define pr_debug(...) /*rt_kprintf(__VA_ARGS__)*/
#define pr_warn(...)  /*rt_kprintf(__VA_ARGS__)*/
#define pr_err(...)   rt_kprintf(__VA_ARGS__)

/* float -> integer for rt_kprintf (no %f on RTOS) */
#define IMX675_GAIN_TO_MILLI(g) ((k_u32)(((g) * 1000.0f) + 0.5f))
#define IMX675_TIME_TO_MICRO(t) ((k_u32)(((t) * 1000000.0f) + 0.5f))

/* Streaming Mode */
#define IMX675_REG_MODE_SELECT 0x3000
#define IMX675_MODE_STANDBY    0x01
#define IMX675_MODE_STREAMING  0x00

#define IMX675_REG_MASTER_SELECT 0x3002
#define IMX675_MASTER_ON         0x00
#define IMX675_MASTER_OFF        0x01

/* Lines per frame */
#define IMX675_REG_LPFR 0x3030

/* Chip ID */
// MX675没有获取ID的寄存器，借用PIX_VWIDTH低位+高位寄存器，默认值0x07ac
#define IMX675_REG_ID 0x3046
#define IMX675_ID     0xac07

/* Exposure control */
#define IMX675_REG_SHR0_L       0x3050
#define IMX675_REG_SHR0_M       0x3051
#define IMX675_REG_SHR1_L       0x3054
#define IMX675_REG_SHR1_M       0x3055
#define IMX675_REG_SHR2_L       0x3058
#define IMX675_REG_SHR2_M       0x3059
#define IMX675_REG_RHS1_L       0x3060
#define IMX675_REG_RHS1_M       0x3061
#define IMX675_REG_RHS2_L       0x3064
#define IMX675_REG_RHS2_M       0x3065
#define IMX675_VMAX_LINEAR      2250 // 4500
#define IMX675_VMAX_IMX675_DOL2 3980
#define IMX675_VMAX_IMX675_DOL3 4500

/* Analog gain control */
#define IMX675_REG_AGAIN_L 0x3070
#define IMX675_REG_AGAIN_H 0x3071
#define IMX675_AGAIN_STEP  (1.0f / 256.0f)

/* Group hold register */
#define IMX675_REG_HOLD 0x3001

/* Input clock rate */
#define IMX675_INCLK_RATE            74250000
#define IMX675_POWER_DELAY_MS        CANMV_SENSOR_POWER_RESET_DELAY_MS
#define IMX675_POWER_STABLE_DELAY_MS CANMV_SENSOR_POWER_STABLE_DELAY_MS

/* CSI2 HW configuration */
#define IMX675_LINK_FREQ      594000000
#define IMX675_NUM_DATA_LANES 2

#define IMX675_REG_MIN 0x00
#define IMX675_REG_MAX 0xfffff

#define IMX675_DOL2_RHS1 482
#define IMX675_DOL3_RHS1 986
#define IMX675_DOL3_RHS2 2608 // 1072

#define IMX675_DOL2_ratio    16.0
#define IMX675_DOL3_LS_ratio 16.0
#define IMX675_DOL3_VS_ratio 16.0

#define IMX675_REG_HREVERSE 0x3020
#define IMX675_REG_VREVERSE 0x3021

#include "sensor_reg_table.c"

#if defined(CONFIG_MPP_ENABLE_CSI_DEV_0)
#include "sensor_csi0_mode_list.c"
#endif
#if defined(CONFIG_MPP_ENABLE_CSI_DEV_1)
#include "sensor_csi1_mode_list.c"
#endif
#if defined(CONFIG_MPP_ENABLE_CSI_DEV_2)
#include "sensor_csi2_mode_list.c"
#endif

static k_s32 _sensor_read_chip_id_r(struct sensor_driver_dev* dev, k_u32* chip_id)
{
    k_s32 ret     = 0;
    k_u16 id_high = 0;
    k_u16 id_low  = 0;

    if (NULL == dev->i2c_info.i2c_bus) {
        return -1;
    }

    ret = sensor_reg_read(&dev->i2c_info, IMX675_REG_ID, &id_high);
    ret |= sensor_reg_read(&dev->i2c_info, IMX675_REG_ID + 1, &id_low);
    if (ret) {
        return -1;
    }

    if (chip_id) {
        *chip_id = (id_high << 8) | id_low;
    }

    return 0;
}

static int _sensor_power_state_set(struct sensor_driver_dev* dev, k_s32 on, k_u32 delay)
{
    const k_s32 pwd_gpio   = dev->pwd_gpio;
    const k_s32 reset_gpio = dev->reset_gpio;

    if (reset_gpio < 0) {
        return 0;
    }

    if (pwd_gpio >= 0) {
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
        rt_thread_mdelay(IMX675_POWER_STABLE_DELAY_MS);
    } else {
        kd_pin_write(reset_gpio, GPIO_PV_LOW);
    }

    return 0;
}

static k_s32 imx675_sensor_get_chip_id(void* ctx, k_u32* chip_id)
{
    struct sensor_driver_dev* dev = ctx;

    return _sensor_read_chip_id_r(dev, chip_id);
}

static k_s32 imx675_sensor_power_on(void* ctx, k_s32 on)
{
    k_s32                     ret = 0;
    struct sensor_driver_dev* dev = ctx;

    if (on) {
        if (dev->power_flag) {
            dev->init_flag = K_TRUE;
            return 0;
        }

        ret = _sensor_power_state_set(dev, K_TRUE, IMX675_POWER_DELAY_MS);
        if (!ret) {
            dev->power_flag = K_TRUE;
            dev->init_flag  = K_TRUE;
        }
        return ret;
    }

    if (dev->power_flag) {
        ret |= _sensor_power_state_set(dev, K_FALSE, IMX675_POWER_DELAY_MS);
    }

    dev->power_flag     = K_FALSE;
    dev->init_flag      = K_FALSE;
    dev->mode_init_flag = K_FALSE;

    return ret;
}

/*
 * IMX675 module default (Sony Excel mode 4) uses HREVERSE=1, VREVERSE=1.
 * VICAP_MIRROR_NONE means "correct orientation for this module", not sensor-native 0/0.
 */
static k_s32 imx675_apply_mirror(struct sensor_driver_dev* dev, k_sensor_mode* mode)
{
    k_u8  hreverse;
    k_u8  vreverse;
    k_s32 ret;

    switch (dev->mirror_setting.mirror) {
    case VICAP_MIRROR_NONE:
        hreverse            = 0x01;
        vreverse            = 0x01;
        mode->bayer_pattern = BAYER_PAT_RGGB;
        break;
    case VICAP_MIRROR_HOR:
        hreverse            = 0x00;
        vreverse            = 0x01;
        mode->bayer_pattern = BAYER_PAT_GRBG;
        break;
    case VICAP_MIRROR_VER:
        hreverse            = 0x01;
        vreverse            = 0x00;
        mode->bayer_pattern = BAYER_PAT_GBRG;
        break;
    case VICAP_MIRROR_BOTH:
        hreverse            = 0x00;
        vreverse            = 0x00;
        mode->bayer_pattern = BAYER_PAT_RGGB;
        break;
    default:
        pr_err("%s, unsupported mirror %d\n", __func__, dev->mirror_setting.mirror);
        return -1;
    }

    ret = sensor_reg_write(&dev->i2c_info, IMX675_REG_HOLD, 0x01);
    ret |= sensor_reg_write(&dev->i2c_info, IMX675_REG_HREVERSE, hreverse);
    ret |= sensor_reg_write(&dev->i2c_info, IMX675_REG_VREVERSE, vreverse);
    ret |= sensor_reg_write(&dev->i2c_info, IMX675_REG_HOLD, 0x00);

    return ret;
}

static k_s32 imx675_sensor_init(void* ctx, k_sensor_mode mode)
{
    k_s32                     ret          = 0;
    struct sensor_driver_dev* dev          = ctx;
    k_sensor_mode*            current_mode = &dev->current_sensor_mode;
    k_vicap_sensor_type       type         = mode.sensor_type;

    memset(current_mode, 0, sizeof(k_sensor_mode));

    for (k_u32 i = 0; i < dev->mode_count; i++) {
        if (dev->sensor_mode_list[i].sensor_type == type) {
            memcpy(current_mode, &dev->sensor_mode_list[i], sizeof(k_sensor_mode));
            memcpy(&current_mode->ae_info, current_mode->sensor_ae_info, sizeof(k_sensor_ae_info));
            break;
        }
    }

    if (NULL == current_mode->reg_list) {
        pr_err("%s, can not init for sensor type %d\n", __func__, type);
        return -1;
    }

    ret = sensor_reg_list_write(&dev->i2c_info, current_mode->reg_list);
    ret |= imx675_apply_mirror(dev, current_mode);

    k_u16 again_h, again_l;
    float again = 0, dgain = 0;

    ret = sensor_reg_read(&dev->i2c_info, IMX675_REG_AGAIN_L, &again_l);
    ret = sensor_reg_read(&dev->i2c_info, IMX675_REG_AGAIN_H, &again_h);
    // again = (float)(((again_h & 0x07) << 8) | again_l) * 0.015f;    //db value/20, (RegVal * 3/10)/20
    again = (float)((again_h & 0x07) << 8 | again_l) * 0.015f;
    again = powf(10, again); // times value

    dgain                                = 1.0;
    current_mode->ae_info.cur_again      = again;
    current_mode->ae_info.cur_long_again = again;
    current_mode->ae_info.cur_vs_again   = again;
    current_mode->ae_info.cur_dgain      = dgain;
    current_mode->ae_info.cur_long_dgain = dgain;
    current_mode->ae_info.cur_vs_dgain   = dgain;
    current_mode->ae_info.cur_gain       = again * dgain;
    current_mode->ae_info.cur_long_gain  = current_mode->ae_info.cur_gain;
    current_mode->ae_info.cur_vs_gain    = current_mode->ae_info.cur_gain;

    if (current_mode->hdr_mode == SENSOR_MODE_LINEAR) {
        k_u16 SHR0_m, SHR0_l;
        k_u32 exp_time;
        ret = sensor_reg_read(&dev->i2c_info, IMX675_REG_SHR0_L, &SHR0_l);
        ret |= sensor_reg_read(&dev->i2c_info, IMX675_REG_SHR0_M, &SHR0_m);
        exp_time = IMX675_VMAX_LINEAR - ((SHR0_m << 8) | SHR0_l);

        current_mode->ae_info.cur_integration_time = current_mode->ae_info.one_line_exp_time * exp_time;

        pr_debug("[imx675] %s: sensor_type=%d again_reg=0x%03x again_milli=%u "
                 "exp_line=%u exp_us=%u shr0=%u\n",
                 __func__, type, (k_u32)(((again_h & 0x07) << 8) | again_l), IMX675_GAIN_TO_MILLI(again), exp_time,
                 IMX675_TIME_TO_MICRO(current_mode->ae_info.cur_integration_time), (k_u32)(IMX675_VMAX_LINEAR - exp_time));
    }

    dev->init_flag = K_TRUE;
    pr_debug("[imx675] %s: done sensor_type=%d ret=%d\n", __func__, type, ret);
    return ret;
}

static k_s32 imx675_sensor_get_mode(void* ctx, k_sensor_mode* mode)
{
    struct sensor_driver_dev* dev          = ctx;
    k_sensor_mode*            current_mode = &dev->current_sensor_mode;
    k_vicap_sensor_type       type         = mode->sensor_type;

    if (current_mode->sensor_type == type) {
        memcpy(mode, current_mode, sizeof(k_sensor_mode));
        return 0;
    }

    for (k_u32 i = 0; i < dev->mode_count; i++) {
        if (type == dev->sensor_mode_list[i].sensor_type) {
            memcpy(current_mode, &dev->sensor_mode_list[i], sizeof(k_sensor_mode));
            memcpy(&current_mode->ae_info, current_mode->sensor_ae_info, sizeof(k_sensor_ae_info));
            memcpy(mode, current_mode, sizeof(k_sensor_mode));
            return 0;
        }
    }

    return -1;
}

static k_s32 imx675_sensor_set_mode(void* ctx, k_sensor_mode mode)
{
    k_s32 ret = 0;

    pr_debug("%s enter\n", __func__);

    return ret;
}

static k_s32 imx675_sensor_enum_mode(void* ctx, k_sensor_enum_mode* enum_mode)
{
    struct sensor_driver_dev* dev = ctx;

    if (enum_mode->index >= dev->mode_count) {
        return -1;
    }

    memcpy(&enum_mode->mode, &dev->sensor_mode_list[enum_mode->index], sizeof(k_sensor_mode));
    return 0;
}

static k_s32 imx675_sensor_get_caps(void* ctx, k_sensor_caps* caps)
{
    struct sensor_driver_dev* dev          = ctx;
    k_sensor_mode*            current_mode = &dev->current_sensor_mode;

    memset(caps, 0, sizeof(k_sensor_caps));
    caps->bit_width         = current_mode->bit_width;
    caps->bayer_pattern     = current_mode->bayer_pattern;
    caps->resolution.width  = current_mode->size.width;
    caps->resolution.height = current_mode->size.height;

    return 0;
}

static k_s32 imx675_sensor_conn_check(void* ctx, k_s32* conn)
{
    k_s32 ret = 0;

    pr_debug("%s enter\n", __func__);
    *conn = 1;

    return ret;
}

static k_s32 imx675_sensor_set_stream(void* ctx, k_s32 enable)
{
    k_s32                     ret = 0;
    struct sensor_driver_dev* dev = ctx;

    pr_debug("%s enter, enable(%d)\n", __func__, enable);
    if (enable) {
        ret = sensor_reg_write(&dev->i2c_info, IMX675_REG_MODE_SELECT, IMX675_MODE_STREAMING);
        rt_thread_mdelay(100);
        ret = sensor_reg_write(&dev->i2c_info, IMX675_REG_MASTER_SELECT, IMX675_MASTER_ON);
        rt_thread_mdelay(100);
    } else {
        ret = sensor_reg_write(&dev->i2c_info, IMX675_REG_MODE_SELECT, IMX675_MODE_STANDBY);
        ret = sensor_reg_write(&dev->i2c_info, IMX675_REG_MASTER_SELECT, IMX675_MASTER_OFF);
    }
    pr_debug("%s exit, ret(%d)\n", __func__, ret);

    return ret;
}

static k_s32 imx675_sensor_get_again(void* ctx, k_sensor_gain* gain)
{
    struct sensor_driver_dev* dev          = ctx;
    k_sensor_mode*            current_mode = &dev->current_sensor_mode;

    if (current_mode->hdr_mode == SENSOR_MODE_LINEAR) {
        gain->gain[SENSOR_LINEAR_PARAS] = current_mode->ae_info.cur_again;
    } else if (current_mode->hdr_mode == SENSOR_MODE_HDR_STITCH) {
        if (current_mode->index == 6) {
            gain->gain[SENSOR_DUAL_EXP_L_PARAS] = current_mode->ae_info.cur_long_again;
            gain->gain[SENSOR_DUAL_EXP_S_PARAS] = current_mode->ae_info.cur_again;
        } else if (current_mode->index == 7) {
            gain->gain[SENSOR_TRI_EXP_L_PARAS]  = current_mode->ae_info.cur_long_again;
            gain->gain[SENSOR_TRI_EXP_S_PARAS]  = current_mode->ae_info.cur_again;
            gain->gain[SENSOR_TRI_EXP_VS_PARAS] = current_mode->ae_info.cur_vs_again;
        }
    } else {
        pr_err("%s, unsupport exposure frame.\n", __func__);
        return -1;
    }

    return 0;
}

static k_s32 imx675_sensor_set_again(void* ctx, k_sensor_gain gain)
{
    k_s32                     ret = 0;
    k_u16                     again;
    float                     SensorGain;
    struct sensor_driver_dev* dev          = ctx;
    k_sensor_mode*            current_mode = &dev->current_sensor_mode;

    if (current_mode->hdr_mode == SENSOR_MODE_LINEAR) {
        again = (k_u16)(log10f(gain.gain[SENSOR_LINEAR_PARAS]) * 200.0f / 3.0f + 0.5f); // 20*log(gain)*10/3
        if (current_mode->sensor_again != again) {
            ret = sensor_reg_write(&dev->i2c_info, IMX675_REG_AGAIN_L, (again & 0xff));
            ret |= sensor_reg_write(&dev->i2c_info, IMX675_REG_AGAIN_H, (again & 0x0700) >> 8);
            current_mode->sensor_again      = again;
            SensorGain                      = (float)(current_mode->sensor_again) * 0.015f; // db value/20,(RegVal * 3/10)/20
            current_mode->ae_info.cur_again = powf(10, SensorGain);
            pr_debug("[imx675] %s: req_gain_milli=%u again_reg=0x%03x cur_again_milli=%u\n", __func__,
                     IMX675_GAIN_TO_MILLI(gain.gain[SENSOR_LINEAR_PARAS]), (k_u32)again,
                     IMX675_GAIN_TO_MILLI(current_mode->ae_info.cur_again));
        }
    } else if (current_mode->hdr_mode == SENSOR_MODE_HDR_STITCH) {
        again = (k_u16)(log10f(gain.gain[SENSOR_DUAL_EXP_L_PARAS]) * 200.0f / 3.0f + 0.5f); // 20*log(gain)*10/3
        ret   = sensor_reg_write(&dev->i2c_info, IMX675_REG_AGAIN_L, (again & 0xff));
        ret |= sensor_reg_write(&dev->i2c_info, IMX675_REG_AGAIN_H, (again & 0x0700) >> 8);

        SensorGain                           = (float)(again) * 0.015f; // db value/20,(RegVal * 3/10)/20
        current_mode->ae_info.cur_long_again = powf(10, SensorGain);

        // again = (k_u32)(gain.gain[SENSOR_DUAL_EXP_S_PARAS] * 16);
        //  TODO
        // current_mode->ae_info.cur_vs_again = again / 16.0f;
        current_mode->ae_info.cur_again    = current_mode->ae_info.cur_long_again;
        current_mode->ae_info.cur_vs_again = current_mode->ae_info.cur_long_again;
    } else {
        pr_err("%s, unsupport exposure frame.\n", __func__);
        return -1;
    }

    return ret;
}

static k_s32 imx675_sensor_get_dgain(void* ctx, k_sensor_gain* gain)
{
    struct sensor_driver_dev* dev          = ctx;
    k_sensor_mode*            current_mode = &dev->current_sensor_mode;

    if (current_mode->hdr_mode == SENSOR_MODE_LINEAR) {
        gain->gain[SENSOR_LINEAR_PARAS] = current_mode->ae_info.cur_dgain;
    } else if (current_mode->hdr_mode == SENSOR_MODE_HDR_STITCH) {
        gain->gain[SENSOR_DUAL_EXP_L_PARAS] = current_mode->ae_info.cur_dgain;
        gain->gain[SENSOR_DUAL_EXP_S_PARAS] = current_mode->ae_info.cur_vs_dgain;
    } else {
        pr_err("%s, unsupport exposure frame.\n", __func__);
        return -1;
    }

    return 0;
}

static k_s32 imx675_sensor_set_dgain(void* ctx, k_sensor_gain gain)
{
    k_s32                     ret = 0;
    k_u32                     dgain;
    struct sensor_driver_dev* dev          = ctx;
    k_sensor_mode*            current_mode = &dev->current_sensor_mode;
    if (current_mode->hdr_mode == SENSOR_MODE_LINEAR) {
        dgain                           = (k_u32)(gain.gain[SENSOR_LINEAR_PARAS] * 1024);
        current_mode->ae_info.cur_dgain = dgain / 1024.0f;

    } else if (current_mode->hdr_mode == SENSOR_MODE_HDR_STITCH) {
        dgain                                = (k_u32)(gain.gain[SENSOR_DUAL_EXP_L_PARAS] * 1024);
        current_mode->ae_info.cur_long_dgain = dgain / 1024.0f;

        // dgain = (k_u32)(gain.gain[SENSOR_DUAL_EXP_S_PARAS] * 1024);
        //  TODO wirte vs gain register
        current_mode->ae_info.cur_dgain    = current_mode->ae_info.cur_long_dgain;
        current_mode->ae_info.cur_vs_dgain = current_mode->ae_info.cur_long_dgain;
    } else {
        pr_err("%s, unsupport exposure frame.\n", __func__);
        return -1;
    }
    current_mode->ae_info.cur_gain      = current_mode->ae_info.cur_again * current_mode->ae_info.cur_dgain;
    current_mode->ae_info.cur_long_gain = current_mode->ae_info.cur_gain;
    current_mode->ae_info.cur_vs_gain   = current_mode->ae_info.cur_gain;
    pr_debug("%s,cur_gain(%d)\n", __func__, (k_u32)(current_mode->ae_info.cur_gain * 10000));

    return ret;
}

static k_s32 imx675_sensor_get_intg_time(void* ctx, k_sensor_intg_time* time)
{
    struct sensor_driver_dev* dev          = ctx;
    k_sensor_mode*            current_mode = &dev->current_sensor_mode;

    if (current_mode->hdr_mode == SENSOR_MODE_LINEAR) {
        time->intg_time[SENSOR_LINEAR_PARAS] = current_mode->ae_info.cur_integration_time;
    } else if (current_mode->hdr_mode == SENSOR_MODE_HDR_STITCH) {
        if (current_mode->index == 6) {
            time->intg_time[SENSOR_DUAL_EXP_L_PARAS] = current_mode->ae_info.cur_long_integration_time;
            time->intg_time[SENSOR_DUAL_EXP_S_PARAS] = current_mode->ae_info.cur_integration_time;
        } else if (current_mode->index == 7) {
            time->intg_time[SENSOR_TRI_EXP_L_PARAS]  = current_mode->ae_info.cur_long_integration_time;
            time->intg_time[SENSOR_TRI_EXP_S_PARAS]  = current_mode->ae_info.cur_integration_time;
            time->intg_time[SENSOR_TRI_EXP_VS_PARAS] = current_mode->ae_info.cur_vs_integration_time;
        }
    } else {
        pr_err("%s, unsupport exposure frame.\n", __func__);
        return -1;
    }

    return 0;
}

static k_s32 imx675_sensor_set_intg_time(void* ctx, k_sensor_intg_time time)
{
    k_s32                     ret             = 0;
    k_u32                     exp_line        = 0;
    float                     integraion_time = 0;
    struct sensor_driver_dev* dev             = ctx;
    k_sensor_mode*            current_mode    = &dev->current_sensor_mode;

    if (current_mode->hdr_mode == SENSOR_MODE_LINEAR) {
        k_u16 shr0 = 0;

        integraion_time = time.intg_time[SENSOR_LINEAR_PARAS];
        exp_line        = integraion_time / current_mode->ae_info.one_line_exp_time;
        exp_line = MIN(current_mode->ae_info.max_integraion_line, MAX(current_mode->ae_info.min_integraion_line, exp_line));
        if (current_mode->et_line != exp_line) {
            shr0 = IMX675_VMAX_LINEAR - exp_line;
            ret  = sensor_reg_write(&dev->i2c_info, IMX675_REG_SHR0_L, shr0 & 0xff);
            ret |= sensor_reg_write(&dev->i2c_info, IMX675_REG_SHR0_M, (shr0 >> 8) & 0xff);
            current_mode->et_line                      = exp_line;
            current_mode->ae_info.cur_integration_time = (float)current_mode->et_line * current_mode->ae_info.one_line_exp_time;
            pr_debug("[imx675] %s: req_exp_us=%u exp_line=%u shr0=%u cur_exp_us=%u\n", __func__,
                     IMX675_TIME_TO_MICRO(integraion_time), exp_line, (k_u32)shr0,
                     IMX675_TIME_TO_MICRO(current_mode->ae_info.cur_integration_time));
        }
    } else if (current_mode->hdr_mode == SENSOR_MODE_HDR_STITCH) {
        integraion_time = time.intg_time[SENSOR_DUAL_EXP_L_PARAS];
        exp_line        = integraion_time / current_mode->ae_info.one_line_exp_time;
        exp_line        = MIN(current_mode->ae_info.max_long_integraion_line,
                              MAX(current_mode->ae_info.min_long_integraion_line, exp_line));
        if (current_mode->index == 6) {
            exp_line         = ((exp_line + 2) >> 2) << 2;
            float temp       = (float)exp_line / (4.0 * IMX675_DOL2_ratio);
            k_u16 exp_s_line = (int)(temp + 0.5) << 2;
            exp_s_line
                = MIN(current_mode->ae_info.max_integraion_line, MAX(current_mode->ae_info.min_integraion_line, exp_s_line));
            if (current_mode->et_line != exp_line) {
                k_u16 SHR0 = current_mode->ae_info.frame_length - exp_line;
                k_u16 RHS1 = IMX675_DOL2_RHS1;
                k_u16 SHR1 = RHS1 - exp_s_line;
                ret        = sensor_reg_write(&dev->i2c_info, IMX675_REG_SHR0_L, SHR0 & 0xff);
                ret |= sensor_reg_write(&dev->i2c_info, IMX675_REG_SHR0_M, (SHR0 >> 8) & 0xff);
                ret |= sensor_reg_write(&dev->i2c_info, IMX675_REG_SHR1_L, SHR1 & 0xff);
                ret |= sensor_reg_write(&dev->i2c_info, IMX675_REG_SHR1_M, (SHR1 >> 8) & 0xff);
                current_mode->et_line = exp_line;
            }
            current_mode->ae_info.cur_long_integration_time = current_mode->ae_info.one_line_exp_time * exp_line;
            current_mode->ae_info.cur_integration_time      = current_mode->ae_info.one_line_exp_time * exp_s_line;
        } else if (current_mode->index == 7) {
            float temp       = (float)exp_line / 6;
            exp_line         = (int)(temp + 0.5) * 6;
            temp             = (float)exp_line / (6.0 * IMX675_DOL3_LS_ratio);
            k_u16 exp_s_line = (int)(temp + 0.5) * 6;
            exp_s_line
                = MIN(current_mode->ae_info.max_integraion_line, MAX(current_mode->ae_info.min_integraion_line, exp_s_line));
            temp              = (float)exp_line / (6.0 * IMX675_DOL3_LS_ratio * IMX675_DOL3_VS_ratio);
            k_u16 exp_vs_line = (int)(temp + 0.5) * 6;
            exp_vs_line       = MIN(current_mode->ae_info.max_vs_integraion_line,
                                    MAX(current_mode->ae_info.min_vs_integraion_line, exp_vs_line));
            if (current_mode->et_line != exp_line) {
                k_u16 SHR0 = current_mode->ae_info.frame_length - exp_line;
                k_u16 RHS1 = IMX675_DOL3_RHS1;
                k_u16 SHR1 = RHS1 - exp_s_line;
                k_u16 RHS2 = IMX675_DOL3_RHS2;
                k_u16 SHR2 = RHS2 - exp_vs_line;
                ret        = sensor_reg_write(&dev->i2c_info, IMX675_REG_SHR0_L, SHR0 & 0xff);
                ret |= sensor_reg_write(&dev->i2c_info, IMX675_REG_SHR0_M, (SHR0 >> 8) & 0xff);
                ret |= sensor_reg_write(&dev->i2c_info, IMX675_REG_SHR1_L, SHR1 & 0xff);
                ret |= sensor_reg_write(&dev->i2c_info, IMX675_REG_SHR1_M, (SHR1 >> 8) & 0xff);
                ret |= sensor_reg_write(&dev->i2c_info, IMX675_REG_SHR1_L, SHR2 & 0xff);
                ret |= sensor_reg_write(&dev->i2c_info, IMX675_REG_SHR1_M, (SHR2 >> 8) & 0xff);
                current_mode->et_line = exp_line;
            }
            current_mode->ae_info.cur_long_integration_time = current_mode->ae_info.one_line_exp_time * exp_line;
            current_mode->ae_info.cur_integration_time      = current_mode->ae_info.one_line_exp_time * exp_s_line;
            current_mode->ae_info.cur_vs_integration_time   = current_mode->ae_info.one_line_exp_time * exp_vs_line;
        }
    } else {
        pr_err("%s, unsupport exposure frame.\n", __func__);
        return -1;
    }
    return ret;
}

static k_s32 imx675_sensor_get_exp_parm(void* ctx, k_sensor_exposure_param* exp_parm)
{
    k_s32 ret = 0;

    pr_debug("%s enter\n", __func__);
    memset(exp_parm, 0, sizeof(k_sensor_exposure_param));

    return ret;
}

static k_s32 imx675_sensor_set_exp_parm(void* ctx, k_sensor_exposure_param exp_parm)
{
    k_s32 ret = 0;

    pr_debug("%s enter\n", __func__);

    return ret;
}

static k_s32 imx675_sensor_get_fps(void* ctx, k_u32* fps)
{
    struct sensor_driver_dev* dev          = ctx;
    k_sensor_mode*            current_mode = &dev->current_sensor_mode;

    *fps = current_mode->fps;

    return 0;
}

static k_s32 imx675_sensor_set_fps(void* ctx, k_u32 fps)
{
    k_s32 ret = 0;

    pr_debug("%s enter\n", __func__);

    return ret;
}

static k_s32 imx675_sensor_get_isp_status(void* ctx, k_sensor_isp_status* staus)
{
    k_s32 ret = 0;

    pr_debug("%s enter\n", __func__);
    memset(staus, 0, sizeof(k_sensor_isp_status));

    return ret;
}

static k_s32 imx675_sensor_set_blc(void* ctx, k_sensor_blc blc)
{
    k_s32 ret = 0;

    pr_debug("%s enter\n", __func__);

    return ret;
}

static k_s32 imx675_sensor_set_wb(void* ctx, k_sensor_white_balance wb)
{
    k_s32 ret = 0;

    pr_debug("%s enter\n", __func__);

    return ret;
}

static k_s32 imx675_sensor_get_tpg(void* ctx, k_sensor_test_pattern* tpg)
{
    k_s32 ret = 0;

    pr_debug("%s enter\n", __func__);
    memset(tpg, 0, sizeof(k_sensor_test_pattern));

    return ret;
}

static k_s32 imx675_sensor_set_tpg(void* ctx, k_sensor_test_pattern tpg)
{
    k_s32 ret = 0;

    pr_debug("%s enter\n", __func__);

    return ret;
}

static k_s32 imx675_sensor_get_expand_curve(void* ctx, k_sensor_compand_curve* curve)
{
    k_s32 ret = 0;

    pr_debug("%s enter\n", __func__);
    memset(curve, 0, sizeof(k_sensor_compand_curve));

    return ret;
}

static k_s32 imx675_sensor_get_otp_data(void* ctx, void* data)
{
    k_s32 ret = 0;

    pr_debug("%s enter\n", __func__);
    memset(data, 0, sizeof(void*));

    return ret;
}

static k_s32 imx675_sensor_mirror_set(void* ctx, k_vicap_mirror_mode mirror)
{
    struct sensor_driver_dev* dev = ctx;

    dev->mirror_setting = mirror;
    if (dev->init_flag) {
        return imx675_apply_mirror(dev, &dev->current_sensor_mode);
    }
    return 0;
}

static const k_sensor_function sensor_functions = {
    .sensor_power            = imx675_sensor_power_on,
    .sensor_init             = imx675_sensor_init,
    .sensor_get_chip_id      = imx675_sensor_get_chip_id,
    .sensor_get_mode         = imx675_sensor_get_mode,
    .sensor_set_mode         = imx675_sensor_set_mode,
    .sensor_enum_mode        = imx675_sensor_enum_mode,
    .sensor_get_caps         = imx675_sensor_get_caps,
    .sensor_conn_check       = imx675_sensor_conn_check,
    .sensor_set_stream       = imx675_sensor_set_stream,
    .sensor_get_again        = imx675_sensor_get_again,
    .sensor_set_again        = imx675_sensor_set_again,
    .sensor_get_dgain        = imx675_sensor_get_dgain,
    .sensor_set_dgain        = imx675_sensor_set_dgain,
    .sensor_get_intg_time    = imx675_sensor_get_intg_time,
    .sensor_set_intg_time    = imx675_sensor_set_intg_time,
    .sensor_get_exp_parm     = imx675_sensor_get_exp_parm,
    .sensor_set_exp_parm     = imx675_sensor_set_exp_parm,
    .sensor_get_fps          = imx675_sensor_get_fps,
    .sensor_set_fps          = imx675_sensor_set_fps,
    .sensor_get_isp_status   = imx675_sensor_get_isp_status,
    .sensor_set_blc          = imx675_sensor_set_blc,
    .sensor_set_wb           = imx675_sensor_set_wb,
    .sensor_get_tpg          = imx675_sensor_get_tpg,
    .sensor_set_tpg          = imx675_sensor_set_tpg,
    .sensor_get_expand_curve = imx675_sensor_get_expand_curve,
    .sensor_get_otp_data     = imx675_sensor_get_otp_data,
    .sensor_mirror_set       = imx675_sensor_mirror_set,

    .sensor_set_focus_pos   = sensor_autofocus_dev_set_position,
    .sensor_get_focus_pos   = sensor_autofocus_dev_get_position,
    .sensor_get_foucs_cap   = sensor_autofocus_dev_get_capability,
    .sensor_set_focus_power = sensor_autofocus_dev_power,
};

k_s32 sensor_imx675_probe(struct k_sensor_probe_cfg* cfg, struct sensor_driver_dev* dev)
{
    k_u32                chip_id     = 0;
    const k_sensor_mode* sensor_mode = NULL;

#if defined(CONFIG_MPP_ENABLE_CSI_DEV_0)
    if (0x00 == cfg->csi_num) {
        dev->mode_count       = sizeof(sensor_csi0_mode_list) / sizeof(sensor_csi0_mode_list[0]);
        dev->sensor_mode_list = &sensor_csi0_mode_list[0];
        sensor_mode           = &dev->sensor_mode_list[0];
    } else
#endif
#if defined(CONFIG_MPP_ENABLE_CSI_DEV_1)
        if (0x01 == cfg->csi_num) {
        dev->mode_count       = sizeof(sensor_csi1_mode_list) / sizeof(sensor_csi1_mode_list[0]);
        dev->sensor_mode_list = &sensor_csi1_mode_list[0];
        sensor_mode           = &dev->sensor_mode_list[0];
    } else
#endif
#if defined(CONFIG_MPP_ENABLE_CSI_DEV_2)
        if (0x02 == cfg->csi_num) {
        dev->mode_count       = sizeof(sensor_csi2_mode_list) / sizeof(sensor_csi2_mode_list[0]);
        dev->sensor_mode_list = &sensor_csi2_mode_list[0];
        sensor_mode           = &dev->sensor_mode_list[0];
    }
#endif

    if (0x00 == dev->mode_count || NULL == sensor_mode) {
        goto _on_failed;
    }

    dev->pwd_gpio   = cfg->pwd_gpio;
    dev->reset_gpio = cfg->reset_gpio;

    if (NULL == (dev->i2c_info.i2c_bus = rt_i2c_bus_device_find(cfg->i2c_name))) {
        goto _on_failed;
    }
    strncpy(dev->i2c_info.i2c_name, cfg->i2c_name, sizeof(dev->i2c_info.i2c_name) - 1);
    memcpy(&dev->sensor_func, &sensor_functions, sizeof(k_sensor_function));

    sensor_set_mclk(&sensor_mode->mclk_setting[0]);
    snprintf(dev->sensor_name, sizeof(dev->sensor_name), "imx675_csi%d", cfg->csi_num);

    _sensor_power_state_set(dev, 1, 1);
    dev->power_flag = K_TRUE;
    dev->init_flag  = K_TRUE;

    dev->i2c_info.reg_addr_size = SENSOR_REG_VALUE_16BIT;
    dev->i2c_info.reg_val_size  = SENSOR_REG_VALUE_8BIT;
    dev->i2c_info.slave_addr    = 0x1a;

    if (0x00 != _sensor_read_chip_id_r(dev, &chip_id)) {
        goto _on_failed;
    }

    if (IMX675_ID != chip_id) {
        rt_kprintf("imx675 chip id 0x%04x (expect 0x%04x)\n", chip_id, IMX675_ID);
        goto _on_failed;
    }

    sensor_autofocus_dev_probe(dev);

    return 0;

_on_failed:
    memset(dev, 0, sizeof(*dev));
    return -1;
}
