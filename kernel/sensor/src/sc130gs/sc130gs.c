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
#include "sensor_dev.h"
#include "io.h"
#include "drv_gpio.h"
#include <stdio.h>

#define pr_info(...) //rt_kprintf(__VA_ARGS__)
#define pr_debug(...) //rt_kprintf(__VA_ARGS__)
#define pr_warn(...)    //rt_kprintf(__VA_ARGS__)
#define pr_err(...)    rt_kprintf(__VA_ARGS__)

/* Sensor private ************************************************************/
/* Chip ID */
#define SC130GS_CHIP_ID         (0x0130)

/* Exposure control */

/* Analog gain control */

#define SC130GS_MIN_GAIN_STEP    (1.0f/32.0f)
#define SC130GS_POWER_DELAY_MS   CANMV_SENSOR_POWER_RESET_DELAY_MS
#define SC130GS_POWER_STABLE_DELAY_MS  CANMV_SENSOR_POWER_STABLE_DELAY_MS

/* Mirror/flip: reg 0x3221 — bit[2:1] mirror (00 off, 11 on), bit[6:5] flip (00 off, 11 on) */
#define SC130GS_REG_MIRROR_FLIP     (0x3221)
#define SC130GS_MIRROR_FIELD_MASK   ((k_u16)(0x3u << 1)) /* bits [2:1] */
#define SC130GS_FLIP_FIELD_MASK     ((k_u16)(0x3u << 5)) /* bits [6:5] */
#define SC130GS_MIRROR_FLIP_MASK    (k_u16)(SC130GS_MIRROR_FIELD_MASK | SC130GS_FLIP_FIELD_MASK)

/* include sensor register configure */
#include "sensor_reg_table.c"

#if defined (CONFIG_MPP_ENABLE_CSI_DEV_0)
    #include "sensor_csi0_mode_list.c"
#endif // CONFIG_MPP_ENABLE_CSI_DEV_0

#if defined (CONFIG_MPP_ENABLE_CSI_DEV_1)
    #include "sensor_csi1_mode_list.c"
#endif // CONFIG_MPP_ENABLE_CSI_DEV_1

#if defined (CONFIG_MPP_ENABLE_CSI_DEV_2)
    #include "sensor_csi2_mode_list.c"
#endif // CONFIG_MPP_ENABLE_CSI_DEV_2

