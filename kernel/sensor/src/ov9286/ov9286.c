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

#define pr_info(...)
#define pr_debug(...)
#define pr_warn(...)
#define pr_err(...)    rt_kprintf(__VA_ARGS__)

/* Chip ID at 0x300a/0x300b. 7-bit I2C address 0x60 (8-bit 0xC0). */
#define OV9286_CHIP_ID              (0x9286)
#define OV9286_I2C_ADDR             (0x60)

#define OV9286_REG_CHIP_ID_H        (0x300a)
#define OV9286_REG_CHIP_ID_L        (0x300b)
#define OV9286_REG_EXP_H            (0x3501)
#define OV9286_REG_EXP_L            (0x3502)
#define OV9286_REG_AGAIN            (0x3509)

/* 30fps VTS=2193, one line 15.2us, exposure cap 2.5ms. 60fps VTS=1915. */
#define OV9286_30_VTS               (2193)
#define OV9286_30_FPS               (30)
#define OV9286_30_ONE_LINE          (0.0000152f)
#define OV9286_30_MAX_EXP_LINE      (165)
#define OV9286_60_VTS               (1915)
#define OV9286_60_FPS               (60)
#define OV9286_60_ONE_LINE          (0.0000087f)
#define OV9286_60_MAX_EXP_LINE      (287)

/* Analog gain register 0x3509 is gain*16, 1x..15.9375x. */
#define OV9286_MIN_GAIN_STEP        (1.0f / 16.0f)
#define OV9286_MAX_AGAIN            (15.9375f)
#define OV9286_AGAIN_REG_MAX        (0xff)

#define OV9286_POWER_DELAY_MS       CANMV_SENSOR_POWER_RESET_DELAY_MS
#define OV9286_POWER_STABLE_DELAY_MS CANMV_SENSOR_POWER_STABLE_DELAY_MS
#define OV9286_SOFT_RESET_DELAY_MS  (20)

/* J3 pin 36 / 44 / 32. Shutdown nets are active-low. */
#define OV9286_IR_XSHUT_GPIO        (27)
#define OV9286_RGB_XSHUT_GPIO       (46)
#define OV9286_PS_SHUT_GPIO         (48)
#define OV9286_PS_INT_GPIO          (47)

#include "sensor_reg_table.c"

/* CSI menu "external clock" leaves MCLK_NUM at -1. Chip-MCLK mode still
 * needs a clock id, so fall back to chip MCLK1. A selected MCLK2/MCLK3 wins.
 */
#if defined (CONFIG_MPP_ENABLE_SENSOR_OV9286_ON_CSI0_USE_CHIP_CLK)
#if defined (CONFIG_MPP_CSI_DEV0_MCLK_NUM) && (CONFIG_MPP_CSI_DEV0_MCLK_NUM >= 1) && (CONFIG_MPP_CSI_DEV0_MCLK_NUM <= 3)
#define OV9286_CSI0_MCLK_ID CONFIG_MPP_CSI_DEV0_MCLK_NUM
#else
#define OV9286_CSI0_MCLK_ID 1
#endif
#endif
#if defined (CONFIG_MPP_ENABLE_SENSOR_OV9286_ON_CSI1_USE_CHIP_CLK)
#if defined (CONFIG_MPP_CSI_DEV1_MCLK_NUM) && (CONFIG_MPP_CSI_DEV1_MCLK_NUM >= 1) && (CONFIG_MPP_CSI_DEV1_MCLK_NUM <= 3)
#define OV9286_CSI1_MCLK_ID CONFIG_MPP_CSI_DEV1_MCLK_NUM
#else
#define OV9286_CSI1_MCLK_ID 1
#endif
#endif
#if defined (CONFIG_MPP_ENABLE_SENSOR_OV9286_ON_CSI2_USE_CHIP_CLK)
#if defined (CONFIG_MPP_CSI_DEV2_MCLK_NUM) && (CONFIG_MPP_CSI_DEV2_MCLK_NUM >= 1) && (CONFIG_MPP_CSI_DEV2_MCLK_NUM <= 3)
#define OV9286_CSI2_MCLK_ID CONFIG_MPP_CSI_DEV2_MCLK_NUM
#else
#define OV9286_CSI2_MCLK_ID 1
#endif
#endif

