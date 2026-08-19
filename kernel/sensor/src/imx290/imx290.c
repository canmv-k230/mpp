/* Copyright (c) 2023, Canaan Bright Sight Co., Ltd
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * IMX290 sensor driver (MIPI CSI-2, 2-lane, 1920x1080 @ 30fps RAW12).
 * Register tables from Sony imx290_linear_1080p30_init; VMAX/HMAX per imx290_cmos.c.
 */

#include "rtthread.h"

#if defined(CONFIG_MPP_ENABLE_SENSOR_IMX290)

#include "sensor_dev.h"
#include "io.h"
#include "drv_gpio.h"

#include <stdio.h>
#include <math.h>

#define pr_info(...)  /* rt_kprintf(__VA_ARGS__) */
#define pr_debug(...) /* rt_kprintf(__VA_ARGS__) */
#define pr_warn(...)
#define pr_err(...) rt_kprintf(__VA_ARGS__)

#define IMX290_VMAX_FR 1126u
/* One line period: HMAX / (74.25 MHz), matches 1080p30 with VMAX=1126 (Sony / datasheet timing). */
#define IMX290_OLET (0.000029603f)
#define IMX290_AGAIN_STEP (1.0f / 256.0f)
#define IMX290_GAIN_REG_MAX 240u /* 0.3 dB/step, 72 dB analog max */
#define IMX290_REG_STANDBY 0x3000
#define IMX290_REG_HOLD 0x3001
#define IMX290_REG_XMSTA 0x3002
#define IMX290_REG_GAIN 0x3014
#define IMX290_REG_XVS_HS_SEL 0x304b
#define IMX290_REG_VMAX_L 0x3018
#define IMX290_REG_HMAX_L 0x301c
#define IMX290_REG_SHS1_L 0x3020
#define IMX290_REG_SOFTWARE_RESET 0x3003
/* Datasheet / register map: 3008h [7:0] "Fixed to A0h" (Chip ID bank 02h, addr 08h). */
#define IMX290_REG_ID   0x3008
#define IMX290_CHIP_ID  0xa0
/* Secondary fixed signature: 3004h "Fixed to 10h". */
#define IMX290_REG_FIXED_10 0x3004
#define IMX290_FIXED_10     0x10

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

/* GAIN 0x3014 is 0.3 dB/step. ISP AE uses linear ratio: 10^((reg*0.3)/20). */
static float imx290_reg_to_again(k_u16 reg)
{
    return powf(10.0f, (float)reg * 0.015f);
}

static k_u16 imx290_again_to_reg(float again)
{
    k_u16 reg;

    if (again < 1.0f) {
        again = 1.0f;
    }
    reg = (k_u16)(log10f(again) * 200.0f / 3.0f + 0.5f);
    if (reg > IMX290_GAIN_REG_MAX) {
        reg = IMX290_GAIN_REG_MAX;
    }
    return reg;
}

static k_s32 _sensor_read_chip_id_r(struct sensor_driver_dev *dev, k_u16 *chip_id)
{
    k_u16 id = 0;
    k_u16 fixed10 = 0;

    if (NULL == dev->i2c_info.i2c_bus) {
        return -1;
    }

    /* Do not reconfigure reset GPIO here: that glitches XCLR while streaming
     * and makes a later G_ID ioctl return 0 (I2C NACK, buf unchanged). */
    if (0x00 != sensor_reg_read(&dev->i2c_info, IMX290_REG_ID, &id)) {
        return -1;
    }
    if (0x00 != sensor_reg_read(&dev->i2c_info, IMX290_REG_FIXED_10, &fixed10)) {
        return -1;
    }
    if ((id != IMX290_CHIP_ID) || (fixed10 != IMX290_FIXED_10)) {
        return -1;
    }

    if (chip_id) {
        *chip_id = id;
    }
    return 0;
}

static int _sensor_power_state_set(struct sensor_driver_dev *dev, k_s32 on, k_u32 delay)
{
    const k_s32 pwd_gpio = dev->pwd_gpio;
    const k_s32 reset_gpio = dev->reset_gpio;

    if (0x00 > reset_gpio) {
        return 0;
    }

    if (-1 != pwd_gpio) {
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
        rt_thread_mdelay(20);
        /* Software reset only while reset pin is released (I2C valid). */
        sensor_reg_write(&dev->i2c_info, IMX290_REG_SOFTWARE_RESET, 0x01);
        rt_thread_mdelay(10);
    } else {
        kd_pin_write(reset_gpio, GPIO_PV_LOW);
        rt_thread_mdelay(20);
    }

    return 0;
}