static k_s32 _sensor_read_chip_id_r(struct sensor_driver_dev *dev, k_u32 *chip_id)
{
    k_s32 ret = 0;
    k_u16 id_high = 0;
    k_u16 id_low = 0;

    const k_s32 pwd_gpio = dev->pwd_gpio;
    const k_s32 reset_gpio = dev->reset_gpio;

    pr_info("%s enter, %s\n", __func__, dev->sensor_name);

    if(NULL == dev->i2c_info.i2c_bus) {
        pr_err("%s no i2c bus\n", dev->i2c_info.i2c_bus);
        return -1;
    }

    if(0 <= reset_gpio) {
        kd_pin_mode(reset_gpio, GPIO_DM_OUTPUT);
        kd_pin_write(reset_gpio, GPIO_PV_HIGH);
    }
    rt_thread_mdelay(1); // wait reset stable.

    ret = sensor_reg_read(&dev->i2c_info, 0x3107, &id_high);
    ret |= sensor_reg_read(&dev->i2c_info, 0x3107 + 1, &id_low);

    if(chip_id) {
        *chip_id = (id_high << 8) | id_low;
        pr_info("%s chip id 0x%x\n", __func__, *chip_id);
    }

    return ret;
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
        rt_thread_mdelay(SC130GS_POWER_STABLE_DELAY_MS);
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

        ret = _sensor_power_state_set(dev, K_TRUE, SC130GS_POWER_DELAY_MS);
        if (!ret) {
            dev->power_flag = K_TRUE;
            dev->init_flag = K_TRUE;
        }
        return ret;
    }

    if (dev->power_flag) {
        ret |= _sensor_power_state_set(dev, K_FALSE, SC130GS_POWER_DELAY_MS);
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
     * Soft reset (0x0103=1) often NACKs mid-transaction because the sensor
     * resets during the I2C write. Treat that as success and wait for recover.
     */
    (void)sensor_reg_write(&dev->i2c_info, 0x0103, 0x01);
    rt_thread_mdelay(10);

    ret = sensor_reg_list_write(&dev->i2c_info, current_mode->reg_list);
    if (ret) {
        pr_err("%s, write reg list failed\n", __func__);
        return -1;
    }

    {
        k_u16 r3221 = 0;
        k_u16 mf = 0;
        k_s32 r_mf = sensor_reg_read(&dev->i2c_info, SC130GS_REG_MIRROR_FLIP, &r3221);

        if (r_mf != 0) {
            r3221 = 0; /* modes without 0x3221 in table: assume mirror/flip fields off */
        }
        r3221 = (k_u16)(r3221 & ~SC130GS_MIRROR_FLIP_MASK);
        switch (dev->mirror_setting.mirror) {
        case VICAP_MIRROR_NONE:
            mf = 0;
            current_mode->bayer_pattern = BAYER_PAT_BGGR;
            break;
       case VICAP_MIRROR_HOR:
           mf = SC130GS_MIRROR_FIELD_MASK;
           current_mode->bayer_pattern = BAYER_PAT_BGGR;
           break;
       case VICAP_MIRROR_VER:
           mf = SC130GS_FLIP_FIELD_MASK;
           current_mode->bayer_pattern = BAYER_PAT_BGGR;
           break;
       case VICAP_MIRROR_BOTH:
           mf = (k_u16)(SC130GS_MIRROR_FIELD_MASK | SC130GS_FLIP_FIELD_MASK);
           current_mode->bayer_pattern = BAYER_PAT_BGGR;
           break;
        default:
            pr_err("%s, not support mirror setting %d\n", __func__, dev->mirror_setting.mirror);
            mf = 0;
            current_mode->bayer_pattern = BAYER_PAT_BGGR;
            break;
        }
        r3221 = (k_u16)(r3221 | mf);
        /* Mirror is best-effort; do not fail init if 0x3221 is unavailable. */
        (void)sensor_reg_write(&dev->i2c_info, SC130GS_REG_MIRROR_FLIP, r3221);
    }

    current_mode->sensor_again = 0;
    current_mode->et_line = 0;

    // sensor custom init.
    k_u16 exp_time;
    float again = 0, dgain = 0;

    again = 1.0;
    dgain = 1.0;
    current_mode->ae_info.cur_again = again;
    current_mode->ae_info.cur_dgain = dgain;
    current_mode->ae_info.cur_gain = again * dgain;
    current_mode->ae_info.cur_long_gain = current_mode->ae_info.cur_gain;
    current_mode->ae_info.cur_vs_gain = current_mode->ae_info.cur_gain;

    /*
     * AE requires non-zero cur_integration_time. Match init table
     * 0x3e01/0x3e02 = 0x2050 (line*16). Do not I2C-read during init.
     */
    exp_time = 0x2050; /* sc130gs_mipi_2lane_1280x1024_90fps_init */
    current_mode->et_line = exp_time / 16;
    current_mode->ae_info.cur_integration_time =
        current_mode->ae_info.one_line_exp_time * (float)current_mode->et_line;

    dev->init_flag = K_TRUE;

    return ret;
}

static k_s32 sensor_get_chip_id_impl(void *ctx, k_u32 *chip_id)
{
    k_s32 ret = 0;
    struct sensor_driver_dev *dev = ctx;

    pr_info("%s enter, %s\n", __func__, dev->sensor_name);

    ret = _sensor_read_chip_id_r(dev, chip_id);

    if(chip_id && (SC130GS_CHIP_ID != *chip_id)) {
        ret = -1;
        pr_err("%s, iic read chip id err \n", __func__);
    }

    return ret;
}