#if defined (CONFIG_MPP_ENABLE_CSI_DEV_0)
#include "sensor_csi0_mode_list.c"
#endif
#if defined (CONFIG_MPP_ENABLE_CSI_DEV_1)
#include "sensor_csi1_mode_list.c"
#endif
#if defined (CONFIG_MPP_ENABLE_CSI_DEV_2)
#include "sensor_csi2_mode_list.c"
#endif

static k_s32 _sensor_read_chip_id(struct sensor_driver_dev *dev, k_u32 *chip_id)
{
    k_s32 ret_h;
    k_s32 ret_l;
    k_u16 id_high = 0;
    k_u16 id_low = 0;

    if (NULL == dev->i2c_info.i2c_bus)
        return -1;

    if (0 <= dev->reset_gpio) {
        kd_pin_mode(dev->reset_gpio, GPIO_DM_OUTPUT);
        kd_pin_write(dev->reset_gpio, GPIO_PV_HIGH);
    }
    rt_thread_mdelay(1);

    ret_h = sensor_reg_read(&dev->i2c_info, OV9286_REG_CHIP_ID_H, &id_high);
    ret_l = sensor_reg_read(&dev->i2c_info, OV9286_REG_CHIP_ID_L, &id_low);
    if (ret_h || ret_l)
        return -1;

    if (chip_id)
        *chip_id = ((k_u32)id_high << 8) | id_low;

    return 0;
}

static void ov9286_xshut_set(k_s32 on)
{
    kd_pin_mode(OV9286_IR_XSHUT_GPIO, GPIO_DM_OUTPUT);
    kd_pin_mode(OV9286_RGB_XSHUT_GPIO, GPIO_DM_OUTPUT);
    kd_pin_mode(OV9286_PS_SHUT_GPIO, GPIO_DM_OUTPUT);
    /* Module drives PS_INT. Leave the SoC pin as an input. */
    kd_pin_mode(OV9286_PS_INT_GPIO, GPIO_DM_INPUT);
    if (on) {
        kd_pin_write(OV9286_IR_XSHUT_GPIO, GPIO_PV_HIGH);
        kd_pin_write(OV9286_RGB_XSHUT_GPIO, GPIO_PV_HIGH);
        kd_pin_write(OV9286_PS_SHUT_GPIO, GPIO_PV_HIGH);
    } else {
        kd_pin_write(OV9286_IR_XSHUT_GPIO, GPIO_PV_LOW);
        kd_pin_write(OV9286_PS_SHUT_GPIO, GPIO_PV_LOW);
    }
}

static int _sensor_power_state_set(struct sensor_driver_dev *dev, k_s32 on, k_u32 delay)
{
    const k_s32 pwd_gpio = dev->pwd_gpio;
    const k_s32 reset_gpio = dev->reset_gpio;

    ov9286_xshut_set(on);

    if (0 > reset_gpio)
        return 0;

    if (0 <= pwd_gpio) {
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
        rt_thread_mdelay(OV9286_POWER_STABLE_DELAY_MS);
    } else {
        kd_pin_write(reset_gpio, GPIO_PV_LOW);
    }

    return 0;
}

static k_s32 ov9286_write_again(struct sensor_driver_dev *dev, k_u16 again)
{
    return sensor_reg_write(&dev->i2c_info, OV9286_REG_AGAIN, again & 0xff);
}

static k_s32 ov9286_write_exp_line(struct sensor_driver_dev *dev, k_u16 exp_line)
{
    k_s32 ret;

    /* 0x3501 is lines[11:4], 0x3502[7:4] is lines[3:0]. */
    ret = sensor_reg_write(&dev->i2c_info, OV9286_REG_EXP_H, (exp_line >> 4) & 0xff);
    ret |= sensor_reg_write(&dev->i2c_info, OV9286_REG_EXP_L, (exp_line << 4) & 0xf0);
    return ret;
}

