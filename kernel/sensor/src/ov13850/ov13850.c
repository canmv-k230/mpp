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

#if 1//defined (CONFIG_MPP_ENABLE_CSI_DEV_0)

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
#define ov13850_REG_MODE_SELECT 0x100
#define ov13850_MODE_STANDBY 0x00
#define ov13850_MODE_STREAMING 0x01

/* Chip ID */
#define OV13850_REG_CHIP_ID		0x300a
#define OV13850_CHIP_ID			0xd850

/* Exposure control */
#define OV13850_REG_EXPOSURE		0x3500

/* Analog gain control */
#define ov13850_REG_AGAIN_L 0x350b
#define ov13850_REG_AGAIN_H 0x350a
#define ov13850_AGAIN_STEP (1.0f/256.0f)

/* Input clock rate */
#define ov13850_INCLK_RATE 24000000

/* CSI2 HW configuration */
#define ov13850_LINK_FREQ 594000000
#define ov13850_NUM_DATA_LANES 2

/* Mirror / flip (OV13850 datasheet 4.1 table 4-1, linux ov13850.c) */
#define OV13850_REG_TIMING_FMT1         0x3820  /* TIMING_REG20, bit[2]: vertical flip */
#define OV13850_REG_TIMING_FMT2         0x3821  /* TIMING_REG21, bit[2]: horizontal mirror */
#define OV13850_TIMING_VFLIP_BIT        0x04
#define OV13850_TIMING_MIRROR_BIT       0x04

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

    ret = sensor_reg_read(&dev->i2c_info, OV13850_REG_CHIP_ID, &id_high);
    ret |= sensor_reg_read(&dev->i2c_info, OV13850_REG_CHIP_ID + 1, &id_low);

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
    } else {
        kd_pin_write(reset_gpio, GPIO_PV_LOW);
    }
    rt_thread_mdelay(1);

    return 0;
}

static k_s32 sensor_power_impl(void *ctx, k_s32 on)
{
    k_s32 ret = 0;
    struct sensor_driver_dev *dev = ctx;

    pr_info("%s enter, %s\n", __func__, dev->sensor_name);

    _sensor_power_state_set(dev, on, 100);
    dev->init_flag = on;

    return ret;
}

/*
 * Bayer: mode table + ISP xml use BGGR (ISI_BPAT_BGBGGRGR). Mirror/flip only
 * change 3820/3821 readout; do not change bayer_pattern in mode_get.
 */