static k_s32 sensor_get_mode_impl(void *ctx, k_sensor_mode *mode)
{
    struct sensor_driver_dev *dev = ctx;
    k_vicap_sensor_type type = mode->sensor_type;
    k_sensor_mode *current_mode = &dev->current_sensor_mode;

    pr_info("%s enter, sensor_type(%d) %s\n", __func__, mode->sensor_type, dev->sensor_name);

    if(current_mode->sensor_type == type) {
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
        ret = sensor_reg_write(&dev->i2c_info, 0x0100, 0x01);
    } else {
        ret = sensor_reg_write(&dev->i2c_info, 0x0100, 0x00);
    }
    pr_info("%s exit, ret(%d)\n", __func__, ret);

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
        gain->gain[SENSOR_DUAL_EXP_L_PARAS] = current_mode->ae_info.cur_again;
        gain->gain[SENSOR_DUAL_EXP_S_PARAS] = current_mode->ae_info.cur_vs_again;
    } else {
        pr_err("%s, unsupport exposure frame.\n", __func__);
        return -1;
    }

    return ret;
}

static k_s32 sensor_set_again_impl(void *ctx, k_sensor_gain gain)
{
    k_s32 ret = 0;
    k_u32 again;
    k_u32 coarse_again_reg;
    k_u32 fine_again_reg;
    k_u32 a_gain;

    struct sensor_driver_dev *dev = ctx;
    k_sensor_mode *current_mode = &dev->current_sensor_mode;

    if (current_mode->hdr_mode == SENSOR_MODE_LINEAR) {
        /* Map float gain to Linux-style a_gain index (0x20~0xF8, 1x = 0x20). */
        again = (k_u32)(gain.gain[SENSOR_LINEAR_PARAS] * 1000.0f + 0.5f);
        a_gain = (k_u32)(gain.gain[SENSOR_LINEAR_PARAS] * 32.0f + 0.5f);
        if (a_gain < 0x20)
            a_gain = 0x20;
        if (a_gain > 0xF8)
            a_gain = 0xF8;

        if (a_gain <= 0x3e) {
            /* 1x~1.9375 */
            fine_again_reg = a_gain & 0x3F;
            coarse_again_reg = 0x03;
        } else if (a_gain <= 0x5D) {
            /* 1.95~3.778 */
            fine_again_reg = ((a_gain - 0x3F) + 0x20) & 0x3F;
            coarse_again_reg = 0x07;
        } else if (a_gain <= 0x78) {
            /* 3.84~6.96 */
            fine_again_reg = ((a_gain - 0x5E) + 0x20) & 0x3F;
            coarse_again_reg = 0x0F;
        } else if (a_gain <= 0x98) {
            /* 7.04~13.86 */
            fine_again_reg = ((a_gain - 0x79) + 0x20) & 0x3F;
            coarse_again_reg = 0x1F;
        } else if (a_gain <= 0xB8) {
            /* 14.08~27.72 */
            fine_again_reg = ((a_gain - 0x99) + 0x20) & 0x3F;
            coarse_again_reg = 0x3F;
        } else if (a_gain <= 0xD8) {
            /* 28.16~55.44 */
            fine_again_reg = ((a_gain - 0xB9) + 0x20) & 0x3F;
            coarse_again_reg = 0x7F;
        } else {
            /* 56.32~110.88 */
            fine_again_reg = ((a_gain - 0xD9) + 0x20) & 0x3F;
            coarse_again_reg = 0xFF;
        }

        ret = sensor_reg_write(&dev->i2c_info, 0x3e08, coarse_again_reg);
        ret |= sensor_reg_write(&dev->i2c_info, 0x3e09, fine_again_reg);

        /* ini gain-switch: [gain<4] / [gain>8] */
        if (gain.gain[SENSOR_LINEAR_PARAS] < 4.0f) {
            ret |= sensor_reg_write(&dev->i2c_info, 0x363a, 0x24);
            ret |= sensor_reg_write(&dev->i2c_info, 0x3630, 0x63);
        } else if (gain.gain[SENSOR_LINEAR_PARAS] > 8.0f) {
            ret |= sensor_reg_write(&dev->i2c_info, 0x363a, 0x64);
            ret |= sensor_reg_write(&dev->i2c_info, 0x3630, 0x73);
        }

        current_mode->sensor_again = again;
        current_mode->ae_info.cur_again = (float)current_mode->sensor_again / 1000.0f;
    } else if (current_mode->hdr_mode == SENSOR_MODE_HDR_STITCH) {
        again = (k_u16)(gain.gain[SENSOR_LINEAR_PARAS] * 64 + 0.5);
        current_mode->ae_info.cur_again = (float)current_mode->sensor_again / 64.0f;
    } else {
        pr_err("%s, unsupport exposure frame.\n", __func__);
        return -1;
    }
    pr_debug("%s, hdr_mode(%d), cur_again(%u)\n", __func__, current_mode->hdr_mode, (k_u32)(current_mode->ae_info.cur_again * 1000));

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
        //ret = sensor_reg_write(&dev->i2c_info, SC130GS_REG_LONG_AGAIN_H,(again & 0x0300)>>8);
        //ret |= sensor_reg_write(&dev->i2c_info, SC130GS_REG_LONG_AGAIN_L,(again & 0xff));
        current_mode->ae_info.cur_dgain = (float)dgain/1024.0f;

    } else if (current_mode->hdr_mode == SENSOR_MODE_HDR_STITCH) {
        dgain = (k_u32)(gain.gain[SENSOR_DUAL_EXP_L_PARAS] * 1024);
        //ret = sensor_reg_write(&dev->i2c_info, SC130GS_REG_LONG_AGAIN_H,(again & 0x0300)>>8);
        //ret |= sensor_reg_write(&dev->i2c_info, SC130GS_REG_LONG_AGAIN_L,(again & 0xff));
        current_mode->ae_info.cur_dgain = (float)dgain/1024.0f;

        dgain = (k_u32)(gain.gain[SENSOR_DUAL_EXP_S_PARAS] * 1024);
        //TODO wirte vs gain register
        current_mode->ae_info.cur_vs_dgain = (float)dgain/1024.0f;
    } else {
        pr_err("%s, unsupport exposure frame.\n", __func__);
        return -1;
    }
    current_mode->ae_info.cur_gain = current_mode->ae_info.cur_again * current_mode->ae_info.cur_dgain;
    pr_debug("%s,cur_gain(%d)\n", __func__, (k_u32)(current_mode->ae_info.cur_gain*10000));
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
        time->intg_time[SENSOR_DUAL_EXP_L_PARAS] = current_mode->ae_info.cur_integration_time;
        time->intg_time[SENSOR_DUAL_EXP_S_PARAS] = current_mode->ae_info.cur_vs_integration_time;
    } else {
        pr_err("%s, unsupport exposure frame.\n", __func__);
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

    if (current_mode->hdr_mode == SENSOR_MODE_LINEAR) {
        integraion_time = time.intg_time[SENSOR_LINEAR_PARAS];
        exp_line = integraion_time / current_mode->ae_info.one_line_exp_time;
        exp_line = MIN(current_mode->ae_info.max_integraion_line, MAX(current_mode->ae_info.min_integraion_line, exp_line));

        /* Skip I2C write when exposure line is unchanged. */
        if (current_mode->et_line != exp_line) {
            k_u16 exp_reg = exp_line * 16;
            ret |= sensor_reg_write(&dev->i2c_info, 0x3e01, (exp_reg >> 8) & 0xff);
            ret |= sensor_reg_write(&dev->i2c_info, 0x3e02, exp_reg & 0xff);
            current_mode->et_line = exp_line;
        }
        current_mode->ae_info.cur_integration_time = (float)current_mode->et_line * current_mode->ae_info.one_line_exp_time;
    } else if (current_mode->hdr_mode == SENSOR_MODE_HDR_STITCH) {
        integraion_time = time.intg_time[SENSOR_DUAL_EXP_L_PARAS];
        exp_line = integraion_time / current_mode->ae_info.one_line_exp_time;
        exp_line = MIN(current_mode->ae_info.max_integraion_line, MAX(1, exp_line));

        current_mode->ae_info.cur_integration_time = (float)exp_line * current_mode->ae_info.one_line_exp_time;

        integraion_time = time.intg_time[SENSOR_DUAL_EXP_S_PARAS];
        exp_line = integraion_time / current_mode->ae_info.one_line_exp_time;
        exp_line = MIN(current_mode->ae_info.max_integraion_line, MAX(1, exp_line));

        current_mode->ae_info.cur_vs_integration_time = (float)exp_line * current_mode->ae_info.one_line_exp_time;
    } else {
        pr_err("%s, unsupport exposure frame.\n", __func__);
        return -1;
    }
    pr_debug("%s hdr_mode(%d), exp_line(%d), integraion_time(%u)\n",\
        __func__, current_mode->hdr_mode, exp_line, (k_u32)(integraion_time * 1000000000));

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

    pr_info("%s enter, %s\n", __func__, dev->sensor_name);
    *fps = dev->current_sensor_mode.fps;

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
k_s32 sensor_sc130gs_probe(struct k_sensor_probe_cfg *cfg, struct sensor_driver_dev *dev)
{
    k_s32 ret = 0;
    k_u32 chip_id = 0;
    const k_sensor_mode *sensor_mode = NULL;

#if defined (CONFIG_MPP_ENABLE_CSI_DEV_0)
    if(0x00 == cfg->csi_num) {
        dev->mode_count = sizeof(sensor_csi0_mode_list) / sizeof(sensor_csi0_mode_list[0]);
        dev->sensor_mode_list = &sensor_csi0_mode_list[0];
        sensor_mode = &dev->sensor_mode_list[0];
    } else
#endif // CONFIG_MPP_ENABLE_CSI_DEV_0
#if defined (CONFIG_MPP_ENABLE_CSI_DEV_1)
    if(0x01 == cfg->csi_num) {
        dev->mode_count = sizeof(sensor_csi1_mode_list) / sizeof(sensor_csi1_mode_list[0]);
        dev->sensor_mode_list = &sensor_csi1_mode_list[0];
        sensor_mode = &dev->sensor_mode_list[0];
    } else
#endif // CONFIG_MPP_ENABLE_CSI_DEV_1
#if defined (CONFIG_MPP_ENABLE_CSI_DEV_2)
    if(0x02 == cfg->csi_num) {
        dev->mode_count = sizeof(sensor_csi2_mode_list) / sizeof(sensor_csi2_mode_list[0]);
        dev->sensor_mode_list = &sensor_csi2_mode_list[0];
        sensor_mode = &dev->sensor_mode_list[0];
    }
#endif // CONFIG_MPP_ENABLE_CSI_DEV_2

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

    /* probe sensor */
    sensor_set_mclk(&sensor_mode->mclk_setting[0]);

    /** NEW SENSOR MODIFY START */
    snprintf(dev->sensor_name, sizeof(dev->sensor_name), "sc130gs_csi%d", cfg->csi_num);

    _sensor_power_state_set(dev, 1, 1);
    dev->power_flag = K_TRUE;
    dev->init_flag = K_TRUE;

    /* probe different slave address */
    dev->i2c_info.reg_addr_size = SENSOR_REG_VALUE_16BIT;
    dev->i2c_info.reg_val_size = SENSOR_REG_VALUE_8BIT;
    dev->i2c_info.slave_addr = 0x30; /* SC130GS */
    if((0x00 != _sensor_read_chip_id_r(dev, &chip_id)) || (SC130GS_CHIP_ID != chip_id)) {
        // rt_kprintf("sc130gs read chip id failed, 0x%04x\n", chip_id);
        goto _on_failed;
    }

    sensor_autofocus_dev_probe(dev);
    /** NEW SENSOR MODIFY END */

    return 0;

_on_failed:
    memset(dev, 0, sizeof(*dev));

    return -1;
}