static k_s32 sensor_power_impl(void *ctx, k_s32 on)
{
    k_s32 ret = 0;
    struct sensor_driver_dev *dev = ctx;

    if (on) {
        if (dev->power_flag) {
            dev->init_flag = K_TRUE;
            return 0;
        }

        ret = _sensor_power_state_set(dev, K_TRUE, OV9286_POWER_DELAY_MS);
        if (!ret) {
            dev->power_flag = K_TRUE;
            dev->init_flag = K_TRUE;
        }
        return ret;
    }

    if (dev->power_flag)
        ret |= _sensor_power_state_set(dev, K_FALSE, OV9286_POWER_DELAY_MS);

    dev->power_flag = K_FALSE;
    dev->init_flag = K_FALSE;
    dev->mode_init_flag = K_FALSE;
    return ret;
}

static k_s32 sensor_init_impl(void *ctx, k_sensor_mode mode)
{
    k_s32 ret = 0;
    struct sensor_driver_dev *dev = ctx;
    k_sensor_mode *current_mode = &dev->current_sensor_mode;
    k_vicap_sensor_type type = mode.sensor_type;

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

    /* Soft reset often NACKs while the sensor restarts. */
    (void)sensor_reg_write(&dev->i2c_info, 0x0103, 0x01);
    rt_thread_mdelay(OV9286_SOFT_RESET_DELAY_MS);

    ret = sensor_reg_list_write(&dev->i2c_info, current_mode->reg_list);
    if (ret) {
        pr_err("%s, write reg list failed\n", __func__);
        return -1;
    }
    rt_thread_mdelay(OV9286_SOFT_RESET_DELAY_MS);
    ret = sensor_reg_write(&dev->i2c_info, 0x0100, 0x00);

    current_mode->sensor_again = 0;
    current_mode->et_line = 0;

    {
        k_u16 again_reg = 0;
        k_u16 exp_h = 0, exp_l = 0;
        k_u16 again = 0;
        k_u16 exp_line = 0;

        if (sensor_reg_read(&dev->i2c_info, OV9286_REG_AGAIN, &again_reg) == 0) {
            again = again_reg & 0xff;
            if (again < 0x10)
                again = 0x10;
        } else {
            again = 0x10;
        }
        current_mode->sensor_again = again;
        current_mode->ae_info.cur_again = (float)again / 16.0f;
        current_mode->ae_info.cur_dgain = 1.0f;
        current_mode->ae_info.cur_gain = current_mode->ae_info.cur_again;
        current_mode->ae_info.cur_long_gain = current_mode->ae_info.cur_gain;
        current_mode->ae_info.cur_vs_gain = current_mode->ae_info.cur_gain;

        if (sensor_reg_read(&dev->i2c_info, OV9286_REG_EXP_H, &exp_h) == 0 &&
            sensor_reg_read(&dev->i2c_info, OV9286_REG_EXP_L, &exp_l) == 0) {
            exp_line = (k_u16)((exp_h << 4) | ((exp_l >> 4) & 0x0f));
            if (exp_line < 1)
                exp_line = 1;
        } else {
            exp_line = 1;
        }
        current_mode->et_line = exp_line;
        current_mode->ae_info.cur_integration_time =
            (float)exp_line * current_mode->ae_info.one_line_exp_time;
    }

    dev->init_flag = K_TRUE;
    dev->mode_init_flag = K_TRUE;
    return ret;
}

static k_s32 sensor_get_chip_id_impl(void *ctx, k_u32 *chip_id)
{
    struct sensor_driver_dev *dev = ctx;
    k_s32 ret = _sensor_read_chip_id(dev, chip_id);

    if (ret || (chip_id && (OV9286_CHIP_ID != *chip_id))) {
        pr_err("%s, iic read chip id err\n", __func__);
        return -1;
    }
    return 0;
}

static k_s32 sensor_get_mode_impl(void *ctx, k_sensor_mode *mode)
{
    struct sensor_driver_dev *dev = ctx;
    k_vicap_sensor_type type = mode->sensor_type;
    k_sensor_mode *current_mode = &dev->current_sensor_mode;

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

    pr_err("%s, the mode not exist.\n", __func__);
    return -1;
}