static k_s32 sensor_power_impl(void *ctx, k_s32 on)
{
    struct sensor_driver_dev *dev = ctx;
    _sensor_power_state_set(dev, on, 100);
    dev->init_flag = on;
    return 0;
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
    
    ret = sensor_reg_list_write(&dev->i2c_info, current_mode->reg_list);

    current_mode->sensor_again = 0;
    current_mode->et_line = 0;

    k_u16 g = 0;
    ret = sensor_reg_read(&dev->i2c_info, IMX290_REG_GAIN, &g);
    current_mode->sensor_again = g;
    current_mode->ae_info.cur_again = imx290_reg_to_again(g);
    current_mode->ae_info.cur_dgain = 1.0f;
    current_mode->ae_info.cur_gain = current_mode->ae_info.cur_again * current_mode->ae_info.cur_dgain;
    current_mode->ae_info.cur_long_again = current_mode->ae_info.cur_again;
    current_mode->ae_info.cur_vs_again = current_mode->ae_info.cur_again;
    current_mode->ae_info.cur_long_gain = current_mode->ae_info.cur_gain;
    current_mode->ae_info.cur_vs_gain = current_mode->ae_info.cur_gain;

    if (current_mode->hdr_mode == SENSOR_MODE_LINEAR) {
        k_u16 shs_l = 0, shs_m = 0, shs_h = 0;
        k_u32 shs1;
        ret = sensor_reg_read(&dev->i2c_info, IMX290_REG_SHS1_L, &shs_l);
        ret |= sensor_reg_read(&dev->i2c_info, IMX290_REG_SHS1_L + 1, &shs_m);
        ret |= sensor_reg_read(&dev->i2c_info, IMX290_REG_SHS1_L + 2, &shs_h);
        shs1 = ((k_u32)shs_h << 16) | ((k_u32)shs_m << 8) | shs_l;
        k_u32 exp_line = IMX290_VMAX_FR - shs1 - 1;
        current_mode->ae_info.cur_integration_time = current_mode->ae_info.one_line_exp_time * (float)exp_line;
    }
    dev->init_flag = K_TRUE;
    return ret;
}

static k_s32 sensor_get_chip_id_impl(void *ctx, k_u32 *chip_id)
{
    k_s32 ret = 0;
    k_u16 id = 0;
    struct sensor_driver_dev *dev = ctx;

    ret = _sensor_read_chip_id_r(dev, &id);
    if (ret) {
        pr_err("%s, iic read chip id err\n", __func__);
        return ret;
    }
    if (chip_id) {
        *chip_id = id;
    }
    return 0;
}