static k_s32 ov13850_apply_mirror_flip(struct sensor_driver_dev *dev, k_sensor_mode *current_mode)
{
    k_s32 ret = 0;
    k_u16 reg3820 = 0;
    k_u16 reg3821 = 0;
    k_vicap_mirror_mode mirror_cfg = dev->mirror_setting;

    ret = sensor_reg_read(&dev->i2c_info, OV13850_REG_TIMING_FMT1, &reg3820);
    ret |= sensor_reg_read(&dev->i2c_info, OV13850_REG_TIMING_FMT2, &reg3821);
    if (ret) {
        pr_err("%s, read timing reg failed\n", __func__);
        return ret;
    }

    /* Only touch bit[2]; bit[0] is binning (see init table / datasheet). */
    reg3820 &= (k_u16)~OV13850_TIMING_VFLIP_BIT;
    reg3821 &= (k_u16)~OV13850_TIMING_MIRROR_BIT;

    switch (mirror_cfg.mirror) {
    case VICAP_MIRROR_NONE:
        break;
    case VICAP_MIRROR_HOR:
        reg3821 |= OV13850_TIMING_MIRROR_BIT;
        break;
    case VICAP_MIRROR_VER:
        reg3820 |= OV13850_TIMING_VFLIP_BIT;
        break;
    case VICAP_MIRROR_BOTH:
        reg3820 |= OV13850_TIMING_VFLIP_BIT;
        reg3821 |= OV13850_TIMING_MIRROR_BIT;
        break;
    default:
        pr_err("%s, not support mirror setting %d\n", __func__, mirror_cfg.mirror);
        return -1;
    }

    ret = sensor_reg_write(&dev->i2c_info, OV13850_REG_TIMING_FMT1, reg3820);
    ret |= sensor_reg_write(&dev->i2c_info, OV13850_REG_TIMING_FMT2, reg3821);

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

    /* write mode table, then mirror/flip (gc2093 / ov5647 style) */
    ret = sensor_reg_list_write(&dev->i2c_info, current_mode->reg_list);
    ret |= ov13850_apply_mirror_flip(dev, current_mode);

    //current_mode->sensor_again = 0;
    current_mode->et_line = 0;

    k_u16 again_h, again_l;
    k_u32 again_reg;
    float again = 0, dgain = 0;
    k_u16 exp_line_hh, exp_line_h, exp_line_l;
    ret = sensor_reg_read(&dev->i2c_info,  ov13850_REG_AGAIN_L, &again_l);
    ret = sensor_reg_read(&dev->i2c_info,  ov13850_REG_AGAIN_H, &again_h);
    again_reg = (again_h << 8) | again_l;
    again = again_reg / 128.0f + 0.5f;
    current_mode->sensor_again = again_reg;

    //dgain = 1.0;
    current_mode->ae_info.cur_gain = again;
    current_mode->ae_info.cur_long_gain = current_mode->ae_info.cur_gain;
    current_mode->ae_info.cur_vs_gain = current_mode->ae_info.cur_gain;

	if(current_mode->hdr_mode == SENSOR_MODE_LINEAR) {
	    k_u32 exp_line;
	    ret = sensor_reg_read(&dev->i2c_info, OV13850_REG_EXPOSURE, &exp_line_hh);
	    ret |= sensor_reg_read(&dev->i2c_info, OV13850_REG_EXPOSURE+1, &exp_line_h);
        ret |= sensor_reg_read(&dev->i2c_info, OV13850_REG_EXPOSURE+2, &exp_line_l);
	    exp_line = ((exp_line_hh & 0xf) << 12) + (exp_line_h << 4) + (exp_line_l >> 4);  // Correct: each part shifted appropriately
        current_mode->et_line = exp_line;
	    current_mode->ae_info.cur_integration_time =  current_mode->ae_info.one_line_exp_time * exp_line;
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

    // if(chip_id && (OV13850_CHIP_ID != *chip_id)) {
    //     ret = -1;
    //     pr_err("%s, iic read chip id err \n", __func__);
    // }

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
        ret = sensor_reg_write(&dev->i2c_info, ov13850_REG_MODE_SELECT, ov13850_MODE_STREAMING);
        sensor_reg_write(&dev->i2c_info, 0x5100, 0x80);
    } else {
        ret = sensor_reg_write(&dev->i2c_info, ov13850_REG_MODE_SELECT, ov13850_MODE_STANDBY);
    }
    pr_info("%s exit, ret(%d)\n", __func__, ret);

    return ret;
}