static k_s32 sensor_set_mode_impl(void *ctx, k_sensor_mode mode)
{
    (void)ctx;
    (void)mode;
    return 0;
}

static k_s32 sensor_enum_mode_impl(void *ctx, k_sensor_enum_mode *enum_mode)
{
    struct sensor_driver_dev *dev = ctx;

    if (enum_mode->index >= dev->mode_count)
        return -1;

    memcpy(&enum_mode->mode, &dev->sensor_mode_list[enum_mode->index], sizeof(k_sensor_mode));
    if (enum_mode->mode.sensor_ae_info)
        memcpy(&enum_mode->mode.ae_info, enum_mode->mode.sensor_ae_info, sizeof(k_sensor_ae_info));
    return 0;
}

static k_s32 sensor_get_caps_impl(void *ctx, k_sensor_caps *caps)
{
    struct sensor_driver_dev *dev = ctx;
    k_sensor_mode *current_mode = &dev->current_sensor_mode;

    memset(caps, 0, sizeof(k_sensor_caps));
    caps->bit_width = current_mode->bit_width;
    caps->bayer_pattern = current_mode->bayer_pattern;
    caps->resolution.width = current_mode->size.width;
    caps->resolution.height = current_mode->size.height;
    return 0;
}

static k_s32 sensor_conn_check_impl(void *ctx, k_s32 *conn)
{
    (void)ctx;
    *conn = 1;
    return 0;
}

static k_s32 sensor_set_stream_impl(void *ctx, k_s32 enable)
{
    struct sensor_driver_dev *dev = ctx;

    if (enable)
        return sensor_reg_write(&dev->i2c_info, 0x0100, 0x01);

    return sensor_reg_write(&dev->i2c_info, 0x0100, 0x00);
}

static k_s32 sensor_get_again_impl(void *ctx, k_sensor_gain *gain)
{
    struct sensor_driver_dev *dev = ctx;
    k_sensor_mode *current_mode = &dev->current_sensor_mode;

    if (current_mode->hdr_mode != SENSOR_MODE_LINEAR)
        return -1;

    gain->gain[SENSOR_LINEAR_PARAS] = current_mode->ae_info.cur_again;
    return 0;
}

static k_s32 sensor_set_again_impl(void *ctx, k_sensor_gain gain)
{
    k_s32 ret = 0;
    k_u16 again;
    struct sensor_driver_dev *dev = ctx;
    k_sensor_mode *current_mode = &dev->current_sensor_mode;

    if (current_mode->hdr_mode != SENSOR_MODE_LINEAR)
        return -1;

    again = (k_u16)(gain.gain[SENSOR_LINEAR_PARAS] * 16.0f + 0.5f);
    if (again < 0x10)
        again = 0x10;
    if (again > OV9286_AGAIN_REG_MAX)
        again = OV9286_AGAIN_REG_MAX;

    if (current_mode->sensor_again != again) {
        ret = ov9286_write_again(dev, again);
        if (ret)
            return ret;
        current_mode->sensor_again = again;
    }
    current_mode->ae_info.cur_again = (float)current_mode->sensor_again / 16.0f;
    return 0;
}

static k_s32 sensor_get_dgain_impl(void *ctx, k_sensor_gain *gain)
{
    struct sensor_driver_dev *dev = ctx;
    k_sensor_mode *current_mode = &dev->current_sensor_mode;

    if (current_mode->hdr_mode != SENSOR_MODE_LINEAR)
        return -1;

    gain->gain[SENSOR_LINEAR_PARAS] = current_mode->ae_info.cur_dgain;
    return 0;
}