static k_s32 sensor_get_mode_impl(void *ctx, k_sensor_mode *mode)
{
    struct sensor_driver_dev *dev = ctx;
    const k_vicap_sensor_type type = mode->sensor_type;
    k_sensor_mode *current_mode = &dev->current_sensor_mode;

    if (type == current_mode->sensor_type) {
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

static k_s32 sensor_set_mode_impl(void *ctx, k_sensor_mode mode)
{
    struct sensor_driver_dev *dev = ctx;
    (void)mode;
    (void)dev;
    return 0;
}

static k_s32 sensor_enum_mode_impl(void *ctx, k_sensor_enum_mode *enum_mode)
{
    struct sensor_driver_dev *dev = ctx;
    (void)dev;
    memset(enum_mode, 0, sizeof(k_sensor_enum_mode));
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
    k_s32 ret = 0;

    if (enable) {
        ret = sensor_reg_write(&dev->i2c_info, IMX290_REG_STANDBY, 0x00);
        /* Linux imx290_start_streaming: msleep(30) between STANDBY off and XMSTA off */
        rt_thread_mdelay(30);
        ret |= sensor_reg_write(&dev->i2c_info, IMX290_REG_XMSTA, 0x00);
        ret |= sensor_reg_write(&dev->i2c_info, IMX290_REG_XVS_HS_SEL, 0x0a);
    } else {
        ret = sensor_reg_write(&dev->i2c_info, IMX290_REG_XMSTA, 0x01);
        ret |= sensor_reg_write(&dev->i2c_info, IMX290_REG_STANDBY, 0x01);
    }
    return ret;
}

static k_s32 sensor_get_again_impl(void *ctx, k_sensor_gain *gain)
{
    struct sensor_driver_dev *dev = ctx;
    k_sensor_mode *current_mode = &dev->current_sensor_mode;
    if (current_mode->hdr_mode == SENSOR_MODE_LINEAR) {
        gain->gain[SENSOR_LINEAR_PARAS] = current_mode->ae_info.cur_again;
    } else {
        return -1;
    }
    return 0;
}

static k_s32 sensor_set_again_impl(void *ctx, k_sensor_gain gain)
{
    struct sensor_driver_dev *dev = ctx;
    k_sensor_mode *current_mode = &dev->current_sensor_mode;
    k_s32 ret = 0;

    if (current_mode->hdr_mode != SENSOR_MODE_LINEAR) {
        return -1;
    }

    k_u16 again = imx290_again_to_reg(gain.gain[SENSOR_LINEAR_PARAS]);
    if (current_mode->sensor_again != again) {
        ret = sensor_reg_write(&dev->i2c_info, IMX290_REG_GAIN, again & 0xff);
        current_mode->sensor_again = again;
        current_mode->ae_info.cur_again = imx290_reg_to_again(again);
        current_mode->ae_info.cur_long_again = current_mode->ae_info.cur_again;
        current_mode->ae_info.cur_vs_again = current_mode->ae_info.cur_again;
        current_mode->ae_info.cur_gain = current_mode->ae_info.cur_again * current_mode->ae_info.cur_dgain;
        current_mode->ae_info.cur_long_gain = current_mode->ae_info.cur_gain;
        current_mode->ae_info.cur_vs_gain = current_mode->ae_info.cur_gain;
    }
    return ret;
}

static k_s32 sensor_get_dgain_impl(void *ctx, k_sensor_gain *gain)
{
    struct sensor_driver_dev *dev = ctx;
    k_sensor_mode *current_mode = &dev->current_sensor_mode;
    if (current_mode->hdr_mode == SENSOR_MODE_LINEAR) {
        gain->gain[SENSOR_LINEAR_PARAS] = current_mode->ae_info.cur_dgain;
    } else {
        return -1;
    }
    return 0;
}

static k_s32 sensor_set_dgain_impl(void *ctx, k_sensor_gain gain)
{
    struct sensor_driver_dev *dev = ctx;
    k_sensor_mode *current_mode = &dev->current_sensor_mode;
    k_u32 dgain;

    if (current_mode->hdr_mode != SENSOR_MODE_LINEAR) {
        return -1;
    }
    dgain = (k_u32)(gain.gain[SENSOR_LINEAR_PARAS] * 1024);
    current_mode->ae_info.cur_dgain = dgain / 1024.0f;
    current_mode->ae_info.cur_gain = current_mode->ae_info.cur_again * current_mode->ae_info.cur_dgain;
    current_mode->ae_info.cur_long_gain = current_mode->ae_info.cur_gain;
    current_mode->ae_info.cur_vs_gain = current_mode->ae_info.cur_gain;
    return 0;
}

static k_s32 sensor_get_intg_time_impl(void *ctx, k_sensor_intg_time *time)
{
    struct sensor_driver_dev *dev = ctx;
    k_sensor_mode *current_mode = &dev->current_sensor_mode;
    if (current_mode->hdr_mode == SENSOR_MODE_LINEAR) {
        time->intg_time[SENSOR_LINEAR_PARAS] = current_mode->ae_info.cur_integration_time;
    } else {
        return -1;
    }
    return 0;
}

static k_s32 sensor_set_intg_time_impl(void *ctx, k_sensor_intg_time time)
{
    struct sensor_driver_dev *dev = ctx;
    k_sensor_mode *current_mode = &dev->current_sensor_mode;
    k_s32 ret = 0;
    float integraion_time = time.intg_time[SENSOR_LINEAR_PARAS];
    k_u16 exp_line = (k_u16)(integraion_time / current_mode->ae_info.one_line_exp_time);

    exp_line = (k_u16)MIN((k_u32)current_mode->ae_info.max_integraion_line,
                          MAX((k_u32)current_mode->ae_info.min_integraion_line, (k_u32)exp_line));

    if (current_mode->hdr_mode == SENSOR_MODE_LINEAR) {
        if (current_mode->et_line != exp_line) {
            k_u32 shs1 = (k_u32)IMX290_VMAX_FR - (k_u32)exp_line - 1u;
            ret = sensor_reg_write(&dev->i2c_info, IMX290_REG_HOLD, 0x01);
            ret |= sensor_reg_write(&dev->i2c_info, IMX290_REG_SHS1_L, (k_u16)(shs1 & 0xff));
            ret |= sensor_reg_write(&dev->i2c_info, IMX290_REG_SHS1_L + 1, (k_u16)((shs1 >> 8) & 0xff));
            ret |= sensor_reg_write(&dev->i2c_info, IMX290_REG_SHS1_L + 2, (k_u16)((shs1 >> 16) & 0xff));
            ret |= sensor_reg_write(&dev->i2c_info, IMX290_REG_HOLD, 0x00);
            current_mode->et_line = exp_line;
            current_mode->ae_info.cur_integration_time =
                (float)current_mode->et_line * current_mode->ae_info.one_line_exp_time;
        }
    } else {
        return -1;
    }
    return ret;
}

static k_s32 sensor_get_exp_parm_impl(void *ctx, k_sensor_exposure_param *exp_parm)
{
    struct sensor_driver_dev *dev = ctx;
    (void)dev;
    memset(exp_parm, 0, sizeof(k_sensor_exposure_param));
    return 0;
}

static k_s32 sensor_set_exp_parm_impl(void *ctx, k_sensor_exposure_param exp_parm)
{
    struct sensor_driver_dev *dev = ctx;
    (void)dev;
    (void)exp_parm;
    return 0;
}

static k_s32 sensor_get_fps_impl(void *ctx, k_u32 *fps)
{
    (void)ctx;
    *fps = 30000;
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
};

k_s32 sensor_imx290_probe(struct k_sensor_probe_cfg *cfg, struct sensor_driver_dev *dev)
{
    k_u16 chip_id = 0;
    const k_sensor_mode *sensor_mode = NULL;

#if defined(CONFIG_MPP_ENABLE_CSI_DEV_0)
    if (0x00 == cfg->csi_num) {
        dev->mode_count = sizeof(sensor_csi0_mode_list) / sizeof(sensor_csi0_mode_list[0]);
        dev->sensor_mode_list = &sensor_csi0_mode_list[0];
        sensor_mode = &dev->sensor_mode_list[0];
    } else
#endif
#if defined(CONFIG_MPP_ENABLE_CSI_DEV_1)
    if (0x01 == cfg->csi_num) {
        dev->mode_count = sizeof(sensor_csi1_mode_list) / sizeof(sensor_csi1_mode_list[0]);
        dev->sensor_mode_list = &sensor_csi1_mode_list[0];
        sensor_mode = &dev->sensor_mode_list[0];
    } else
#endif
#if defined(CONFIG_MPP_ENABLE_CSI_DEV_2)
    if (0x02 == cfg->csi_num) {
        dev->mode_count = sizeof(sensor_csi2_mode_list) / sizeof(sensor_csi2_mode_list[0]);
        dev->sensor_mode_list = &sensor_csi2_mode_list[0];
        sensor_mode = &dev->sensor_mode_list[0];
    }
#endif

    if (0x00 == dev->mode_count || NULL == sensor_mode) {
        goto _on_failed;
    }

    dev->pwd_gpio = cfg->pwd_gpio;
    dev->reset_gpio = cfg->reset_gpio;

    if (NULL == (dev->i2c_info.i2c_bus = rt_i2c_bus_device_find(cfg->i2c_name))) {
        rt_kprintf("Can't find %s\n", cfg->i2c_name);
        goto _on_failed;
    }
    strncpy(&dev->i2c_info.i2c_name[0], cfg->i2c_name, sizeof(dev->i2c_info.i2c_name));
    memcpy(&dev->sensor_func, &sensor_functions, sizeof(k_sensor_function));

    sensor_set_mclk(&sensor_mode->mclk_setting[0]);

    snprintf(dev->sensor_name, sizeof(dev->sensor_name), "imx290_csi%d", cfg->csi_num);

    /* I2C addressing must be valid before any sensor_reg_* in power/reset path. */
    dev->i2c_info.reg_addr_size = SENSOR_REG_VALUE_16BIT;
    dev->i2c_info.reg_val_size = SENSOR_REG_VALUE_8BIT;
    dev->i2c_info.slave_addr = 0x1a; /* Sony imx290_i2c_addr 0x34 (8-bit) -> 7-bit 0x1A */

    _sensor_power_state_set(dev, 1, 1);

    if (0x00 != _sensor_read_chip_id_r(dev, &chip_id)) {
        goto _on_failed;
    }

    sensor_autofocus_dev_probe(dev);
    return 0;

_on_failed:
    memset(dev, 0, sizeof(*dev));
    return -1;
}

#endif /* CONFIG_MPP_ENABLE_SENSOR_IMX290 */