static k_s32 sensor_get_again_impl(void *ctx, k_sensor_gain *gain)
{
    k_s32 ret = 0;

    struct sensor_driver_dev *dev = ctx;
    k_sensor_mode *current_mode = &dev->current_sensor_mode;

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

static k_s32 sensor_set_again_impl(void *ctx, k_sensor_gain gain)
{
    k_s32 ret = 0;
    k_u32 again, dgain, total;
    k_u8 i;
    float SensorGain;

    struct sensor_driver_dev *dev = ctx;
    k_sensor_mode *current_mode = &dev->current_sensor_mode;

    if (current_mode->hdr_mode == SENSOR_MODE_LINEAR) {
        again = (k_u32)((gain.gain[SENSOR_LINEAR_PARAS])*16.0f + 0.5f);
        if(again > 0x3ff)
            again = 0x3ff;
        if(current_mode->sensor_again != again) {
            ret = sensor_reg_write(&dev->i2c_info, ov13850_REG_AGAIN_L,(again & 0xff));
            ret |= sensor_reg_write(&dev->i2c_info, ov13850_REG_AGAIN_H, (again >> 8)&0x3f);
            current_mode->sensor_again = again;
            current_mode->ae_info.cur_gain = gain.gain[SENSOR_LINEAR_PARAS];
            current_mode->ae_info.cur_long_gain = current_mode->ae_info.cur_gain;
            current_mode->ae_info.cur_vs_gain = current_mode->ae_info.cur_gain;
            //rt_kprintf("%s, sensor_set_again_impl again:0x%x %u\n", __func__, again, (k_u32)(gain.gain[SENSOR_LINEAR_PARAS]*1000));
        }
        //SensorGain = (float)(current_mode->sensor_again);
    } else {
        rt_kprintf("%s, unsupport hdr_mode.\n", __func__);
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

uint32_t align_to_16(uint32_t line_num)
{
    return (line_num + 15) & (~15);
}

static k_s32 sensor_set_intg_time_impl(void *ctx, k_sensor_intg_time time)
{
    k_s32 ret = 0;
    uint32_t exp_line = 0;
    float integraion_time = 0;
    struct sensor_driver_dev *dev = ctx;
    k_sensor_mode *current_mode = &dev->current_sensor_mode;

    if (current_mode->hdr_mode == SENSOR_MODE_LINEAR) {
        integraion_time = time.intg_time[SENSOR_LINEAR_PARAS];
        exp_line = integraion_time / current_mode->ae_info.one_line_exp_time;
        exp_line = MIN(current_mode->ae_info.max_integraion_line, MAX(current_mode->ae_info.min_integraion_line, exp_line));
        /* OV13850 exposure register: 12-bit integer + 4-bit fraction
         * Need to shift left by 4 to get register value
         */
        if (current_mode->et_line != exp_line) {
            uint32_t reg_value = exp_line << 4;
            //rt_kprintf("%s enter, %s %d\n", __func__, dev->sensor_name, (k_u32)(integraion_time*1000000));
            /* Write register value:
             * 0x3500 = exp[19:16] = (reg_value >> 16) & 0x0F
             * 0x3501 = exp[15:8] = (reg_value >> 8) & 0xFF
             * 0x3502 = exp[7:0] = reg_value & 0xF0 (low 4 bits are fraction = 0)
             */
            ret = sensor_reg_write(&dev->i2c_info, OV13850_REG_EXPOSURE, (reg_value >> 16) & 0x0F);
            ret |= sensor_reg_write(&dev->i2c_info, OV13850_REG_EXPOSURE + 1, (reg_value >> 8) & 0xFF);
            ret |= sensor_reg_write(&dev->i2c_info, OV13850_REG_EXPOSURE + 2, reg_value & 0xF0);

            current_mode->et_line = exp_line;
            current_mode->ae_info.cur_integration_time = (float)current_mode->et_line * current_mode->ae_info.one_line_exp_time;
            //rt_kprintf("%s, sensor_set_intg_time_impl exp_line 0x%x.\n", __func__, exp_line);
        }
    } else {
        rt_kprintf("%s, unsupport hdr_mode.\n", __func__);
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
k_s32 sensor_ov13850_probe(struct k_sensor_probe_cfg *cfg, struct sensor_driver_dev *dev)
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
#if defined (CONFIG_MPP_ENABLE_CSI_DEV_1) && !defined (CONFIG_MPP_SENSOR_ov13850_ENABLE_4LANE_CONFIGURE)
    // if enable 4Lane configure support, will disable CSI1
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
    dev->pwd_gpio = -1;//cfg->pwd_gpio;
    dev->reset_gpio = -1;//cfg->reset_gpio;

    if(NULL == (dev->i2c_info.i2c_bus = rt_i2c_bus_device_find(cfg->i2c_name))) {
        rt_kprintf("Can't find %s\n", cfg->i2c_name);
        goto _on_failed;
    }
    strncpy(&dev->i2c_info.i2c_name[0], cfg->i2c_name, sizeof(dev->i2c_info.i2c_name));
    memcpy(&dev->sensor_func, &sensor_functions, sizeof(k_sensor_function));

    /* probe sensor */
    sensor_set_mclk(&sensor_mode->mclk_setting[0]);

    /** NEW SENSOR MODIFY START */
    snprintf(dev->sensor_name, sizeof(dev->sensor_name), "ov13850_csi%d", cfg->csi_num);

    _sensor_power_state_set(dev, 1, 1);

    dev->i2c_info.reg_addr_size = SENSOR_REG_VALUE_16BIT;
    dev->i2c_info.reg_val_size = SENSOR_REG_VALUE_8BIT;
    dev->i2c_info.slave_addr = 0x10;//0x36
    if((0x00 != _sensor_read_chip_id_r(dev, &chip_id))/* || (OV13850_CHIP_ID != chip_id) */) {
        // rt_kprintf("ov13850 read chip id failed, 0x%04x\n", chip_id);
        goto _on_failed;
    }

    if (OV13850_CHIP_ID != chip_id) {
        rt_kprintf("TODO: ov13850 read chip id maybe failed, 0x%04x != 0x%04x\n", OV13850_CHIP_ID, chip_id);
    }

    sensor_autofocus_dev_probe(dev);
    /** NEW SENSOR MODIFY END */

    return 0;

_on_failed:
    memset(dev, 0, sizeof(*dev));

    return -1;
}

#endif // CONFIG_MPP_ENABLE_CSI_DEV_0