static k_s32 sensor_set_dgain_impl(void *ctx, k_sensor_gain gain)
{
    k_u32 dgain;
    struct sensor_driver_dev *dev = ctx;
    k_sensor_mode *current_mode = &dev->current_sensor_mode;

    if (current_mode->hdr_mode != SENSOR_MODE_LINEAR)
        return -1;

    /* Linear path keeps digital gain at 1x; remember the request. */
    dgain = (k_u32)(gain.gain[SENSOR_LINEAR_PARAS] * 1024.0f);
    if (dgain < 1024)
        dgain = 1024;
    current_mode->ae_info.cur_dgain = (float)dgain / 1024.0f;
    current_mode->ae_info.cur_gain = current_mode->ae_info.cur_again * current_mode->ae_info.cur_dgain;
    return 0;
}

static k_s32 sensor_get_intg_time_impl(void *ctx, k_sensor_intg_time *time)
{
    struct sensor_driver_dev *dev = ctx;
    k_sensor_mode *current_mode = &dev->current_sensor_mode;

    if (current_mode->hdr_mode != SENSOR_MODE_LINEAR)
        return -1;

    time->intg_time[SENSOR_LINEAR_PARAS] = current_mode->ae_info.cur_integration_time;
    return 0;
}

static k_s32 sensor_set_intg_time_impl(void *ctx, k_sensor_intg_time time)
{
    k_s32 ret = 0;
    k_u16 exp_line;
    float integraion_time;
    struct sensor_driver_dev *dev = ctx;
    k_sensor_mode *current_mode = &dev->current_sensor_mode;

    if (current_mode->hdr_mode != SENSOR_MODE_LINEAR)
        return -1;
    if (current_mode->ae_info.one_line_exp_time <= 0.0f)
        return -1;

    integraion_time = time.intg_time[SENSOR_LINEAR_PARAS];
    exp_line = (k_u16)(integraion_time / current_mode->ae_info.one_line_exp_time);
    exp_line = MIN(current_mode->ae_info.max_integraion_line,
                   MAX(current_mode->ae_info.min_integraion_line, exp_line));

    if (current_mode->et_line != exp_line) {
        ret = ov9286_write_exp_line(dev, exp_line);
        if (ret)
            return ret;
        current_mode->et_line = exp_line;
    }
    current_mode->ae_info.cur_integration_time =
        (float)current_mode->et_line * current_mode->ae_info.one_line_exp_time;
    return 0;
}

static k_s32 sensor_get_exp_parm_impl(void *ctx, k_sensor_exposure_param *exp_parm)
{
    (void)ctx;
    memset(exp_parm, 0, sizeof(k_sensor_exposure_param));
    return 0;
}

static k_s32 sensor_set_exp_parm_impl(void *ctx, k_sensor_exposure_param exp_parm)
{
    (void)ctx;
    (void)exp_parm;
    return 0;
}

static k_s32 sensor_get_fps_impl(void *ctx, k_u32 *fps)
{
    struct sensor_driver_dev *dev = ctx;

    *fps = dev->current_sensor_mode.fps;
    return 0;
}

static k_s32 sensor_set_fps_impl(void *ctx, k_u32 fps)
{
    (void)ctx;
    (void)fps;
    return 0;
}

static k_s32 sensor_get_isp_status_impl(void *ctx, k_sensor_isp_status *staus)
{
    (void)ctx;
    memset(staus, 0, sizeof(k_sensor_isp_status));
    return 0;
}

static k_s32 sensor_set_blc_impl(void *ctx, k_sensor_blc blc)
{
    (void)ctx;
    (void)blc;
    return 0;
}

static k_s32 sensor_set_wb_impl(void *ctx, k_sensor_white_balance wb)
{
    (void)ctx;
    (void)wb;
    return 0;
}

static k_s32 sensor_get_tpg_impl(void *ctx, k_sensor_test_pattern *tpg)
{
    (void)ctx;
    memset(tpg, 0, sizeof(k_sensor_test_pattern));
    return 0;
}

static k_s32 sensor_set_tpg_impl(void *ctx, k_sensor_test_pattern tpg)
{
    (void)ctx;
    (void)tpg;
    return 0;
}

static k_s32 sensor_get_expand_curve_impl(void *ctx, k_sensor_compand_curve *curve)
{
    (void)ctx;
    memset(curve, 0, sizeof(k_sensor_compand_curve));
    return 0;
}

static k_s32 sensor_get_otp_data_impl(void *ctx, void *data)
{
    (void)ctx;
    memset(data, 0, sizeof(void *));
    return 0;
}

static k_s32 sensor_mirror_set_impl(void *ctx, k_vicap_mirror_mode mirror)
{
    struct sensor_driver_dev *dev = ctx;

    dev->mirror_setting = mirror;
    return 0;
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
    .sensor_get_exposure_time_range = sensor_get_exposure_time_range_common,
    .sensor_get_gain_range = sensor_get_gain_range_common,
    .sensor_ircut_set = sensor_ircut_set_common,
    .sensor_ircut_get = sensor_ircut_get_common,
};

k_s32 sensor_ov9286_probe(struct k_sensor_probe_cfg *cfg, struct sensor_driver_dev *dev)
{
    k_s32 id_ret;
    k_u32 chip_id = 0;
    const k_sensor_mode *sensor_mode = NULL;
    const k_sensor_mclk_setting *mclk;

#if defined (CONFIG_MPP_ENABLE_CSI_DEV_0)
    if (0x00 == cfg->csi_num) {
        dev->mode_count = sizeof(sensor_csi0_mode_list) / sizeof(sensor_csi0_mode_list[0]);
        dev->sensor_mode_list = &sensor_csi0_mode_list[0];
        sensor_mode = &dev->sensor_mode_list[0];
    } else
#endif
#if defined (CONFIG_MPP_ENABLE_CSI_DEV_1)
    if (0x01 == cfg->csi_num) {
        dev->mode_count = sizeof(sensor_csi1_mode_list) / sizeof(sensor_csi1_mode_list[0]);
        dev->sensor_mode_list = &sensor_csi1_mode_list[0];
        sensor_mode = &dev->sensor_mode_list[0];
    } else
#endif
#if defined (CONFIG_MPP_ENABLE_CSI_DEV_2)
    if (0x02 == cfg->csi_num) {
        dev->mode_count = sizeof(sensor_csi2_mode_list) / sizeof(sensor_csi2_mode_list[0]);
        dev->sensor_mode_list = &sensor_csi2_mode_list[0];
        sensor_mode = &dev->sensor_mode_list[0];
    }
#endif

    if (0x00 == dev->mode_count || NULL == sensor_mode)
        goto _on_failed;

    mclk = &sensor_mode->mclk_setting[0];

    dev->pwd_gpio = cfg->pwd_gpio;
    dev->reset_gpio = cfg->reset_gpio;

    if (NULL == (dev->i2c_info.i2c_bus = rt_i2c_bus_device_find(cfg->i2c_name)))
        goto _on_failed;
    strncpy(&dev->i2c_info.i2c_name[0], cfg->i2c_name, sizeof(dev->i2c_info.i2c_name));
    dev->i2c_info.i2c_name[sizeof(dev->i2c_info.i2c_name) - 1] = '\0';
    memcpy(&dev->sensor_func, &sensor_functions, sizeof(k_sensor_function));

    if (sensor_set_mclk(mclk) != K_SUCCESS)
        goto _on_failed;

    snprintf(dev->sensor_name, sizeof(dev->sensor_name), "ov9286_csi%d", cfg->csi_num);

    _sensor_power_state_set(dev, 1, 1);
    dev->power_flag = K_TRUE;
    dev->init_flag = K_TRUE;

    dev->i2c_info.reg_addr_size = SENSOR_REG_VALUE_16BIT;
    dev->i2c_info.reg_val_size = SENSOR_REG_VALUE_8BIT;
    dev->i2c_info.slave_addr = OV9286_I2C_ADDR;

    id_ret = _sensor_read_chip_id(dev, &chip_id);
    if (id_ret || (OV9286_CHIP_ID != chip_id)) {
        (void)sensor_probe_power_off(dev, mclk);
        goto _on_failed;
    }

    sensor_autofocus_dev_probe(dev);

    if (sensor_probe_power_off(dev, mclk) != K_SUCCESS)
        goto _on_failed;

    return 0;

_on_failed:
    memset(dev, 0, sizeof(*dev));
    return -1;
}
